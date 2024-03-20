/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This is the real heart of the PHP agent. These hook functions are what tie
 * most of the rest of the agent and Axiom together. They are also one of the
 * most performance-critical parts of the agent as they affect the actual
 * running speed of each PHP VM (whereas a lot of other work is done after the
 * VM is done and does not affect user-perceived speed). Therefore, absolutely
 * anything that can be done to make things quicker, should be, (almost, but
 * not entirely) to the exclusion of all else.
 */
#include "php_agent.h"
#include "php_curl.h"
#include "php_error.h"
#include "php_execute.h"
#include "php_globals.h"
#include "php_hash.h"
#include "php_hooks.h"
#include "php_internal_instrument.h"
#include "php_stacked_segment.h"
#include "php_user_instrument.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_number_converter.h"
#include "util_strings.h"
#include "util_url.h"
#include "util_url.h"
#include "util_metrics.h"
#include "util_number_converter.h"
#include "fw_support.h"
#include "fw_hooks.h"
#include "php_observer.h"

/*
 * This wall of text is important. Read it. Understand it. Really.
 *
 * These execute hooks are the single most critical performance path of the
 * agent. As history has shown us, even slight improvements here have a visible
 * effect on the overall agent overhead, especially when measured over a long
 * time. EXTREME care must be taken when modifying anything in this file.
 *
 * Aside from raw performance another critical aspect is resource consumption.
 * Of those resources, the most important is stack space. Since PHP functions
 * are often called recursively, bear in mind that any stack space you use in
 * these functions is amplified by each level of recursion. Trimming out stack
 * usage is much harder than it appears at first glance. Consider this code:
 *
 *   int nr_php_execute (...)
 *   {
 *     char tmpbuf[2048];
 *
 *     some_stuff ();
 *
 *     if (condition) {
 *       char otherbuf[2048];
 *     }
 *
 *     ...
 *   }
 *
 * At first glance you may think that the normal course of evens will only use
 * 2K of stack space, and that the 2K inside the condition will be allocated
 * when needed. This is not true. The compiler will allocate 4K (plus other
 * space for other automatic variables) on entry. The lesson here is that the
 * compiler allocates the space for *ALL* automatic variables on function
 * entry. This gets really expensive, really quickly.
 *
 * We used to obsess about not calling functions, citing the cost of function
 * constructions and teardown as reasons to avoid excessive function calls.
 * This too is erroneous. The cost of calling a function is about 4 assembler
 * instructions. This is negligible. Therefore, as a means of reducing stack
 * usage, if you need stack space it is better to put that usage into a static
 * function and call it from the main function, because then that stack space is
 * genuinely only allocated when needed.
 *
 * A not-insignificant performance boost comes from accurate branch hinting
 * using the nrlikely() and nrunlikely() macros. This prevents pipeline stalls
 * in the case of a branch not taken (or taken, depending on the logic). Using
 * branch hints allows the CPU fetch/decode engine to fetch the appropriate
 * instruction for the normal case.
 *
 * Try to avoid using the heap for micro-blocks. If you find you need memory
 * from the heap for some new feature, consider allocating more space than you
 * need for the feature *outside* of this code and simply consuming that space
 * here.
 *
 * Try to order conditionals such that the most likely clause to affect the
 * boolean short-circuiting is first. In other words, put conditions that are
 * most likely to be true at the end of a set of conditional clauses. Try to
 * make conditionals as simple as possible and avoid lengthy multi-clause
 * conditions wherever possible.
 */

static void nr_php_show_exec_return(NR_EXECUTE_PROTO TSRMLS_DC);
static int nr_php_show_exec_indentation(TSRMLS_D);
static void nr_php_show_exec(NR_EXECUTE_PROTO TSRMLS_DC);

/*
 * Purpose: Enable monitoring on specific functions in the framework.
 */
typedef void (*nr_framework_enable_fn_t)(TSRMLS_D);

/*
 * Purpose: Enable monitoring on specific functions for a detected library.
 */
typedef void (*nr_library_enable_fn_t)(TSRMLS_D);

/*
 * Purpose: Enable monitoring on specific functions for a detected vulnerability
 *          management package.
 */
typedef void (*nr_vuln_mgmt_enable_fn_t)();

/*
 * This code is used for function call debugging.
 */
#define MAX_NR_EXECUTE_DEBUG_STRLEN (80)
#define NR_EXECUTE_DEBUG_STRBUFSZ (16384)

#define safe_append(S, L)                     \
  if (avail > (size_t)(L)) {                  \
    nr_strxcpy(pbuf + pos, (S), (size_t)(L)); \
    pos += (size_t)(L);                       \
    avail -= (size_t)(L);                     \
  } else if (avail > 3) {                     \
    nr_strxcpy(pbuf + pos, "...", 3);         \
    pos = pos + 3;                            \
    avail = avail - 3;                        \
  }

int nr_format_zval_for_debug(zval* arg,
                             char* pbuf,
                             size_t pos,
                             size_t avail,
                             size_t depth TSRMLS_DC) {
  nr_string_len_t len;
  nr_string_len_t i;
  char* str;
  char tmp[128];
  zend_class_entry* ce;
  size_t orig_avail = avail;

  (void)depth; /* only useful when recursing to print out arrays */

  nr_php_zval_unwrap(arg);

  switch (Z_TYPE_P(arg)) {
    case IS_NULL:
      safe_append("null", 4);
      break;

    case IS_STRING:
      if (avail < 2) {
        break;
      }

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP7+ */
      if (NULL == Z_STR_P(arg)) {
        safe_append("invalid string", 14);
        break;
      }
#endif

      str = Z_STRVAL_P(arg);
      len = Z_STRLEN_P(arg);

      if (0
          == (NR_PHP_PROCESS_GLOBALS(special_flags).show_executes_untrimmed)) {
        if (len > MAX_NR_EXECUTE_DEBUG_STRLEN) {
          len = MAX_NR_EXECUTE_DEBUG_STRLEN;
        }

        for (i = 5; i < len; i++) {
          if ('\n' == str[i]) {
            len = i - 1;
            break;
          }
        }
      }

      if (len > ((nr_string_len_t)(avail - 2))) {
        len = (nr_string_len_t)(avail - 2);
      }

      safe_append("'", 1);
      nr_strxcpy(pbuf + pos, str, len);
      pos = pos + len;
      avail = avail - len;
      if (len < Z_STRLEN_P(arg)) {
        safe_append("...'", 4);
      } else {
        safe_append("'", 1);
      }
      break;

    case IS_LONG:
      len = snprintf(tmp, sizeof(tmp) - 1, "%ld", (long)Z_LVAL_P(arg));
      safe_append(tmp, len);
      break;

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP7+ */
    case IS_TRUE:
      safe_append("true", 4);
      break;

    case IS_FALSE:
      safe_append("false", 5);
      break;
#else
    case IS_BOOL:
      if (0 == Z_BVAL_P(arg)) {
        safe_append("false", 5);
      } else {
        safe_append("true", 4);
      }
      break;
#endif /* PHP7 */

    case IS_DOUBLE:
      /*
       * Watch out: There's an assumption here that tmp is big enough to hold
       * the entire formatted number, and that len <= sizeof (tmp).
       */
      len = nr_double_to_str(tmp, sizeof(tmp) - 1, (double)Z_DVAL_P(arg));
      safe_append(tmp, len);
      break;

    case IS_OBJECT:
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP7+ */
      if (NULL == Z_OBJ_P(arg)) {
        safe_append("invalid object", 14);
        break;
      }
#endif /* PHP7 */

      ce = Z_OBJCE_P(arg);
      len = snprintf(tmp, sizeof(tmp) - 1,
                     ":%.*s:", NRSAFELEN(nr_php_class_entry_name_length(ce)),
                     nr_php_class_entry_name(ce));
      safe_append(tmp, len);
      break;

    case IS_ARRAY:
      /*
       * It is tempting to print out all of the array elements, using
       * zend_hash_foo_ex functions. But that has been a source of bugs,
       * complexity, and hasn't given us much value.
       *
       * Note that the call here to zend_hash_num_elements does not
       * change the hash table.
       */
      safe_append("[", 1);
      len = snprintf(tmp, sizeof(tmp) - 1, "<%d elements>",
                     zend_hash_num_elements(Z_ARRVAL_P(arg)));
      safe_append(tmp, len);
      safe_append("]", 1);
      break;

    default:
      len = snprintf(tmp, sizeof(tmp) - 1, "#%d", (int)Z_TYPE_P(arg));
      safe_append(tmp, len);
      break;
  }
  (void)pos;

  return orig_avail - avail;
}

