/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_main.h"
#include "nr_axiom.h"

#include "util_trie.h"

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

#define TEST_SUFFIX_START "dir"
#define TEST_SUFFIX_ENDING "file-name"
#define TEST_SUFFIX TEST_SUFFIX_START "/" TEST_SUFFIX_ENDING
#define TEST_SUFFIX_UC "DIR/FILE-NAME"
#define TEST_VALUE ((void*)0x1234)
#define TEST_STRING_LC "/srv/" TEST_SUFFIX
#define TEST_STRING_UC "/SRV/" TEST_SUFFIX_UC
#define TEST_STRING_EXT_LC "/srv/" TEST_SUFFIX ".ext"
#define TEST_STRING_EXT_UC "/SRV/" TEST_SUFFIX_UC ".EXT"
#define TEST_EXT_LEN 4

static void test_create_destroy(void) {
  nr_trie_t* trie = nr_trie_create();
  tlib_pass_if_not_null("trie create", trie);
  nr_trie_suffix_add(trie, NR_PSTR("a"), false, (void*)0x1234);
  nr_trie_destroy(&trie);
  tlib_pass_if_null("trie destroy", trie);
}

static void test_case_insensitive_suffix_lookup(void) {
  nr_trie_t* trie = nr_trie_create();
  void* value = NULL;

  nr_trie_suffix_add(NULL, NR_PSTR(TEST_SUFFIX), false, TEST_VALUE);

  value = nr_trie_suffix_lookup(NULL, NR_PSTR("foo"), 0);
  tlib_pass_if_null("looking in non-existing trie", value);

  nr_trie_suffix_add(trie, NR_PSTR(TEST_SUFFIX), false, TEST_VALUE);
  tlib_pass_if_not_null("add suffix", trie);
  value = nr_trie_suffix_lookup(trie, NR_PSTR(TEST_STRING_LC), 0);
  tlib_pass_if_ptr_equal("no skip lookup, lowercase match", TEST_VALUE, value);
  value = nr_trie_suffix_lookup(trie, NR_PSTR(TEST_STRING_UC), 0);
  tlib_pass_if_ptr_equal("no skip lookup, uppercase match", TEST_VALUE, value);
  // clang-format off
  value = nr_trie_suffix_lookup(trie, NR_PSTR(TEST_STRING_EXT_LC), TEST_EXT_LEN);
  tlib_pass_if_ptr_equal("skip last n-chars lookup, lowercase match", TEST_VALUE, value);
  value = nr_trie_suffix_lookup(trie, NR_PSTR(TEST_STRING_EXT_UC), TEST_EXT_LEN);
  tlib_pass_if_ptr_equal("skip last n-chars lookup, uppercase match", TEST_VALUE, value);
  // clang-format on
  value = nr_trie_suffix_lookup(trie, NR_PSTR("foo"), 0);
  tlib_pass_if_null("lookup no match", value);
  value = nr_trie_suffix_lookup(trie, NR_PSTR(TEST_SUFFIX_ENDING), 0);
  tlib_pass_if_null("lookup incomplete suffix", value);

  nr_trie_destroy(&trie);
}

static void test_case_sensitive_suffix_lookup(void) {
  nr_trie_t* trie = nr_trie_create();
  void* value = NULL;

  nr_trie_suffix_add(NULL, NR_PSTR(TEST_SUFFIX), true, TEST_VALUE);

  value = nr_trie_suffix_lookup(NULL, NR_PSTR("foo"), 0);
  tlib_pass_if_null("looking in non-existing trie", value);

  nr_trie_suffix_add(trie, NR_PSTR(TEST_SUFFIX), true, TEST_VALUE);
  tlib_pass_if_not_null("add suffix", trie);
  value = nr_trie_suffix_lookup(trie, NR_PSTR(TEST_STRING_LC), 0);
  tlib_pass_if_ptr_equal("no skip lookup, lowercase match", TEST_VALUE, value);
  value = nr_trie_suffix_lookup(trie, NR_PSTR(TEST_STRING_UC), 0);
  tlib_pass_if_null("no skip lookup, uppercase match", value);
  // clang-format off
  value = nr_trie_suffix_lookup(trie, NR_PSTR(TEST_STRING_EXT_LC), TEST_EXT_LEN);
  tlib_pass_if_ptr_equal("skip last n-chars lookup, lowercase match", TEST_VALUE, value);
  value = nr_trie_suffix_lookup(trie, NR_PSTR(TEST_STRING_EXT_UC), TEST_EXT_LEN);
  tlib_pass_if_null("skip last n-chars lookup, uppercase match",value);
  // clang-format on
  value = nr_trie_suffix_lookup(trie, NR_PSTR("foo"), 0);
  tlib_pass_if_null("lookup no match", value);
  value = nr_trie_suffix_lookup(trie, NR_PSTR(TEST_SUFFIX_ENDING), 0);
  tlib_pass_if_null("lookup incomplete suffix", value);

  nr_trie_destroy(&trie);
}

