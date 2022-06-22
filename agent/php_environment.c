/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_environment.h"
#include "php_globals.h"
#include "php_hash.h"
#include "util_logging.h"
#include "util_object.h"
#include "util_strings.h"
#include "util_syscalls.h"
#include "util_system.h"
#include "util_text.h"

#define MAX_PLUGIN_NAME_LEN 128

void nr_php_parse_rocket_assignment_list(char* s,
                                         size_t len,
                                         nrobj_t* kv_hash) {
  const char* key = NULL;
  const char* value = NULL;
  char* smax = s + len;
  int state = 0;

  if (NULL == s) {
    return;
  }

  if (NULL == kv_hash) {
    return;
  }

  while (s < smax) {
    switch (state) {
      case 0: /* looking for a \n */
        if ('\n' == *s) {
          state = 1;
        }
        s++;
        break;

      case 1: /* start key */
        key = s;
        state = 2;
        if ('\n' == *s) {
          state = 1;
        }
        s++;
        break;

      case 2: /* gathering key */
        if ('=' == *s) {
          state = 3;
          if (' ' == s[-1]) {
            s[-1] = 0;
          }
        }
        if ('\n' == *s) {
          state = 1;
        }
        s++;
        break;

      case 3: /* end of key "=" */
        s[-1] = 0;
        s++; /* ">" */
        s++; /* " " */
        state = 4;
        break;

      case 4: /* start value */
        value = s;
        state = 5;
        if ('\n' == *s) {
          state = 6;
        }
        s++;
        break;

      case 5: /* gathering value */
        if ('\n' == *s) {
          state = 6;
        } else {
          s++;
        }
        break;

      case 6: /* end of value "\n" */
        *s = 0;
        nro_set_hash_string(kv_hash, key, value);
        state = 1;
        s++;
        break;
    }
  }
}

static void nr_php_add_zend_extension_to_hash(zend_extension* ext,
                                              void* plugins TSRMLS_DC) {
  char buf[(MAX_PLUGIN_NAME_LEN * 2) + 4];
  int n = nr_strlen(ext->name);
  int m = 0;

  NR_UNUSED_TSRMLS;

  if (ext->version) {
    m = nr_strlen(ext->version);
  }

  n = n > MAX_PLUGIN_NAME_LEN ? MAX_PLUGIN_NAME_LEN - 1 : n;
  nr_strxcpy(buf, ext->name, n);
  m = m > MAX_PLUGIN_NAME_LEN ? MAX_PLUGIN_NAME_LEN - 1 : m;
  if (ext->version) {
    char* p = &(buf[n]);

    *p = '(';
    p++;
    nr_strxcpy(p, ext->version, m);
    p += m;
    *p = ')';
    p++;
    *p = '\0';
  }

  nro_set_array_string((nrobj_t*)plugins, 0, buf);
}

static int nr_php_add_dynamic_module_to_hash(zend_module_entry* ext,
                                             nrobj_t* plugins,
                                             zend_hash_key* key NRUNUSED
                                                 TSRMLS_DC) {
  char buf[(MAX_PLUGIN_NAME_LEN * 2) + 4];
  int n = nr_strlen(ext->name);
  int m = 0;

  NR_UNUSED_TSRMLS;

  if (ext->version) {
    m = nr_strlen(ext->version);
  }

  n = n > MAX_PLUGIN_NAME_LEN ? MAX_PLUGIN_NAME_LEN - 1 : n;
  nr_strxcpy(buf, ext->name, n);
  m = m > MAX_PLUGIN_NAME_LEN ? MAX_PLUGIN_NAME_LEN - 1 : m;
  if (ext->version) {
    char* p = &(buf[n]);

    *p = '(';
    p++;
    nr_strxcpy(p, ext->version, m);
    p += m;
    *p = ')';
    p++;
    *p = '\0';
  }

  nro_set_array_string(plugins, 0, buf);
  return ZEND_HASH_APPLY_KEEP;
}

static void nr_php_gather_dynamic_modules(nrobj_t* env TSRMLS_DC) {
  nrobj_t* plugins = nro_new(NR_OBJECT_ARRAY);

  zend_llist_apply_with_argument(
      &zend_extensions,
      (llist_apply_with_arg_func_t)nr_php_add_zend_extension_to_hash,
      plugins TSRMLS_CC);

  nr_php_zend_hash_ptr_apply(
      &module_registry, (nr_php_ptr_apply_t)nr_php_add_dynamic_module_to_hash,
      plugins TSRMLS_CC);

  nro_set_hash(env, "Plugin List", plugins);

  nro_delete(plugins);
}

static void call_phpinfo(TSRMLS_D) {
  int save_sapi_flag = sapi_module.phpinfo_as_text;

  sapi_module.phpinfo_as_text = 1; /* Force the output to the text format. */
  php_print_info((PHP_INFO_GENERAL)TSRMLS_CC);
  sapi_module.phpinfo_as_text = save_sapi_flag;
}