static void nr_show_execute_params(NR_EXECUTE_PROTO, char* pbuf TSRMLS_DC) {
  size_t avail = NR_EXECUTE_DEBUG_STRBUFSZ - 1;
  size_t pos = 0;

  NR_UNUSED_SPECIALFN;

  pbuf[0] = 0;

  if (0 == (NR_PHP_PROCESS_GLOBALS(special_flags).show_executes_untrimmed)) {
    avail = 1023;
  }

  if (NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_params) {
    size_t arg_count
        = nr_php_get_user_func_arg_count(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    size_t i;

    /* Arguments are 1-indexed. */
    for (i = 1; i <= arg_count; i++) {
      zval* arg = nr_php_get_user_func_arg(i, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

      if (NULL == arg) {
        safe_append("MANGLED ARGUMENT ", 17);
      } else {
        size_t len
            = nr_format_zval_for_debug(arg, pbuf, pos, avail, 0 TSRMLS_CC);

        pos += len;
        avail -= len;
      }

      if (i < arg_count) {
        safe_append(", ", 2);
      }
    }
  }

  (void)avail;
  (void)pos;
}

/*
 * Framework handling, definition and callbacks.
 */
typedef struct _nr_framework_table_t {
  const char* framework_name;
  const char* config_name;
  const char* file_to_check;
  size_t file_to_check_len;
  nr_framework_special_fn_t special;
  nr_framework_enable_fn_t enable;
  nrframework_t detected;
} nr_framework_table_t;

/*
 * Note that the maximum length of framework and library names is presently 31
 * bytes due to the use of a 64 byte static buffer when constructing
 * supportability metrics.
 *
 * Note that all paths should be in lowercase.
 */
// clang-format: off
static const nr_framework_table_t all_frameworks[] = {
    /*
     * Watch out:
     *   cake1.2 and cake1.3 use a subdirectory named 'cake' (lower case)
     *   cake2.0 and on use a subdirectory named 'Cake' (upper case file name)
     */
    {"CakePHP", "cakephp", NR_PSTR("cake/libs/object.php"), nr_cakephp_special_1,
     nr_cakephp_enable_1, NR_FW_CAKEPHP},
    {"CakePHP", "cakephp", NR_PSTR("cake/core/app.php"), nr_cakephp_special_2,
     nr_cakephp_enable_2, NR_FW_CAKEPHP},

    /*
     * Watch out: frameworks or CMS' build on top of CodeIgniter might not get
     * detected uniquely, and will instead be detected as CodeIgniter, since
     * this file load occurs first, before any other files get loaded.  This is
     * specifically a problem for Expression Engine (look for expression_engine,
     * below.)
     */
    {"CodeIgniter", "codeigniter", NR_PSTR("codeigniter.php"), 0, nr_codeigniter_enable,
     NR_FW_CODEIGNITER},

    {"Drupal8", "drupal8", NR_PSTR("core/includes/bootstrap.inc"), 0, nr_drupal8_enable,
     NR_FW_DRUPAL8},
    {"Drupal", "drupal", NR_PSTR("includes/common.inc"), 0, nr_drupal_enable,
     NR_FW_DRUPAL},

    {"Joomla", "joomla", NR_PSTR("joomla/import.php"), 0, nr_joomla_enable,
     NR_FW_JOOMLA}, /* <= Joomla 1.5 */
    {"Joomla", "joomla", NR_PSTR("libraries/joomla/factory.php"), 0, nr_joomla_enable,
     NR_FW_JOOMLA}, /* >= Joomla 1.6, including 2.5 and 3.2 */

    {"Kohana", "kohana", NR_PSTR("kohana/core.php"), 0, nr_kohana_enable, NR_FW_KOHANA},
    {"Kohana", "kohana", NR_PSTR("kohana/core.php"), 0, nr_kohana_enable, NR_FW_KOHANA},

    /* See below: Zend, the legacy project of Laminas, which shares much
       of the instrumentation implementation with Laminas */
    {"Laminas3", "laminas3", NR_PSTR("laminas/mvc/application.php"), 0,
     nr_laminas3_enable, NR_FW_LAMINAS3},
    {"Laminas3", "laminas3", NR_PSTR("laminas-mvc/src/application.php"), 0,
     nr_laminas3_enable, NR_FW_LAMINAS3},

    {"Laravel", "laravel", NR_PSTR("illuminate/foundation/application.php"), 0,
     nr_laravel_enable, NR_FW_LARAVEL},
    {"Laravel", "laravel", NR_PSTR("bootstrap/compiled.php"), 0, nr_laravel_enable,
     NR_FW_LARAVEL}, /* 4.x */
    {"Laravel", "laravel", NR_PSTR("storage/framework/compiled.php"), 0,
     nr_laravel_enable, NR_FW_LARAVEL}, /* 5.0.0-14 */
    {"Laravel", "laravel", NR_PSTR("vendor/compiled.php"), 0, nr_laravel_enable,
     NR_FW_LARAVEL}, /* 5.0.15-5.0.x */
    {"Laravel", "laravel", NR_PSTR("bootstrap/cache/compiled.php"), 0, nr_laravel_enable,
     NR_FW_LARAVEL}, /* 5.1.0-x */

    {"Lumen", "lumen", NR_PSTR("lumen-framework/src/helpers.php"), 0, nr_lumen_enable,
     NR_FW_LUMEN},

    {"Magento", "magento", NR_PSTR("app/mage.php"), 0, nr_magento1_enable,
     NR_FW_MAGENTO1},
    {"Magento2", "magento2", NR_PSTR("magento/framework/registration.php"), 0,
     nr_magento2_enable, NR_FW_MAGENTO2},

    {"MediaWiki", "mediawiki", NR_PSTR("includes/webstart.php"), 0, nr_mediawiki_enable,
     NR_FW_MEDIAWIKI},

    {"Silex", "silex", NR_PSTR("silex/application.php"), 0, nr_silex_enable,
     NR_FW_SILEX},

    {"Slim", "slim", NR_PSTR("slim/slim/app.php"), 0, nr_slim_enable,
     NR_FW_SLIM}, /* 3.x */
    {"Slim", "slim", NR_PSTR("slim/slim/slim.php"), 0, nr_slim_enable,
     NR_FW_SLIM}, /* 2.x */

    {"Symfony", "symfony1", NR_PSTR("sfcontext.class.php"), 0, nr_symfony1_enable,
     NR_FW_SYMFONY1},
    {"Symfony", "symfony1", NR_PSTR("sfconfig.class.php"), 0, nr_symfony1_enable,
     NR_FW_SYMFONY1},
    {"Symfony2", "symfony2", NR_PSTR("bootstrap.php.cache"), 0, nr_symfony2_enable,
     NR_FW_SYMFONY2}, /* also Symfony 3 */
    {"Symfony2", "symfony2",
     NR_PSTR("symfony/bundle/frameworkbundle/frameworkbundle.php"), 0,
     nr_symfony2_enable, NR_FW_SYMFONY2}, /* also Symfony 3 */
    {"Symfony4", "symfony4", NR_PSTR("http-kernel/httpkernel.php"), 0,
     nr_symfony4_enable, NR_FW_SYMFONY4}, /* also Symfony 5 */

    {"WordPress", "wordpress", NR_PSTR("wp-config.php"), 0, nr_wordpress_enable,
     NR_FW_WORDPRESS},

    {"Yii", "yii", NR_PSTR("framework/yii.php"), 0, nr_yii_enable, NR_FW_YII},
    {"Yii", "yii", NR_PSTR("framework/yiilite.php"), 0, nr_yii_enable, NR_FW_YII},

    /* See above: Laminas, the successor to Zend, which shares much
       of the instrumentation implementation with Zend */
    {"Zend", "zend", NR_PSTR("zend/loader.php"), 0, nr_zend_enable, NR_FW_ZEND},
    {"Zend2", "zend2", NR_PSTR("zend/mvc/application.php"), 0, nr_fw_zend2_enable,
     NR_FW_ZEND2},
    {"Zend2", "zend2", NR_PSTR("zend-mvc/src/application.php"), 0, nr_fw_zend2_enable,
     NR_FW_ZEND2},
};
// clang-format: on
static const int num_all_frameworks
    = sizeof(all_frameworks) / sizeof(nr_framework_table_t);

nrframework_t nr_php_framework_from_config(const char* config_name) {
  int i;

  if (0 == nr_stricmp("none", config_name)) {
    return NR_FW_NONE;
  }
  if (0 == nr_stricmp("no_framework", config_name)) {
    return NR_FW_NONE;
  }

  for (i = 0; i < num_all_frameworks; i++) {
    if (all_frameworks[i].config_name) {
      if (0 == nr_stricmp(all_frameworks[i].config_name, config_name)) {
        return all_frameworks[i].detected;
      }
    }
  }

  return NR_FW_UNSET;
}

/*
 * Library handling.
 *
 * For the purposes of the agent, a "library" is distinct from a "framework" in
 * that the user may have multiple libraries in use in a single request, all of
 * which are instrumented. This contrasts with frameworks, of which there is
 * only ever one detected per request. Otherwise, the detection method works
 * the exact same way (with the exception that libraries don't support special
 * detection functions).
 *
 * The enable function should call
 * nr_php_add_library_{pre,post,exec}_callback_function(), which add the
 * callback to every framework in the wraprec's
 * {pre,post,execute}_special_instrumentation array. (Ugly, but effective.)
 * This works because we don't actually check if a framework is set when
 * calling instrumentation callbacks: provided we set them all, even if the
 * current framework is FW_UNSET, the callback will still be called.
 */

typedef struct _nr_library_table_t {
  const char* library_name;
  const char* file_to_check;
  size_t file_to_check_len;
  nr_library_enable_fn_t enable;
} nr_library_table_t;

/*
 * Note that all paths should be in lowercase.
 */
// clang-format: off
static nr_library_table_t libraries[] = {
    {"Doctrine 2", NR_PSTR("doctrine/orm/query.php"), nr_doctrine2_enable},
    {"Guzzle 3", NR_PSTR("guzzle/http/client.php"), nr_guzzle3_enable},
    {"Guzzle 4-5", NR_PSTR("hasemitterinterface.php"), nr_guzzle4_enable},
    {"Guzzle 6", NR_PSTR("guzzle/src/functions_include.php"), nr_guzzle6_enable},

    {"MongoDB", NR_PSTR("mongodb/src/client.php"), nr_mongodb_enable},

    /*
     * The first path is for Composer installs, the second is for
     * /usr/local/bin.
     */
    {"PHPUnit", NR_PSTR("phpunit/src/framework/test.php"), nr_phpunit_enable},
    {"PHPUnit", NR_PSTR("phpunit/framework/test.php"), nr_phpunit_enable},

    {"Predis", NR_PSTR("predis/src/client.php"), nr_predis_enable},
    {"Predis", NR_PSTR("predis/client.php"), nr_predis_enable},

    /*
     * Allow Zend Framework 1.x to be detected as a library as well as a
     * framework. This allows Zend_Http_Client to be instrumented when used
     * with other frameworks or even without a framework at all. This is
     * necessary for Magento in particular, which is built on ZF1.
     */
    {"Zend_Http", NR_PSTR("zend/http/client.php"), nr_zend_http_enable},

    /*
     * Allow Laminas Framework 3.x to be detected as a library as well as a
     * framework. This allows Laminas_Http_Client to be instrumented when used
     * with other frameworks or even without a framework at all.
     */
    {"Laminas_Http", NR_PSTR("laminas-http/src/client.php"), nr_laminas_http_enable},

    /*
     * Other frameworks, detected only, but not specifically
     * instrumented. We detect these as libraries so that we don't prevent
     * detection of a supported framework or library later (since a transaction
     * can only have one framework).
     */
    {"Aura1", NR_PSTR("aura/framework/system.php"), NULL},
    {"Aura2", NR_PSTR("aura/di/src/containerinterface.php"), NULL},
    {"Aura3", NR_PSTR("aura/di/src/containerconfiginterface.php"), NULL},
    {"CakePHP3", NR_PSTR("cakephp/src/core/functions.php"), NULL},
    {"Fuel", NR_PSTR("fuel/core/classes/fuel.php"), NULL},
    {"Lithium", NR_PSTR("lithium/core/libraries.php"), NULL},
    {"Phpbb", NR_PSTR("phpbb/request/request.php"), NULL},
    {"Phpixie2", NR_PSTR("phpixie/core/classes/phpixie/pixie.php"), NULL},
    {"Phpixie3", NR_PSTR("phpixie/framework.php"), NULL},
    {"React", NR_PSTR("react/event-loop/src/loopinterface.php"), NULL},
    {"SilverStripe", NR_PSTR("injector/silverstripeinjectioncreator.php"), NULL},
    {"SilverStripe4", NR_PSTR("silverstripeserviceconfigurationlocator.php"), NULL},
    {"Typo3", NR_PSTR("classes/typo3/flow/core/bootstrap.php"), NULL},
    {"Typo3", NR_PSTR("typo3/sysext/core/classes/core/bootstrap.php"), NULL},
    {"Yii2", NR_PSTR("yii2/baseyii.php"), NULL},

    /*
     * Other CMS (content management systems), detected only, but
     * not specifically instrumented.
     */
    {"Moodle", NR_PSTR("moodlelib.php"), NULL},
    /*
     * It is likely that this will never be found, since the CodeIgniter.php
     * will get loaded first, and as such mark this transaction as belonging to
     * CodeIgniter, and not Expession Engine.
     */
    {"ExpressionEngine", NR_PSTR("system/expressionengine/config/config.php"), NULL},
    /*
     * ExpressionEngine 5, however, has a very obvious file we can look for.
     */
    {"ExpressionEngine5", NR_PSTR("expressionengine/boot/boot.php"), NULL},
    /*
     * DokuWiki uses doku.php as an entry point, but has other files that are
     * loaded directly that this won't pick up. That's probably OK for
     * supportability metrics, but we'll add the most common name for the
     * configuration file as well just in case.
     */
    {"DokuWiki", NR_PSTR("doku.php"), NULL},
    {"DokuWiki", NR_PSTR("conf/dokuwiki.php"), NULL},

    /*
     * SugarCRM no longer has a community edition, so this likely only works
     * with older versions.
     */
    {"SugarCRM", NR_PSTR("sugarobjects/sugarconfig.php"), NULL},

    {"Xoops", NR_PSTR("class/xoopsload.php"), NULL},
    {"E107", NR_PSTR("e107_handlers/e107_class.php"), NULL},
};
// clang-format: on

static size_t num_libraries = sizeof(libraries) / sizeof(nr_library_table_t);

// clang-format: off
static nr_library_table_t logging_frameworks[] = {
    /* Monolog - Logging for PHP */
    {"Monolog", NR_PSTR("monolog/logger.php"), nr_monolog_enable},
    /* Consolidation/Log - Logging for PHP */
    {"Consolidation/Log", NR_PSTR("consolidation/log/src/logger.php"), NULL},
    /* laminas-log - Logging for PHP */
    {"laminas-log", NR_PSTR("laminas-log/src/logger.php"), NULL},
    /* cakephp-log - Logging for PHP */
    {"cakephp-log", NR_PSTR("cakephp/log/log.php"), NULL},
    /* Analog - Logging for PHP */
    {"Analog", NR_PSTR("analog/analog.php"), NULL},
};
// clang-format: on

static size_t num_logging_frameworks
    = sizeof(logging_frameworks) / sizeof(nr_library_table_t);

/* Package handling for Vulnerability Management */
typedef struct _nr_vuln_mgmt_table_t {
  const char* package_name;
  const char* file_to_check;
  nr_vuln_mgmt_enable_fn_t enable;
} nr_vuln_mgmt_table_t;

/* Note that all paths should be in lowercase. */
static const nr_vuln_mgmt_table_t vuln_mgmt_packages[] = {
    {"Drupal", "drupal/component/dependencyinjection/container.php", nr_drupal_version},
    {"Wordpress", "wp-includes/version.php", nr_wordpress_version},
};

static const size_t num_packages
    = sizeof(vuln_mgmt_packages) / sizeof(nr_vuln_mgmt_table_t);

/*
 * This const char[] provides enough white space to indent functions to
 * (sizeof (nr_php_indentation_spaces) / NR_EXECUTE_INDENTATION_WIDTH) deep.
 * Anything deeper than that will all be shown with the same depth.
 */
static const char nr_php_indentation_spaces[]
    = "                                                                        "
      "     "
      "                                                                        "
      "     "
      "                                                                        "
      "     "
      "                                                                        "
      "     "
      "                                                                        "
      "     "
      "                                                                        "
      "     ";

#define NR_EXECUTE_INDENTATION_WIDTH 2

/*
 * Return the number of spaces of indentation to use when printing PHP stack
 * frames.
 */
static int nr_php_show_exec_indentation(TSRMLS_D) {
  if (NRPRG(php_cur_stack_depth) < 0) {
    return 0;
  }
  return NRPRG(php_cur_stack_depth) * NR_EXECUTE_INDENTATION_WIDTH;
}

/*
 * Note that this function doesn't handle internal functions, and will crash if
 * you give it one.
 */
static void nr_php_show_exec(NR_EXECUTE_PROTO TSRMLS_DC) {
  char argstr[NR_EXECUTE_DEBUG_STRBUFSZ];
  const char* filename = nr_php_op_array_file_name(NR_OP_ARRAY);
  const char* function_name = nr_php_op_array_function_name(NR_OP_ARRAY);

  argstr[0] = '\0';

  if (NR_OP_ARRAY->scope) {
    /*
     * classname::method
     */
    nr_show_execute_params(NR_EXECUTE_ORIG_ARGS, argstr TSRMLS_CC);
    nrl_verbosedebug(
        NRL_AGENT,
        "execute: %.*s scope={%.*s} function={" NRP_FMT_UQ
        "}"
        " params={" NRP_FMT_UQ
        "}"
        " %.5s"
        "@ " NRP_FMT_UQ ":%d",
        nr_php_show_exec_indentation(TSRMLS_C), nr_php_indentation_spaces,
        NRSAFELEN(nr_php_class_entry_name_length(NR_OP_ARRAY->scope)),
        nr_php_class_entry_name(NR_OP_ARRAY->scope),
        NRP_PHP(function_name ? function_name : "?"), NRP_ARGSTR(argstr),
#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
        nr_php_op_array_get_wraprec(NR_OP_ARRAY TSRMLS_CC) ? " *" : "",
#else
        nr_php_get_wraprec(execute_data->func) ? " *" : "",
#endif
        NRP_FILENAME(filename), NR_OP_ARRAY->line_start);
  } else if (NR_OP_ARRAY->function_name) {
    /*
     * function
     */
    nr_show_execute_params(NR_EXECUTE_ORIG_ARGS, argstr TSRMLS_CC);
    nrl_verbosedebug(
        NRL_AGENT,
        "execute: %.*s function={" NRP_FMT_UQ
        "}"
        " params={" NRP_FMT_UQ
        "}"
        " %.5s"
        "@ " NRP_FMT_UQ ":%d",
        nr_php_show_exec_indentation(TSRMLS_C), nr_php_indentation_spaces,
        NRP_PHP(function_name), NRP_ARGSTR(argstr),
#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
        nr_php_op_array_get_wraprec(NR_OP_ARRAY TSRMLS_CC) ? " *" : "",
#else
        nr_php_get_wraprec(execute_data->func) ? " *" : "",
#endif
        NRP_FILENAME(filename), NR_OP_ARRAY->line_start);
  } else if (NR_OP_ARRAY->filename) {
    /*
     * file
     */
    nrl_verbosedebug(NRL_AGENT, "execute: %.*s file={" NRP_FMT "}",
                     nr_php_show_exec_indentation(TSRMLS_C),
                     nr_php_indentation_spaces, NRP_FILENAME(filename));
  } else {
    /*
     * unknown
     */
    nrl_verbosedebug(NRL_AGENT, "execute: %.*s ?",
                     nr_php_show_exec_indentation(TSRMLS_C),
                     nr_php_indentation_spaces);
  }
}

/*
 * Show the return value, assuming that there is one.
 * The return value is an attribute[sic] of the caller site,
 * not an attribute of if the callee has actually returned something.
 */
static void nr_php_show_exec_return(NR_EXECUTE_PROTO TSRMLS_DC) {
  char argstr[NR_EXECUTE_DEBUG_STRBUFSZ];
  zval* return_value = nr_php_get_return_value(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (NULL != return_value) {
    nr_format_zval_for_debug(return_value, argstr, 0,
                             NR_EXECUTE_DEBUG_STRBUFSZ - 1, 0 TSRMLS_CC);
    nrl_verbosedebug(NRL_AGENT, "execute: %.*s return: " NRP_FMT,
                     nr_php_show_exec_indentation(TSRMLS_C),
                     nr_php_indentation_spaces, NRP_ARGSTR(argstr));
  }
}

static nrframework_t nr_try_detect_framework(
    const nr_framework_table_t frameworks[],
    size_t num_frameworks,
    const char* filename,
    const size_t filename_len TSRMLS_DC);
static nrframework_t nr_try_force_framework(
    const nr_framework_table_t frameworks[],
    size_t num_frameworks,
    nrframework_t forced,
    const char* filename TSRMLS_DC);

static void nr_framework_log(const char* log_prefix,
                             const char* framework_name) {
  nrl_debug(NRL_FRAMEWORK, "%s = '%s'", log_prefix, framework_name);
}

void nr_framework_create_metric(TSRMLS_D) {
  char* metric_name = NULL;
  const char* framework_name = "None";
  nrframework_t fw = NRPRG(current_framework);

  if (NR_FW_UNSET == fw) {
    return;
  }

  if (NR_FW_NONE != fw) {
    int i;

    for (i = 0; i < num_all_frameworks; i++) {
      if (fw == all_frameworks[i].detected) {
        framework_name = all_frameworks[i].framework_name;
        break;
      }
    }
  }

  if (NR_FW_UNSET == NRINI(force_framework)) {
    metric_name
        = nr_formatf("Supportability/framework/%s/detected", framework_name);
  } else {
    metric_name
        = nr_formatf("Supportability/framework/%s/forced", framework_name);
  }

  if (NRPRG(txn)) {
    nrm_force_add(NRPRG(txn)->unscoped_metrics, metric_name, 0);
  }

  nr_free(metric_name);
}

/*
 * Detect or force the framework, if we haven't done so already.
 *
 * When debugging framework detection,
 * if you want to see the files as they are loaded into PHP,
 * consider the tracing in nr_php_execute_file that's sensitive to
 *   special_flags.show_loaded_files
 *
 * This function manages the state of the various global variables
 * associated with framework detection and forcing.
 */
static void nr_execute_handle_framework(const nr_framework_table_t frameworks[],
                                        size_t num_frameworks,
                                        const char* filename,
                                        const size_t filename_len TSRMLS_DC) {
  if (NR_FW_UNSET != NRPRG(current_framework)) {
    return;
  }

  if (NR_FW_UNSET == NRINI(force_framework)) {
    nrframework_t detected_framework = NR_FW_UNSET;

    detected_framework = nr_try_detect_framework(
        frameworks, num_frameworks, filename, filename_len TSRMLS_CC);
    if (NR_FW_UNSET != detected_framework) {
      NRPRG(current_framework) = detected_framework;
    }
  } else if (NR_FW_NONE == NRINI(force_framework)) {
    nr_framework_log("forcing framework", "None");
    NRPRG(current_framework) = NR_FW_NONE;
  } else {
    nrframework_t forced_framework = NR_FW_UNSET;

    forced_framework = nr_try_force_framework(
        frameworks, num_frameworks, NRINI(force_framework), filename TSRMLS_CC);
    if (NR_FW_UNSET != forced_framework) {
      NRPRG(current_framework) = forced_framework;
    }
  }
}

#define STR_AND_LEN(v) v, v##_len

/*
 * Attempt to detect a framework.
 * Call the appropriate enable function if we find the framework.
 * Return the framework found, or NR_FW_UNSET otherwise.
 */
static nrframework_t nr_try_detect_framework(
    const nr_framework_table_t frameworks[],
    size_t num_frameworks,
    const char* filename,
    const size_t filename_len TSRMLS_DC) {
  nrframework_t detected = NR_FW_UNSET;
  size_t i;

  for (i = 0; i < num_frameworks; i++) {
    if (nr_striendswith(STR_AND_LEN(filename),
                        STR_AND_LEN(frameworks[i].file_to_check))) {
      /*
       * If we have a special check function and it tells us to ignore
       * the file name because some other condition wasn't met, continue
       * the loop.
       */
      if (frameworks[i].special) {
        nr_framework_classification_t special
            = frameworks[i].special(filename TSRMLS_CC);

        if (FRAMEWORK_IS_NORMAL == special) {
          continue;
        }
      }

      nr_framework_log("detected framework", frameworks[i].framework_name);
      nrl_verbosedebug(
          NRL_FRAMEWORK, "framework '%s' detected with %s, which ends with %s",
          frameworks[i].framework_name, filename, frameworks[i].file_to_check);

      frameworks[i].enable(TSRMLS_C);
      detected = frameworks[i].detected;
      goto end;
    }
  }

end:
  return detected;
}

/*
 * We are forcing the framework. Attempt to initialize a forced
 * framework.
 * Return the framework that we have forced, or NR_FW_UNSET if we couldn't find
 * such a framework.
 *
 * Call the appropriate enable function if we find the framework to force.
 */
static nrframework_t nr_try_force_framework(
    const nr_framework_table_t frameworks[],
    size_t num_frameworks,
    nrframework_t forced,
    const char* filename TSRMLS_DC) {
  size_t i;

  for (i = 0; i < num_frameworks; i++) {
    if (forced == frameworks[i].detected) {
      if (frameworks[i].special) {
        nr_framework_classification_t special
            = frameworks[i].special(filename TSRMLS_CC);

        if (FRAMEWORK_IS_NORMAL == special) {
          continue;
        }
      }

      nr_framework_log("forcing framework", frameworks[i].framework_name);

      frameworks[i].enable(TSRMLS_C);
      return frameworks[i].detected;
    }
  }
  return NR_FW_UNSET;
}

static void nr_execute_handle_library(const char* filename,
                                      const size_t filename_len TSRMLS_DC) {
  size_t i;

  for (i = 0; i < num_libraries; i++) {
    if (nr_striendswith(STR_AND_LEN(filename),
                        STR_AND_LEN(libraries[i].file_to_check))) {
      nrl_debug(NRL_INSTRUMENT, "detected library=%s",
                libraries[i].library_name);

      nr_fw_support_add_library_supportability_metric(
          NRPRG(txn), libraries[i].library_name);

      if (NULL != libraries[i].enable) {
        libraries[i].enable(TSRMLS_C);
      }
    }
  }
}

static void nr_execute_handle_logging_framework(const char* filename,
                                                const size_t filename_len
                                                    TSRMLS_DC) {
  bool is_enabled = false;
  size_t i;

  for (i = 0; i < num_logging_frameworks; i++) {
    if (nr_striendswith(STR_AND_LEN(filename),
                        STR_AND_LEN(logging_frameworks[i].file_to_check))) {
      nrl_debug(NRL_INSTRUMENT, "detected library=%s",
                logging_frameworks[i].library_name);

      nr_fw_support_add_library_supportability_metric(
          NRPRG(txn), logging_frameworks[i].library_name);

      if (NRINI(logging_enabled) && NULL != logging_frameworks[i].enable) {
        is_enabled = true;
        logging_frameworks[i].enable(TSRMLS_C);
      }
      nr_fw_support_add_logging_supportability_metric(
          NRPRG(txn), logging_frameworks[i].library_name, is_enabled);
    }
  }
}

#undef STR_AND_LEN

static void nr_execute_handle_package(const char* filename) {
  if (NULL == filename || 0 >= nr_strlen(filename)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: The file name is NULL",
                     __func__);
    return;
  }
  char* filename_lower = nr_string_to_lowercase(filename);
  size_t i = 0;

  for (i = 0; i < num_packages; i++) {
    if (nr_stridx(filename_lower, vuln_mgmt_packages[i].file_to_check) >= 0) {
      if (NULL != vuln_mgmt_packages[i].enable) {
        vuln_mgmt_packages[i].enable();
      }
    }
  }

  nr_free(filename_lower);
}

/*
 * Purpose : Detect library and framework usage from a PHP file.
 *
 *           Enables a library or framework if the passed file is
 *           defined as a key file for this library or framework.
 *
 * Params  : 1. Full name of a PHP file.
 */
static void nr_php_user_instrumentation_from_file(const char* filename,
                                                  const size_t filename_len
                                                      TSRMLS_DC) {
  /* short circuit if filename_len is 0; a single place short circuit */
  if (0 == filename_len) {
    nrl_verbosedebug(NRL_AGENT,
                     "%s - received invalid filename_len for file=%s", __func__,
                     filename);
    return;
  }
  nr_execute_handle_framework(all_frameworks, num_all_frameworks, filename,
                              filename_len TSRMLS_CC);
  nr_execute_handle_library(filename, filename_len TSRMLS_CC);
  nr_execute_handle_logging_framework(filename, filename_len TSRMLS_CC);
  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_execute_handle_package(filename);
  }
}

/*
 * The maximum length of a custom metric.
 */
#define METRIC_NAME_MAX_LEN 512

static void nr_php_execute_file(const zend_op_array* op_array,
                                NR_EXECUTE_PROTO TSRMLS_DC) {
  const char* filename = nr_php_op_array_file_name(op_array);
  size_t filename_len = nr_php_op_array_file_name_len(op_array);

  NR_UNUSED_FUNC_RETURN_VALUE;

  if (nrunlikely(NR_PHP_PROCESS_GLOBALS(special_flags).show_loaded_files)) {
    nrl_debug(NRL_AGENT, "loaded file=" NRP_FMT, NRP_FILENAME(filename));
  }

  /*
   * Check for, and handle, frameworks and libraries.
   */
  nr_php_user_instrumentation_from_file(filename, filename_len TSRMLS_CC);

  nr_txn_match_file(NRPRG(txn), filename);

  NR_PHP_PROCESS_GLOBALS(orig_execute)
  (NR_EXECUTE_ORIG_ARGS_OVERWRITE TSRMLS_CC);

  if (0 == nr_php_recording(TSRMLS_C)) {
    return;
  }

  nr_php_add_user_instrumentation(TSRMLS_C);
}

/*
 * Purpose : Initialise a metadata structure from an op array.
 *
 * Params  : 1. A pointer to a metadata structure.
 *           2. The op array.
 *
 * Note    : It is the responsibility of the caller to allocate the metadata
 *           structure. In general, it's expected that this will be a pointer
 *           to a stack variable.
 */
static void nr_php_execute_metadata_init(nr_php_execute_metadata_t* metadata,
                                         zend_op_array* op_array) {
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP7+ */
  if (op_array->scope && op_array->scope->name && op_array->scope->name->len) {
    metadata->scope = op_array->scope->name;
    zend_string_addref(metadata->scope);
  } else {
    metadata->scope = NULL;
  }
  if (op_array->function_name && op_array->function_name->len) {
    metadata->function = op_array->function_name;
    zend_string_addref(metadata->function);
  } else {
    metadata->function = NULL;
  }
  if (!NRINI(code_level_metrics_enabled)
      || ZEND_USER_FUNCTION != op_array->type) {
    metadata->filepath = NULL;
    return;
  }
  if (op_array->filename && op_array->filename->len) {
    metadata->filepath = op_array->filename;
    zend_string_addref(metadata->filepath);
  } else {
    metadata->filepath = NULL;
  }

  metadata->function_lineno = op_array->line_start;

#else
  metadata->op_array = op_array;
#endif /* PHP7 */
}

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP7+ */
/*
 * Purpose : If code level metrics are enabled, use the metadata to create agent
 * attributes in the segment with code level metrics.
 *
 * Params  : 1. segment to create and add agent attributes to
 *           2. metadata that will populate the CLM attributes
 *
 * Returns : void
 *
 * Note: PHP has a concept of calling files with no function names.  In the
 *       case of a file being called when there is no function name, the agent
 *       instruments the file.  In this case, we provide the filename to CLM
 *       as the "function" name.
 *       Current CLM functionality only works with PHP 7+
 */
static inline void nr_php_execute_segment_add_code_level_metrics(
    nr_segment_t* segment,
    const nr_php_execute_metadata_t* metadata) {
  /*
   * Check if code level metrics are enabled in the ini.
   * If they aren't, exit and don't add any attributes.
   */
  if (!NRINI(code_level_metrics_enabled)) {
    return;
  }

  if (NULL == metadata) {
    return;
  }

  if (NULL == segment) {
    return;
  }

  /*
   * At a minimum, at least one of the following attribute combinations MUST be
   * implemented in order for customers to be able to accurately identify their
   * instrumented functions:
   *  - code.filepath AND code.function
   *  - code.namespace AND code.function
   *
   * If we don't have the minimum requirements, exit and don't add any
   * attributes.
   *
   * Additionally, none of the needed attributes can exceed 255 characters.
   */

#define CLM_STRLEN_MAX (255)

#define CHK_CLM_STRLEN(s, zstr_len) \
  if (CLM_STRLEN_MAX < zstr_len) {  \
    s = NULL;                       \
  }

  const char* namespace = NULL;
  const char* function = NULL;
  const char* filepath = NULL;

  if (NULL != metadata->scope) {
    namespace = ZSTR_VAL(metadata->scope);
    CHK_CLM_STRLEN(namespace, ZSTR_LEN(metadata->scope));
  }

  if (NULL != metadata->function) {
    function = ZSTR_VAL(metadata->function);
    CHK_CLM_STRLEN(function, ZSTR_LEN(metadata->function));
  }

  if (NULL != metadata->filepath) {
    filepath = ZSTR_VAL(metadata->filepath);
    CHK_CLM_STRLEN(filepath, ZSTR_LEN(metadata->filepath));
  }

#undef CHK_CLM_STRLEN

  if (1 == metadata->function_lineno) {
    /*
     * It's a file.  For CLM purposes, the "function" name is the filepath.
     */
    function = filepath;
  }

  if (nr_strempty(function)) {
    /*
     * Name isn't set so don't do anything
     */
    return;
  }
  if (nr_strempty(namespace) && nr_strempty(filepath)) {
    /*
     * CLM MUST have either function+namespace or function+filepath.
     */
    return;
  }

  /*
   * Only go through the trouble of actually allocating agent attributes if we
   * know we have valid values to turn into attributes.
   */

  if (NULL == segment->attributes) {
    segment->attributes = nr_attributes_create(segment->txn->attribute_config);
  }

  if (nrunlikely(NULL == segment->attributes)) {
    return;
  }

#define CLM_ATTRIBUTE_DESTINATION                                      \
  (NR_ATTRIBUTE_DESTINATION_TXN_TRACE | NR_ATTRIBUTE_DESTINATION_ERROR \
   | NR_ATTRIBUTE_DESTINATION_TXN_EVENT | NR_ATTRIBUTE_DESTINATION_SPAN)

  /*
   * If the string is empty, CLM specs say don't add it.
   * nr_attributes_agent_add_string is okay with an empty string attribute.
   * Already checked function for strempty no need to check again, but will need
   * to check filepath and namespace.
   */

  nr_attributes_agent_add_string(segment->attributes, CLM_ATTRIBUTE_DESTINATION,
                                 "code.function", function);

  if (!nr_strempty(filepath)) {
    nr_attributes_agent_add_string(segment->attributes,
                                   CLM_ATTRIBUTE_DESTINATION, "code.filepath",
                                   filepath);
  }

  if (!nr_strempty(namespace)) {
    nr_attributes_agent_add_string(segment->attributes,
                                   CLM_ATTRIBUTE_DESTINATION, "code.namespace",
                                   namespace);
  }

  nr_attributes_agent_add_long(segment->attributes, CLM_ATTRIBUTE_DESTINATION,
                               "code.lineno", metadata->function_lineno);
}

#endif
/*
 * Purpose : Create a metric name from the given metadata.
 *
 * Params  : 1. A pointer to the metadata.
 *           2. A pointer to an allocated buffer to place the name in.
 *           3. The size of the buffer, in bytes.
 *
 * Warning : No check is made whether buf is valid, as the normal use case for
 *           this involves alloca(), which doesn't signal errors via NULL (or
 *           any other useful return value). Similarly, metadata is unchecked.
 */
static void nr_php_execute_metadata_metric(
    const nr_php_execute_metadata_t* metadata,
    char* buf,
    size_t len) {
  const char* function_name;
  const char* scope_name;

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP7+ */
  scope_name = metadata->scope ? ZSTR_VAL(metadata->scope) : NULL;
  function_name = metadata->function ? ZSTR_VAL(metadata->function) : NULL;
#else
  scope_name = nr_php_op_array_scope_name(metadata->op_array);
  function_name = nr_php_op_array_function_name(metadata->op_array);
#endif /* PHP7 */

  snprintf(buf, len, "Custom/%s%s%s", scope_name ? scope_name : "",
           scope_name ? "::" : "", function_name ? function_name : "<unknown>");
}

/*
 * Purpose : Release any cached metadata.
 *
 * Params  : 1. A pointer to the metadata.
 */
static inline void nr_php_execute_metadata_release(
    nr_php_execute_metadata_t* metadata) {
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO

  if (NULL != metadata->scope) {
    zend_string_release(metadata->scope);
    metadata->scope = NULL;
  }

  if (NULL != metadata->function) {
    zend_string_release(metadata->function);
    metadata->function = NULL;
  }

  if (NULL != metadata->filepath) {
    zend_string_release(metadata->filepath);
    metadata->filepath = NULL;
  }

#else
  metadata->op_array = NULL;
#endif /* PHP7 */
}

static inline void nr_php_execute_segment_add_metric(
    nr_segment_t* segment,
    const nr_php_execute_metadata_t* metadata,
    bool create_metric) {
  char buf[METRIC_NAME_MAX_LEN];

  nr_php_execute_metadata_metric(metadata, buf, sizeof(buf));

  if (create_metric) {
    nr_segment_add_metric(segment, buf, true);
  }
  nr_segment_set_name(segment, buf);
}

/*
 * Purpose : Evaluate what the disposition of the given segment is: do we
 *           discard or keep it, and if the latter, do we need to create a
 *           custom metric?
 *
 * Params  : 1. The stacked segment to end.
 *              OAPI does not use stacked segments, and should pass
 *              a heap allocated segment instead
 *           2. The function naming metadata.
 *           3. Whether to create a metric.
 */
static inline void nr_php_execute_segment_end(
    nr_segment_t* stacked,
    const nr_php_execute_metadata_t* metadata,
    bool create_metric TSRMLS_DC) {
  nrtime_t duration;

  if (NULL == stacked) {
    return;
  }

  if (0 == stacked->stop_time) {
    /*
     * Only set if it wasn't set already.
     */
    stacked->stop_time = nr_txn_now_rel(NRPRG(txn));
  }

  duration = nr_time_duration(stacked->start_time, stacked->stop_time);
  if (create_metric || (duration >= NR_PHP_PROCESS_GLOBALS(expensive_min))
      || nr_vector_size(stacked->metrics) || stacked->id || stacked->attributes
      || stacked->error) {

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
    // There are no stacked segments for OAPI.
    nr_segment_t* s = stacked;
#else
    nr_segment_t* s = nr_php_stacked_segment_move_to_heap(stacked TSRMLS_CC);
#endif // OAPI
    nr_php_execute_segment_add_metric(s, metadata, create_metric);

    /*
     * Check if code level metrics are enabled in the ini.
     * If they aren't, exit and don't create any CLM.
     */
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP >= PHP7 */
    if (NRINI(code_level_metrics_enabled)) {
      nr_php_execute_segment_add_code_level_metrics(s, metadata);
    }
#endif

    nr_segment_end(&s);
  } else {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
    // There are no stacked segments for OAPI.
    nr_segment_discard(&stacked);
#else
    nr_php_stacked_segment_deinit(stacked TSRMLS_CC);
#endif // OAPI
  }
}

/*
 * This is the user function execution hook. Hook the user-defined (PHP)
 * function execution. For speed, we have a pointer that we've installed in the
 * function record as a flag to indicate whether to instrument this function.
 * If the flag is NULL, then we've only added a couple of CPU instructions to
 * the call path and thus the overhead is (hopefully) very low.
 */
#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
    || defined OVERWRITE_ZEND_EXECUTE_DATA /* not OAPI */
static void nr_php_execute_enabled(NR_EXECUTE_PROTO TSRMLS_DC) {
  int zcaught = 0;
  nrtime_t txn_start_time;
  nr_php_execute_metadata_t metadata = {0};
  nr_segment_t stacked = {0};
  nr_segment_t* segment = NULL;
  nruserfn_t* wraprec = NULL;

  NRTXNGLOBAL(execute_count) += 1;

  if (nrunlikely(OP_ARRAY_IS_A_FILE(NR_OP_ARRAY))) {
    nr_php_execute_file(NR_OP_ARRAY, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    return;
  }

  /*
   * The function name needs to be checked before the NR_OP_ARRAY->fn_flags
   * since in PHP 5.1 fn_flags is not initialized for files.
   */
#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
  wraprec = nr_php_op_array_get_wraprec(NR_OP_ARRAY TSRMLS_CC);
#else
  wraprec = nr_php_get_wraprec(execute_data->func);
#endif

  if (NULL != wraprec) {
    /*
     * This is the case for specifically requested custom instrumentation.
     */
    bool create_metric = wraprec->create_metric;

    nr_php_execute_metadata_init(&metadata, NR_OP_ARRAY);

    nr_txn_force_single_count(NRPRG(txn), wraprec->supportability_metric);

    /*
     * Check for, and handle, frameworks.
     */

    if (wraprec->is_names_wt_simple) {
      nr_txn_name_from_function(NRPRG(txn), wraprec->funcname,
                                wraprec->classname);
    }

    /*
     * The nr_txn_should_create_span_events() check is there so we don't record
     * error attributes on the txn (and root segment) because it should already
     * be recorded on the span that exited unhandled.
     */
    if (wraprec->is_exception_handler
        && !nr_txn_should_create_span_events(NRPRG(txn))) {
      zval* exception
          = nr_php_get_user_func_arg(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

      /*
       * The choice of E_ERROR for the error level is basically arbitrary, but
       * matches the error level PHP uses if there isn't an exception handler,
       * so this should give more consistency for the user in terms of what
       * they'll see with and without an exception handler installed.
       */
      nr_php_error_record_exception(
          NRPRG(txn), exception, nr_php_error_get_priority(E_ERROR), true,
          "Uncaught exception ", &NRPRG(exception_filters) TSRMLS_CC);
    }

    txn_start_time = nr_txn_start_time(NRPRG(txn));

    segment = nr_php_stacked_segment_init(&stacked TSRMLS_CC);
    zcaught = nr_zend_call_orig_execute_special(wraprec, segment,
                                                NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

    /*
     * During this call, the transaction may have been ended and/or a new
     * transaction may have started.  To detect this, we compare the
     * currently active transaction's start time with the transaction
     * start time we saved before.
     *
     * Just comparing the transaction pointer is not enough, as a newly
     * started transaction might actually obtain the same address as a
     * transaction freed before.
     */
    if (nrunlikely(nr_txn_start_time(NRPRG(txn)) != txn_start_time)) {
      segment = NULL;
    }

    nr_php_execute_segment_end(segment, &metadata, create_metric TSRMLS_CC);
    nr_php_execute_metadata_release(&metadata);

    if (nrunlikely(zcaught)) {
      zend_bailout();
    }
  } else if (NRINI(tt_detail) && NR_OP_ARRAY->function_name) {
    nr_php_execute_metadata_init(&metadata, NR_OP_ARRAY);

    /*
     * This is the case for transaction_tracer.detail >= 1 requested custom
     * instrumentation.
     */

    txn_start_time = nr_txn_start_time(NRPRG(txn));

    segment = nr_php_stacked_segment_init(&stacked TSRMLS_CC);

    zcaught = nr_zend_call_orig_execute_special(wraprec, &stacked,
                                                NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

    if (nr_txn_should_create_span_events(NRPRG(txn))) {
      if (EG(exception)) {
        zval* exception_zval = NULL;
        nr_status_t status;

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP7+ */
        /*
         * On PHP 7, EG(exception) is stored as a zend_object, and is only
         * wrapped in a zval when it actually needs to be.
         */
        zval exception;

        ZVAL_OBJ(&exception, EG(exception));
        exception_zval = &exception;
#else
        /*
         * On PHP 5, the exception is just a regular old zval.
         */
        exception_zval = EG(exception);
#endif /* PHP7 */

        status = nr_php_error_record_exception_segment(
            NRPRG(txn), exception_zval, &NRPRG(exception_filters) TSRMLS_CC);

        if (NR_FAILURE == status) {
          nrl_verbosedebug(
              NRL_AGENT, "%s: unable to record exception on segment", __func__);
        }
      }
    }

    /*
     * During this call, the transaction may have been ended and/or a new
     * transaction may have started.  To detect this, we compare the
     * currently active transaction's start time with the transaction
     * start time we saved before.
     */
    if (nrunlikely(nr_txn_start_time(NRPRG(txn)) != txn_start_time)) {
      segment = NULL;
    }

    nr_php_execute_segment_end(segment, &metadata, false TSRMLS_CC);
    nr_php_execute_metadata_release(&metadata);

    if (nrunlikely(zcaught)) {
      zend_bailout();
    }
  } else {
    /*
     * This is the case for New Relic is enabled, but we're not recording.
     */
    NR_PHP_PROCESS_GLOBALS(orig_execute)
    (NR_EXECUTE_ORIG_ARGS_OVERWRITE TSRMLS_CC);
  }
}

static void nr_php_execute_show(NR_EXECUTE_PROTO TSRMLS_DC) {
  if (nrunlikely(NR_PHP_PROCESS_GLOBALS(special_flags).show_executes)) {
    nr_php_show_exec(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  }

  nr_php_execute_enabled(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (nrunlikely(NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_returns)) {
    nr_php_show_exec_return(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  }
}
#endif // not OAPI

static void nr_php_max_nesting_level_reached(TSRMLS_D) {
  /*
   * Reset the stack depth to ensure that when php_error is done executing
   * longjmp to discard all of the C frames and PHP frames, that the stack
   * depth is correct. Execution will probably not continue after E_ERROR;
   * that decision may rest on the error handler(s) registered as callbacks.
   */
  NRPRG(php_cur_stack_depth) = 0;

  nrl_error(NRL_AGENT,
            "The New Relic imposed maximum PHP function nesting level of '%d' "
            "has been reached. "
            "If you think this limit is too small, adjust the value of the "
            "setting newrelic.special.max_nesting_level in the newrelic.ini "
            "file, and restart php.",
            (int)NRINI(max_nesting_level));

  php_error(E_ERROR,
            "Aborting! "
            "The New Relic imposed maximum PHP function nesting level of '%d' "
            "has been reached. "
            "This limit is to prevent the PHP execution from catastrophically "
            "running out of C-stack frames. "
            "If you think this limit is too small, adjust the value of the "
            "setting newrelic.special.max_nesting_level in the newrelic.ini "
            "file, and restart php. "
            "Please file a ticket at https://support.newrelic.com if you need "
            "further assistance. ",
            (int)NRINI(max_nesting_level));
}

/*
 * This function is single entry, single exit, so that we can keep track
 * of the PHP stack depth. NOTE: the stack depth is not maintained in
 * the presence of longjmp as from zend_bailout when processing zend internal
 * errors, as for example when calling php_error.
 */
#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
    || defined OVERWRITE_ZEND_EXECUTE_DATA /* not OAPI */
void nr_php_execute(NR_EXECUTE_PROTO_OVERWRITE TSRMLS_DC) {
  /*
   * We do not use zend_try { ... } mechanisms here because zend_try
   * involves a setjmp, and so may be too expensive along this oft-used
   * path. We believe that the corresponding zend_catch will only be
   * taken when there's an internal zend error, and execution will some
   * come to a controlled premature end. The corresponding zend_catch
   * is NOT called when PHP exceptions are thrown, which happens
   * (relatively) frequently.
   *
   * The only reason for bracketing this with zend_try would be to
   * maintain the consistency of the php_cur_stack_depth counter, which
   * is only used for clamping the depth of PHP stack execution, or for
   * pretty printing PHP stack frames in nr_php_execute_show. Since the
   * zend_catch is called to avoid catastrophe on the way to a premature
   * exit, maintaining this counter perfectly is not a necessity.
   */

  NRPRG(php_cur_stack_depth) += 1;

  if (((int)NRINI(max_nesting_level) > 0)
      && (NRPRG(php_cur_stack_depth) >= (int)NRINI(max_nesting_level))) {
    nr_php_max_nesting_level_reached(TSRMLS_C);
  }

  if (nrunlikely(0 == nr_php_recording(TSRMLS_C))) {
    NR_PHP_PROCESS_GLOBALS(orig_execute)
    (NR_EXECUTE_ORIG_ARGS_OVERWRITE TSRMLS_CC);
  } else {
    int show_executes
        = NR_PHP_PROCESS_GLOBALS(special_flags).show_executes
          || NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_returns;

    if (nrunlikely(show_executes)) {
      nr_php_execute_show(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    } else {
      nr_php_execute_enabled(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    }
  }
  NRPRG(php_cur_stack_depth) -= 1;

  return;
}
#endif // not OAPI

static void nr_php_show_exec_internal(NR_EXECUTE_PROTO_OVERWRITE,
                                      const zend_function* func TSRMLS_DC) {
  char argstr[NR_EXECUTE_DEBUG_STRBUFSZ] = {'\0'};
  const char* name = nr_php_function_debug_name(func);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */
  zval* func_return_value = NULL;
#endif

  nr_show_execute_params(NR_EXECUTE_ORIG_ARGS, argstr TSRMLS_CC);

  nrl_verbosedebug(
      NRL_AGENT,
      "execute: %.*s function={" NRP_FMT_UQ "} params={" NRP_FMT_UQ "}",
      nr_php_show_exec_indentation(TSRMLS_C), nr_php_indentation_spaces,
      NRP_PHP(name ? name : "?"), NRP_ARGSTR(argstr));
}

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO
#define CALL_ORIGINAL \
  (NR_PHP_PROCESS_GLOBALS(orig_execute_internal)(execute_data, return_value))

void nr_php_execute_internal(zend_execute_data* execute_data,
                             zval* return_value NRUNUSED)
#elif ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
#define CALL_ORIGINAL                                               \
  (NR_PHP_PROCESS_GLOBALS(orig_execute_internal)(execute_data, fci, \
                                                 return_value_used TSRMLS_CC))

void nr_php_execute_internal(zend_execute_data* execute_data,
                             zend_fcall_info* fci,
                             int return_value_used TSRMLS_DC)
#else
#define CALL_ORIGINAL                                          \
  (NR_PHP_PROCESS_GLOBALS(orig_execute_internal)(execute_data, \
                                                 return_value_used TSRMLS_CC))

void nr_php_execute_internal(zend_execute_data* execute_data,
                             int return_value_used TSRMLS_DC)
#endif
{
  nrtime_t duration = 0;
  zend_function* func = NULL;
  nr_segment_t* segment;

  if (nrunlikely(!nr_php_recording(TSRMLS_C))) {
    CALL_ORIGINAL;
    return;
  }

  if (nrunlikely(NULL == execute_data)) {
    nrl_verbosedebug(NRL_AGENT, "%s: NULL execute_data", __func__);
    CALL_ORIGINAL;
    return;
  }

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP7+ */
  func = execute_data->func;
#else
  func = execute_data->function_state.function;
#endif /* PHP7 */

  if (nrunlikely(NULL == func)) {
    nrl_verbosedebug(NRL_AGENT, "%s: NULL func", __func__);
    CALL_ORIGINAL;
    return;
  }

  /*
   * Handle the show_executes flags except for show_execute_returns. Getting
   * the return value reliably across versions is hard; given that the likely
   * number of times we'll want the intersection of internal function
   * instrumentation enabled, show_executes enabled, _and_
   * show_execute_returns enabled is zero, let's not spend the time
   * implementing it.
   */
  if (nrunlikely(NR_PHP_PROCESS_GLOBALS(special_flags).show_executes)) {
#if ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
    nr_php_show_exec_internal(NR_EXECUTE_ORIG_ARGS_OVERWRITE, func TSRMLS_CC);
#else
    /*
     * We're passing the same pointer twice. This is inefficient. However, no
     * user is ever likely to be affected, since this is a code path handling
     * a special flag, and it makes the nr_php_show_exec_internal() API cleaner
     * for modern versions of PHP without needing to have another function
     * conditionally compiled.
     */
    nr_php_show_exec_internal((zend_op_array*)func, func TSRMLS_CC);
#endif /* PHP >= 5.5 */
  }
  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  CALL_ORIGINAL;

  duration = nr_time_duration(segment->start_time, nr_txn_now_rel(NRPRG(txn)));
  nr_segment_set_timing(segment, segment->start_time, duration);

  if (duration >= NR_PHP_PROCESS_GLOBALS(expensive_min)) {
    nr_php_execute_metadata_t metadata = {0};

    nr_php_execute_metadata_init(&metadata, (zend_op_array*)func);

    nr_php_execute_segment_add_metric(segment, &metadata, false);

    nr_php_execute_metadata_release(&metadata);
  }

  nr_segment_end(&segment);
}

void nr_php_user_instrumentation_from_opcache(TSRMLS_D) {
  zval* status = NULL;
  zval* scripts = NULL;
  zend_ulong key_num = 0;
  nr_php_string_hash_key_t* key_str = NULL;
  zval* val = NULL;
  const char* filename;
  size_t filename_len;

  status = nr_php_call(NULL, "opcache_get_status");

  if (NULL == status) {
    nrl_warning(NRL_INSTRUMENT,
                "User instrumentation from opcache: error obtaining opcache "
                "status, even though opcache.preload is set");
    return;
  }
  if (IS_ARRAY != Z_TYPE_P(status)) {
    /*
     * `opcache_get_status` returns either an array or false.  If it's not an
     * array, it must have returned false indicating we are unable to get the
     * status yet.
     */
    nrl_debug(NRL_INSTRUMENT,
              "User instrumentation from opcache: opcache status "
              "information is not an array");
    goto end;
  }

  scripts = nr_php_zend_hash_find(Z_ARRVAL_P(status), "scripts");

  if (NULL == scripts) {
    nrl_warning(NRL_INSTRUMENT,
                "User instrumentation from opcache: missing 'scripts' key in "
                "status information");
    goto end;
  }

  if (IS_ARRAY != Z_TYPE_P(scripts)) {
    nrl_warning(NRL_INSTRUMENT,
                "User instrumentation from opcache: 'scripts' value in status "
                "information is not an array");
    goto end;
  }

  nrl_debug(NRL_INSTRUMENT, "User instrumentation from opcache: started");

  ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(scripts), key_num, key_str, val) {
    (void)key_num;
    (void)val;

    filename = ZEND_STRING_VALUE(key_str);
    filename_len = ZEND_STRING_LEN(key_str);

    nr_php_user_instrumentation_from_file(filename, filename_len TSRMLS_CC);
  }
  ZEND_HASH_FOREACH_END();

  nrl_debug(NRL_INSTRUMENT, "User instrumentation from opcache: done");

end:
  nr_php_zval_free(&status);
}

/*
 * nr_php_observer_fcall_begin and nr_php_observer_fcall_end
 * are Observer API function handlers that are the entry point to instrumenting
 * userland code and should replicate the functionality of
 * nr_php_execute_enabled, nr_php_execute, and nr_php_execute_show that are used
 * when hooking in via zend_execute_ex.
 *
 * Observer API functionality was added with PHP 8.0.
 * See nr_php_observer.h/c for more information.
 */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP8+ and OAPI */

static void nr_php_observer_attempt_call_cufa_handler(NR_EXECUTE_PROTO) {
  NR_UNUSED_FUNC_RETURN_VALUE;
  if (NULL == execute_data->prev_execute_data) {
    nrl_verbosedebug(NRL_AGENT, "%s: cannot get previous execute data",
                     __func__);
    return;
  }

  /*
   * COPIED Comment from php_vm.c:
   * To actually determine whether this is a call_user_func_array() call we
   * have to look at one of the previous opcodes. ZEND_DO_FCALL will never be
   * the first opcode in an op array -- minimally, there is always at least a
   * ZEND_INIT_FCALL before it -- so this is safe.
   *
   * When PHP 7+ flattens a call_user_func_array() call into direct opcodes, it
   * uses ZEND_SEND_ARRAY to send the arguments in a single opline, and that
   * opcode is the opcode before the ZEND_DO_FCALL. Therefore, if we see
   * ZEND_SEND_ARRAY, we know it's call_user_func_array(). The relevant code
   * can be found at:
   * https://github.com/php/php-src/blob/php-7.0.19/Zend/zend_compile.c#L3082-L3098
   * https://github.com/php/php-src/blob/php-7.1.5/Zend/zend_compile.c#L3564-L3580
   *
   * In PHP 8, sometimes a ZEND_CHECK_UNDEF_ARGS opcode is added after the call
   * to ZEND_SEND_ARRAY and before ZEND_DO_FCALL so we need to sometimes look
   * back two opcodes instead of just one.
   *
   * Note that this heuristic will fail if the Zend Engine ever starts
   * compiling inlined call_user_func_array() calls differently. PHP 7.2 made
   * a change, but it only optimized array_slice() calls, which as an internal
   * function won't get this far anyway.) We can disable this behaviour by
   * setting the ZEND_COMPILE_NO_BUILTINS compiler flag, but since that will
   * cause additional performance overhead, this should be considered a last
   * resort.
   */

  /*
   * When Observer API is used, this code executes in the context of
   * zend_execute and not in the context of VM (as was the case pre-OAPI),
   * therefore we need to ensure we're dealing with a user function. We cannot
   * safely access the opline of internal functions, and we only want to
   * instrument cufa calls from user functions anyway.
   */
  if (UNEXPECTED(NULL == execute_data->prev_execute_data->func)) {
    nrl_verbosedebug(NRL_AGENT, "%s: cannot get previous function", __func__);
    return;
  }
  if (!ZEND_USER_CODE(execute_data->prev_execute_data->func->type)) {
    nrl_verbosedebug(NRL_AGENT, "%s: caller is php internal function",
                     __func__);
    return;
  }

  if (UNEXPECTED(NULL == execute_data->prev_execute_data->opline)) {
    nrl_verbosedebug(NRL_AGENT, "%s: cannot get previous opline", __func__);
    return;
  }

  const zend_op* prev_opline = execute_data->prev_execute_data->opline;

  /*
   * Extra safety check. Previously, we instrumented by overwritting
   * ZEND_DO_FCALL. Within OAPI, for consistency's sake, we will ensure the same
   */
  if (ZEND_DO_FCALL != prev_opline->opcode) {
    nrl_verbosedebug(NRL_AGENT, "%s: cannot get previous function name",
                     __func__);
    return;
  }
  prev_opline -= 1;  // Checks previous opcode
  if (ZEND_CHECK_UNDEF_ARGS == prev_opline->opcode) {
    prev_opline -= 1;  // Checks previous opcode
  }
  if (ZEND_SEND_ARRAY == prev_opline->opcode) {
    if (UNEXPECTED((NULL == execute_data->func))) {
      nrl_verbosedebug(NRL_AGENT, "%s: cannot get current function", __func__);
      return;
    }
    if (UNEXPECTED(
            NULL
            == execute_data->prev_execute_data->func->common.function_name)) {
      nrl_verbosedebug(NRL_AGENT, "%s: cannot get previous function name",
                       __func__);
      return;
    }

    nr_php_call_user_func_array_handler(NRPRG(cufa_callback),
                                        execute_data->func,
                                        execute_data->prev_execute_data);
  }
}

static void nr_php_instrument_func_begin(NR_EXECUTE_PROTO) {
  nr_segment_t* segment = NULL;
  nruserfn_t* wraprec = NULL;
  nrtime_t txn_start_time = 0;
  int zcaught = 0;
  NR_UNUSED_FUNC_RETURN_VALUE;

  if (NULL == NRPRG(txn)) {
    return;
  }

  NRTXNGLOBAL(execute_count) += 1;
  txn_start_time = nr_txn_start_time(NRPRG(txn));
  /*
   * Handle here, but be aware the classes might not be loaded yet.
   */
  if (nrunlikely(OP_ARRAY_IS_A_FILE(NR_OP_ARRAY))) {
    const char* filename = nr_php_op_array_file_name(NR_OP_ARRAY);
    size_t filename_len = nr_php_op_array_file_name_len(NR_OP_ARRAY);
    nr_execute_handle_framework(all_frameworks, num_all_frameworks,
                                filename, filename_len TSRMLS_CC);
    return;
  }
  if (NULL != NRPRG(cufa_callback) && NRPRG(check_cufa)) {
    /*
     * For PHP 7+, call_user_func_array() is flattened into an inline by
     * default. Because of this, we must check the opcodes set to see whether we
     * are calling it flattened. If we have a cufa callback, we want to call
     * that here. This will create the wraprec for the user function we want to
     * instrument and thus must be called before we search the wraprecs
     *
     * For non-OAPI, this is handled in php_vm.c by overwriting the
     * ZEND_DO_FCALL opcode.
     */
    nr_php_observer_attempt_call_cufa_handler(NR_EXECUTE_ORIG_ARGS);
  }
  wraprec = nr_php_get_wraprec(execute_data->func);

  segment = nr_segment_start(NRPRG(txn), NULL, NULL);

  if (nrunlikely(NULL == segment)) {
    nrl_verbosedebug(NRL_AGENT, "Error starting segment.");
    return;
  }

  if (NULL == wraprec) {
    return;
  }
  /*
   * If a function needs to have arguments modified, do so in
   * nr_zend_call_oapi_special_before.
   */
  segment->wraprec = wraprec;
  zcaught = nr_zend_call_oapi_special_before(wraprec, segment,
                                             NR_EXECUTE_ORIG_ARGS);
  if (nrunlikely(zcaught)) {
    zend_bailout();
  }

  /*
   * During nr_zend_call_oapi_special_before, the transaction may have been
   * ended and/or a new transaction may have started.  To detect this, we
   * compare the currently active transaction's start time with the transaction
   * start time we saved before.
   *
   * Just comparing the transaction pointer is not enough, as a newly
   * started transaction might actually obtain the same address as a
   * transaction freed before.
   */
  if (nrunlikely(nr_txn_start_time(NRPRG(txn)) != txn_start_time)) {
    nrl_verbosedebug(NRL_AGENT,
                     "%s txn ended and/or started while in a wrapped function",
                     __func__);

    return;
  }

  nr_txn_force_single_count(NRPRG(txn), wraprec->supportability_metric);
  /*
   * Check for, and handle, frameworks.
   */
  if (wraprec->is_names_wt_simple) {
    nr_txn_name_from_function(NRPRG(txn), wraprec->funcname,
                              wraprec->classname);
  }
}

static void nr_php_instrument_func_end(NR_EXECUTE_PROTO) {
  int zcaught = 0;
  nr_segment_t* segment = NULL;
  nruserfn_t* wraprec = NULL;
  bool create_metric = false;
  nr_php_execute_metadata_t metadata = {0};
  nrtime_t txn_start_time = 0;

  if (NULL == NRPRG(txn)) {
    return;
  }
  txn_start_time = nr_txn_start_time(NRPRG(txn));

  /*
   * Let's get the framework info.
   */
  if (nrunlikely(OP_ARRAY_IS_A_FILE(NR_OP_ARRAY))) {
    nr_php_execute_file(NR_OP_ARRAY, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    return;
  }

  /*
   * Get the current segment and return if null.
   */
  segment = nr_txn_get_current_segment(NRPRG(txn), NULL);
  if (nrunlikely(NULL == segment)) {
    /*
     * Most likely caused by txn ending prematurely and closing all segments. We
     * can only exit since the segments were already closed.
     */
    return;
  }
  if (nrunlikely(NRPRG(txn)->segment_root == segment)) {
    /*
     * There should be no fcall_end associated with the segment root, If we are
     * here, it is most likely due to an API call to newrelic_end_transaction
     */
    return;
  }

  wraprec = segment->wraprec;

  if (wraprec && wraprec->is_exception_handler) {
    /*
     * After running the exception handler segment, create an error from
     * the exception it handled, and save the error in the transaction.
     * The choice of E_ERROR for the error level is basically arbitrary,
     * but matches the error level PHP uses if there isn't an exception
     * handler, so this should give more consistency for the user in terms
     * of what they'll see with and without an exception handler installed.
     */
    zval* exception
        = nr_php_get_user_func_arg(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    nr_php_error_record_exception(
        NRPRG(txn), exception, nr_php_error_get_priority(E_ERROR), false,
        "Uncaught exception ", &NRPRG(exception_filters) TSRMLS_CC);
  } else if (NULL == nr_php_get_return_value(NR_EXECUTE_ORIG_ARGS)) {
    /*
     * Having no return value (and not being an exception handler) indicates
     * that this segment had an uncaught exception. We want to add that
     * exception to the segment.
     */
    zval exception;
    ZVAL_OBJ(&exception, EG(exception));
    nr_status_t status = nr_php_error_record_exception_segment(
      NRPRG(txn), &exception,
      &NRPRG(exception_filters));

    if (NR_FAILURE == status) {
      nrl_verbosedebug(NRL_AGENT, "%s: unable to record exception on segment",
                       __func__);
    }
  }

  /*
   * Stop the segment time now so we don't add our additional processing on to
   * the segment's time.
   */
  segment->stop_time = nr_txn_now_rel(NRPRG(txn));

  /*
   * Check if we have special instrumentation for this function or if the user
   * has specifically requested it.
   */

  if (NULL != wraprec) {
    /*
     * This is the case for specifically requested custom instrumentation.
     */
    create_metric = wraprec->create_metric;

    zcaught = nr_zend_call_orig_execute_special(wraprec, segment,
                                              NR_EXECUTE_ORIG_ARGS);
    if (nrunlikely(zcaught)) {
      zend_bailout();
    }
  } else if (!NRINI(tt_detail) || !(NR_OP_ARRAY->function_name)) {
    /*
     * If there is no custom instrumentation and tt detail is not more than 0,
     * do not record the segment
     */
    nr_segment_discard(&segment);
    return;
  }
  /*
   * During nr_zend_call_orig_execute_special, the transaction may have been
   * ended and/or a new transaction may have started.  To detect this, we
   * compare the currently active transaction's start time with the transaction
   * start time we saved before.
   *
   * Just comparing the transaction pointer is not enough, as a newly
   * started transaction might actually obtain the same address as a
   * transaction freed before.
   */
  if (nrunlikely(nr_txn_start_time(NRPRG(txn)) != txn_start_time)) {
    nrl_verbosedebug(NRL_AGENT,
                     "%s txn ended and/or started while in a wrapped function",
                     __func__);

    return;
  }

  /*
   * Reassign segment to the current segment, as some before/after wraprecs
   * start and then stop a segment. If that happened, we want to ensure we
   * get the now-current segment
   */
  segment = nr_txn_get_current_segment(NRPRG(txn), NULL);
  nr_php_execute_metadata_init(&metadata, NR_OP_ARRAY);
  nr_php_execute_segment_end(segment, &metadata, create_metric);
  nr_php_execute_metadata_release(&metadata);
  return;
}

void nr_php_observer_fcall_begin(zend_execute_data* execute_data) {
  /*
   * Instrument the function.
   * This and any other needed helper functions will replace:
   * nr_php_execute_enabled
   * nr_php_execute
   * nr_php_execute_show
   */
  zval* func_return_value = NULL;
  if (nrunlikely(NULL == execute_data)) {
    return;
  }

  NRPRG(php_cur_stack_depth) += 1;

  if ((0 < ((int)NRINI(max_nesting_level)))
      && (NRPRG(php_cur_stack_depth) >= (int)NRINI(max_nesting_level))) {
    nr_php_max_nesting_level_reached();
  }

  if (nrunlikely(0 == nr_php_recording())) {
    return;
  }

  int show_executes = NR_PHP_PROCESS_GLOBALS(special_flags).show_executes;

  if (nrunlikely(show_executes)) {
    nrl_verbosedebug(NRL_AGENT,
                     "Stack depth: %d after OAPI function beginning via %s",
                     NRPRG(php_cur_stack_depth), __func__);
    nr_php_show_exec(NR_EXECUTE_ORIG_ARGS);
  }
  nr_php_instrument_func_begin(NR_EXECUTE_ORIG_ARGS);

  return;
}

void nr_php_observer_fcall_end(zend_execute_data* execute_data,
                               zval* func_return_value) {
  /*
   * Instrument the function.
   * This and any other needed helper functions will replace:
   * nr_php_execute_enabled
   * nr_php_execute
   * nr_php_execute_show
   */
  if (nrunlikely(NULL == execute_data)) {
    return;
  }

  if (nrlikely(1 == nr_php_recording())) {
    int show_executes_return
        = NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_returns;

    if (nrunlikely(show_executes_return)) {
      nrl_verbosedebug(NRL_AGENT,
                       "Stack depth: %d before OAPI function exiting via %s",
                       NRPRG(php_cur_stack_depth), __func__);
      nr_php_show_exec_return(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    }

    nr_php_instrument_func_end(NR_EXECUTE_ORIG_ARGS);
  }

  NRPRG(php_cur_stack_depth) -= 1;

  return;
}

#endif