static char* nr_string_to_uppercase(const char* in) {
  int i;
  char* out;

  out = nr_strdup(in);
  if (0 == out) {
    return 0;
  }

  for (i = 0; in[i]; i++) {
    out[i] = nr_tolower(in[i]);
  }
  return out;
}

static void test_suffix_stress(void) {
  // clang-format off
  struct suffix {
      const char* value; 
      const size_t len;
  } suffixes[] = {
    { NR_PSTR("aws-sdk-php/src/awsclient.php") },
    { NR_PSTR("doctrine/orm/query.php") },
    { NR_PSTR("doctrine/orm/src/query.php") },
    { NR_PSTR("guzzle/http/client.php") },
    { NR_PSTR("hasemitterinterface.php") },
    { NR_PSTR("guzzle/src/functions_include.php") },
    { NR_PSTR("mongodb/src/client.php") },
    { NR_PSTR("phpunit/src/framework/test.php") },
    { NR_PSTR("phpunit/framework/test.php") },
    { NR_PSTR("predis/src/client.php") },
    { NR_PSTR("predis/client.php") },
    { NR_PSTR("zend/http/client.php") },
    { NR_PSTR("laminas-http/src/client.php") },
    { NR_PSTR("aura/framework/system.php") },
    { NR_PSTR("aura/di/src/containerinterface.php") },
    { NR_PSTR("aura/di/src/containerconfiginterface.php") },
    { NR_PSTR("fuel/core/classes/fuel.php") },
    { NR_PSTR("lithium/core/libraries.php") },
    { NR_PSTR("phpbb/request/request.php") },
    { NR_PSTR("phpixie/core/classes/phpixie/pixie.php") },
    { NR_PSTR("phpixie/framework.php") },
    { NR_PSTR("react/event-loop/src/loopinterface.php") },
    { NR_PSTR("injector/silverstripeinjectioncreator.php") },
    { NR_PSTR("silverstripeserviceconfigurationlocator.php") },
    { NR_PSTR("classes/typo3/flow/core/bootstrap.php") },
    { NR_PSTR("typo3/sysext/core/classes/core/bootstrap.php") },
    { NR_PSTR("moodlelib.php") },
    { NR_PSTR("system/expressionengine/config/config.php") },
    { NR_PSTR("expressionengine/boot/boot.php") },
    { NR_PSTR("doku.php") },
    { NR_PSTR("conf/dokuwiki.php") },
    { NR_PSTR("sugarobjects/sugarconfig.php") },
    { NR_PSTR("class/xoopsload.php") },
    { NR_PSTR("e107_handlers/e107_class.php") },
    { NR_PSTR("monolog/logger.php") },
    { NR_PSTR("consolidation/log/src/logger.php") },
    { NR_PSTR("laminas-log/src/logger.php") },
    { NR_PSTR("drupal/component/dependencyinjection/container.php") },
    { NR_PSTR("wp-includes/version.php") }
  }, *value;
  // clang-format on
  const size_t num_suffixes = sizeof(suffixes) / sizeof(suffixes[0]);
  nr_trie_t* trie = nr_trie_create();
  const char* needle;

  for (size_t i = 0; i < num_suffixes; i++) {
    struct suffix* s = &suffixes[i];
    nr_trie_suffix_add(trie, s->value, s->len, false, s);
  }

  needle = "foo";
  value = nr_trie_suffix_lookup(trie, needle, nr_strlen(needle), 0);
  tlib_pass_if_null("lookup no match", value);

  needle = suffixes[0].value;
  value = nr_trie_suffix_lookup(trie, needle, nr_strlen(needle), 0);
  tlib_pass_if_ptr_equal("lookup exact case match", &suffixes[0], value);

  needle = nr_string_to_uppercase(suffixes[num_suffixes - 1].value);
  value = nr_trie_suffix_lookup(trie, needle, nr_strlen(needle), 0);
  tlib_pass_if_ptr_equal("lookup case insensitive match",
                         &suffixes[num_suffixes - 1], value);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
  nr_free(needle);
#pragma GCC diagnostic pop

  nr_trie_destroy(&trie);
}

void test_main(void* p NRUNUSED) {
  test_create_destroy();
  test_case_insensitive_suffix_lookup();
  test_case_sensitive_suffix_lookup();
  test_suffix_stress();
}