#if ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO
/*
 * PHP's output system was rewritten in PHP 5.4. Among the many new
 * capabilities, internal output handlers can register an opaque pointer that
 * will be provided on each output operation.
 *
 * We'll use this to register a buffer, and have phpinfo_output_handler() pump
 * output data into that buffer and then swallow it so that other output
 * handlers never see any data.
 */

static int phpinfo_output_handler(void** buf_ptr, php_output_context* ctx) {
  nrbuf_t* buf;

  if ((NULL == buf_ptr) || (NULL == *buf_ptr)) {
    nrl_verbosedebug(NRL_AGENT, "%s: invalid buffer pointer", __func__);
    return FAILURE;
  }
  buf = *((nrbuf_t**)buf_ptr);

  if (NULL == ctx) {
    nrl_verbosedebug(NRL_AGENT, "%s: invalid context", __func__);
    return FAILURE;
  }

  /*
   * Although we never expect a clean operation, let's handle it just in case.
   */
  if (PHP_OUTPUT_HANDLER_CLEAN & ctx->op) {
    nr_buffer_reset(buf);
    return SUCCESS;
  }

  /*
   * Check if there's actually input data. It's not an error to get a context
   * which doesn't use the input.
   */
  if (ctx->in.used) {
    /*
     * Add input data to the buffer.
     */
    nr_buffer_add(buf, ctx->in.data, ctx->in.used);

    /*
     * Indicate that we have no data to give to the next output handler.
     */
    ctx->out.used = 0;
    ctx->out.data = NULL;
  }

  return SUCCESS;
}

static void nr_php_gather_php_information(nrobj_t* env TSRMLS_DC) {
  nrbuf_t* buf = nr_buffer_create(65536, 0);
  php_output_handler* handler;

  handler = php_output_handler_create_internal(
      NR_PSTR("New Relic phpinfo"), phpinfo_output_handler, 4096,
      PHP_OUTPUT_HANDLER_STDFLAGS TSRMLS_CC);

  /*
   * Although there's no way current versions of PHP can return NULL from
   * php_output_handler_create_internal(), we'll still check just in case. We
   * don't want to end up accidentally spewing phpinfo() output to the user.
   */
  if (NULL == handler) {
    nrl_verbosedebug(NRL_AGENT, "%s: unexpected NULL handler", __func__);
    goto end;
  }

  php_output_handler_set_context(handler, buf, NULL TSRMLS_CC);
  php_output_handler_start(handler TSRMLS_CC);
  call_phpinfo(TSRMLS_C);

  /*
   * Note that php_output_discard() calls php_output_handler_free() internally.
   * This means two things: firstly, we don't need to call it ourselves when
   * cleaning up, and secondly, we CANNOT use handler after this function.
   */
  php_output_discard(TSRMLS_C);
  handler = NULL;

  nr_php_parse_rocket_assignment_list(nr_remove_const(nr_buffer_cptr(buf)),
                                      nr_buffer_len(buf), env);

end:
  nr_buffer_destroy(&buf);
}
#else
static void nr_php_gather_php_information(nrobj_t* env TSRMLS_DC) {
  zval* output_handler = NULL;
  long chunk_size = 0;
  zend_bool erase = 1;
  zval* tmp_obj = NULL;

  if (FAILURE
      == php_start_ob_buffer(output_handler, chunk_size, erase TSRMLS_CC)) {
    /* don't call phpinfo() if we can't buffer because otherwise we're
     * going to dump into the user's page.
     */
    return;
  }

  call_phpinfo(TSRMLS_C);

  tmp_obj = nr_php_zval_alloc();

  php_ob_get_buffer(tmp_obj TSRMLS_CC);
  php_end_ob_buffer(0, 0 TSRMLS_CC);

  nr_php_parse_rocket_assignment_list(Z_STRVAL_P(tmp_obj), Z_STRLEN_P(tmp_obj),
                                      env);

  nr_php_zval_free(&tmp_obj);
}
#endif /* PHP >= 5.4 */

static void nr_php_gather_machine_information(nrobj_t* env) {
  const char* dyno_value = NULL;
  char final[2048];
  nr_system_t* sys = nr_system_get_system_information();

  if (NULL == sys) {
    return;
  }

  final[0] = 0;
  snprintf(final, sizeof(final), "%s %s %s %s %s", NRBLANKSTR(sys->sysname),
           NRBLANKSTR(sys->nodename), NRBLANKSTR(sys->release),
           NRBLANKSTR(sys->version), NRBLANKSTR(sys->machine));

  nro_set_hash_string(env, "OS version", final);

  /*
   * Advertise that we are running PHP on Heroku if DYNO env var is
   * present *and* so is the '/app/.heroku/php' directory. This detection
   * is here to increase Supportability: so we have an additional clue
   * that the agent is on Heroku.
   */
  dyno_value = getenv("DYNO");
  if (!nr_strempty(dyno_value) && (0 == nr_access("/app/.heroku/php", F_OK))) {
    nro_set_hash_string(env, "Heroku", "yes");
  }

  nr_system_destroy(&sys);
}

static void nr_php_gather_dispatcher_information(nrobj_t* env) {
  char dstring[512];
  char* p;

  nr_strcpy(dstring, NR_PHP_PROCESS_GLOBALS(php_version));

  p = nr_strchr(dstring, '-');
  if (NULL != p) {
    *p = 0;
  }
  p = nr_strchr(dstring, '/');
  if (NULL != p) {
    *p = 0;
  }

#ifdef ZTS
  nr_strcat(dstring, "Z");
#endif
  nr_strcat(dstring, "+");

  if (0 == nr_strcmp(sapi_module.name, "apache2handler")) {
    nr_strcat(dstring, "a2h");
  } else if (0 == nr_strcmp(sapi_module.name, "apache2filter")) {
    nr_strcat(dstring, "a2f");
  } else {
    nr_strcat(dstring, sapi_module.name);
  }

  if (NR_PHP_PROCESS_GLOBALS(is_apache)) {
    char tstr[512];

    snprintf(tstr, sizeof(tstr), "%d.%d.%d %s",
             NR_PHP_PROCESS_GLOBALS(apache_major),
             NR_PHP_PROCESS_GLOBALS(apache_minor),
             NR_PHP_PROCESS_GLOBALS(apache_patch),
             NR_PHP_PROCESS_GLOBALS(apache_add));
    nro_set_hash_string(env, "Apache Version", tstr);

    snprintf(tstr, sizeof(tstr), "(%d.%d.%d%s%s)",
             NR_PHP_PROCESS_GLOBALS(apache_major),
             NR_PHP_PROCESS_GLOBALS(apache_minor),
             NR_PHP_PROCESS_GLOBALS(apache_patch),
             NR_PHP_PROCESS_GLOBALS(apache_add),
             0 == NR_PHP_PROCESS_GLOBALS(apache_threaded) ? "" : "W");
    nr_strcat(dstring, tstr);
  }

  nro_set_hash_string(env, "Dispatcher", dstring);
}
void nr_php_process_environment_variable(const char* prefix,
                                         const char* key,
                                         const char* value,
                                         nrobj_t* kv_hash) {
  if ((NULL == prefix) || (NULL == kv_hash) || (NULL == key)) {
    return;
  }

  if (nr_strlen(prefix) >= nr_strlen(key)) {
    return;
  }

  if (0 == nr_strncmp(key, prefix, nr_strlen(prefix))) {
    nro_set_hash_string(kv_hash, key, value);
  }
}
static void nr_php_get_environment_variables() {
  nrobj_t* parsed_key_val = NULL;
  /*
   * `environ` works for non-windows machines.
   * Otherwise, we'd need to use *__p__environ() as well.
   */
  extern char** environ;

  /*
   * Initialize the metadata hash.  If there aren't any variables, we still need
   * to send the empty hash.
   */
  NR_PHP_PROCESS_GLOBALS(metadata) = nro_new_hash();

  /*
   * If we are unable to get the environment don't try to parse it.
   */
  if (NULL == environ) {
    nrl_warning(NRL_AGENT, "%s: Unable to access environmental variables.",
                __func__);
    return;
  }

  /*
   * Iterate through the environment variables, searching for a single key or
   * a set of keys with a prefix that the agent will use.
   */
  for (size_t i = 0; environ[i] != NULL; i++) {
    parsed_key_val = nr_strsplit(environ[i], "=", 0);
    if ((NULL == parsed_key_val) || (2 != nro_getsize(parsed_key_val))) {
      nrl_verbosedebug(NRL_AGENT,
                       "%s: Skipping malformed environmental variable %s",
                       __func__, environ[i]);
    } else {
      const char* key = nro_get_array_string(parsed_key_val, 1, NULL);
      const char* value = nro_get_array_string(parsed_key_val, 2, NULL);
      nr_php_process_environment_variable(NR_METADATA_PREFIX, key, value,
                                          NR_PHP_PROCESS_GLOBALS(metadata));
    }
    nro_delete(parsed_key_val);
  }
}

nrobj_t* nr_php_get_environment(TSRMLS_D) {
  nrobj_t* env;

  env = nro_new(NR_OBJECT_HASH);
  nr_php_gather_php_information(env TSRMLS_CC);
  nr_php_gather_machine_information(env);
  nr_php_gather_dynamic_modules(env TSRMLS_CC);
  nr_php_gather_dispatcher_information(env);
  nr_php_get_environment_variables();

  return env;
}
