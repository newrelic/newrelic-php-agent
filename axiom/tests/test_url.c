/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "util_memory.h"
#include "util_strings.h"
#include "util_text.h"
#include "util_url.h"

#include "tlib_main.h"

#define STRLEN(X) (X), (sizeof(X) - 1)

#define clean_testcase(...) clean_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void clean_testcase_fn(const char* testname,
                              const char* expected,
                              const char* in,
                              const char* file,
                              int line) {
  int inlen = nr_strlen(in);
  char* rv = nr_url_clean(in, inlen);

  test_pass_if_true(testname, 0 == nr_strcmp(rv, expected), "rv=%s expected=%s",
                    NRSAFESTR(rv), NRSAFESTR(expected));
  nr_free(rv);
}

static void test_clean_normal(void) {
  char* json = 0;
  nrobj_t* array = 0;
  nrotype_t otype;
  int i;

#define URL_CLEAN_TESTS_FILE CROSS_AGENT_TESTS_DIR "/url_clean.json"
  json = nr_read_file_contents(URL_CLEAN_TESTS_FILE, 10 * 1000 * 1000);
  tlib_pass_if_true("tests valid", 0 != json, "json=%p", json);

  if (0 == json) {
    return;
  }

  array = nro_create_from_json(json);
  tlib_pass_if_true("tests valid", 0 != array, "array=%p", array);
  otype = nro_type(array);
  tlib_pass_if_true("tests valid", NR_OBJECT_ARRAY == otype, "otype=%d",
                    (int)otype);

  if (array && (NR_OBJECT_ARRAY == nro_type(array))) {
    for (i = 1; i <= nro_getsize(array); i++) {
      const nrobj_t* hash = nro_get_array_hash(array, i, 0);
      const char* testname = nro_get_hash_string(hash, "testname", 0);
      const char* input = nro_get_hash_string(hash, "input", 0);
      const char* expected = nro_get_hash_string(hash, "expected", 0);

      tlib_pass_if_true("tests valid", 0 != input, "input=%p", input);
      tlib_pass_if_true("tests valid", 0 != expected, "expected=%p", expected);

      if (input && expected) {
        clean_testcase(testname ? testname : input, expected, input);
      }
    }
  }

  nro_delete(array);
  nr_free(json);
}

static void test_clean_bad_params(void) {
  char* rv;

  /*
   * Test : Bad Parameters
   */
  rv = nr_url_clean(0, 9);
  tlib_pass_if_true("null url", 0 == rv, "rv=%p", rv);

  rv = nr_url_clean("domain.com", 0);
  tlib_pass_if_true("zero length", 0 == rv, "rv=%p", rv);

  rv = nr_url_clean("domain.com", -1);
  tlib_pass_if_true("negative length", 0 == rv, "rv=%p", rv);

  rv = nr_url_clean("", 9);
  tlib_pass_if_true("empty url", 0 == rv, "rv=%p", rv);

  /*
   * Test : Malformed URLs
   *
   * nr_url_clean is not designed to return 0 on every conceivable erroneous
   * URL. Instead these tests are meant to ensure that our parser does not
   * crash on weird input.
   */
  clean_testcase("starts with ;", 0, ";zap.com");
  clean_testcase("ends with @", "", "zap.com@");
  clean_testcase("starts with @", "zap.com", "@zap.com");
  clean_testcase("multiple @", "zap.com", "zap@zap@zap.com");

  /*
   * Test : Early Null Terminator
   */
  rv = nr_url_clean(STRLEN("domain.com\0/should/not/appear"));
  tlib_pass_if_true("early terminator", 0 == nr_strcmp("domain.com", rv),
                    "rv=%s", NRSAFESTR(rv));
  nr_free(rv);

  /*
   * Test : urllen Obeyed
   */
  rv = nr_url_clean("domain.com/should/not/appear", 10);
  tlib_pass_if_true("urllen obeyed", 0 == nr_strcmp(rv, "domain.com"), "rv=%s",
                    rv);
  nr_free(rv);
}

#define extract_testcase(...) \
  test_url_extract_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_url_extract_testcase_fn(const char* expected,
                                         const char* in,
                                         const char* file,
                                         int line) {
  int len = 0;
  int inlen = nr_strlen(in);
  const char* rv = nr_url_extract_domain(in, inlen, &len);

  if (expected) {
    /*
     * Success is expected.
     */
    int expectedlen = nr_strlen(expected);

    test_pass_if_true(in,
                      (0 != rv) && (expectedlen == len)
                          && (0 == nr_strncmp(rv, expected, expectedlen)),
                      "expected=%s expectedlen=%d len=%d rv=%.*s", expected,
                      expectedlen, len, len, NRSAFESTR(rv));
  } else {
    /*
     * Failure is expected.
     */
    test_pass_if_true(in, (0 == rv) && (-1 == len), "len=%d rv=%.*s", len, len,
                      NRSAFESTR(rv));
  }
}

static void test_extract_domain_normal(void) {
  char* json = 0;
  nrobj_t* array = 0;
  nrotype_t otype;
  int i;

#define DOMAIN_EXTRACTION_TESTS_FILE \
  CROSS_AGENT_TESTS_DIR "/url_domain_extraction.json"
  json = nr_read_file_contents(DOMAIN_EXTRACTION_TESTS_FILE, 10 * 1000 * 1000);
  tlib_pass_if_true("tests valid", 0 != json, "json=%p", json);

  if (0 == json) {
    return;
  }

  array = nro_create_from_json(json);
  tlib_pass_if_true("tests valid", 0 != array, "array=%p", array);
  otype = nro_type(array);
  tlib_pass_if_true("tests valid", NR_OBJECT_ARRAY == otype, "otype=%d",
                    (int)otype);

  if (array && (NR_OBJECT_ARRAY == nro_type(array))) {
    for (i = 1; i <= nro_getsize(array); i++) {
      const nrobj_t* hash = nro_get_array_hash(array, i, 0);
      const char* input = nro_get_hash_string(hash, "input", 0);
      const char* expected = nro_get_hash_string(hash, "expected", 0);

      tlib_pass_if_true("tests valid", 0 != input, "input=%p", input);
      tlib_pass_if_true("tests valid", 0 != expected, "expected=%p", expected);

      if (input && expected) {
        extract_testcase(expected, input);
      }
    }
  }

  nro_delete(array);
  nr_free(json);
}

static void test_extract_domain_bad_params(void) {
  const char* rv;
  int dnlen;

  /*
   * Test : Bad Parameters
   */
  rv = nr_url_extract_domain(0, 0, 0);
  tlib_pass_if_true("zero params", 0 == rv, "rv=%p", rv);

  rv = nr_url_extract_domain("a", 1, 0);
  tlib_pass_if_true("zero dnlen", 0 == rv, "rv=%p", rv);

  rv = nr_url_extract_domain(0, 1, &dnlen);
  tlib_pass_if_true("zero url", (0 == rv) && (-1 == dnlen), "rv=%p dnlen=%d",
                    rv, dnlen);

  rv = nr_url_extract_domain("", 1, &dnlen);
  tlib_pass_if_true("empty url", (0 == rv) && (-1 == dnlen), "rv=%p dnlen=%d",
                    rv, dnlen);

  rv = nr_url_extract_domain("a", 0, &dnlen);
  tlib_pass_if_true("zero len", (0 == rv) && (-1 == dnlen), "rv=%p dnlen=%d",
                    rv, dnlen);

  rv = nr_url_extract_domain("a", -1, &dnlen);
  tlib_pass_if_true("negative len", (0 == rv) && (-1 == dnlen),
                    "rv=%p dnlen=%d", rv, dnlen);

  /*
   * Test : Malformed URLs
   *
   * Since the scheme:// is optional, it is hard to determine whether or not
   * the url is 'valid'. Therefore, here we are mostly interested that our
   * parser does not blow up.
   */
  extract_testcase("p", "p:/d.e.f/a/b");
  extract_testcase("a", "a:b:c//whatever.com");
  extract_testcase("zap", "zap:/bar//bing");
  extract_testcase("zap", "@zap");
  extract_testcase("zap", "zap?");
  extract_testcase("zap", "zap;");
  extract_testcase("zap", "zap?@@@@@@");
  extract_testcase("zap", "zap;://://://://");
  extract_testcase("zap", "zap#@://@://@://");

  extract_testcase(0, "@");
  extract_testcase(0, "foo@");
  extract_testcase(0, "/");
  extract_testcase(0, "//");
  extract_testcase(0, ":");
  extract_testcase(0, "://");
  extract_testcase(0, "://://");
  extract_testcase(0, "@:");
  extract_testcase(0, "zap@@@@@@?");
  extract_testcase(0, "@://");
  extract_testcase(0, "/@/");
  extract_testcase(0, "x@y@z");
  extract_testcase(0, "x://y://z");
  extract_testcase(0, "x@y://z");
  extract_testcase(0, "x@y://z@");

  /*
   * Test : Early Null Terminator
   */
  rv = nr_url_extract_domain(STRLEN("domain.com\0/should/not/appear"), &dnlen);
  tlib_pass_if_true(
      "early terminator",
      (0 != rv) && (10 == dnlen) && (0 == nr_strncmp(rv, "domain.com", 10)),
      "dnlen=%d rv=%.*s", dnlen, dnlen, NRSAFESTR(rv));

  /*
   * Test : urllen Obeyed
   */
  rv = nr_url_extract_domain("domainNOOOOOOOO", 6, &dnlen);
  tlib_pass_if_true(
      "urllen obeyed",
      (0 != rv) && (6 == dnlen) && (0 == nr_strncmp(rv, "domain", 6)),
      "dnlen=%d rv=%.*s", dnlen, dnlen, NRSAFESTR(rv));
}

#define proxy_clean_testcase(...) \
  test_url_proxy_clean_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_url_proxy_clean_testcase_fn(const char* expected,
                                             const char* in,
                                             const char* file,
                                             int line) {
  char* rv = nr_url_proxy_clean(in);

  if (expected) {
    /*
     * Success is expected.
     */
    test_pass_if_true(in, 0 == nr_strcmp(rv, expected), "expected=%s rv=%s",
                      expected, NRSAFESTR(rv));
  } else {
    /*
     * Failure is expected.
     */
    test_pass_if_true(in, (0 == rv), "rv=%s", NRSAFESTR(rv));
  }

  nr_free(rv);
}

static void test_proxy_clean(void) {
  proxy_clean_testcase("hostname", "hostname");
  proxy_clean_testcase("hostname:port", "hostname:port");
  proxy_clean_testcase("****@hostname", "user@hostname");
  proxy_clean_testcase("****@hostname:port", "user@hostname:port");
  proxy_clean_testcase("****:****@hostname", "user:password@hostname");
  proxy_clean_testcase("****:****@hostname:port",
                       "user:password@hostname:port");
  proxy_clean_testcase("scheme://hostname", "scheme://hostname");
  proxy_clean_testcase("scheme://hostname:port", "scheme://hostname:port");
  proxy_clean_testcase("scheme://****@hostname", "scheme://user@hostname");
  proxy_clean_testcase("scheme://****@hostname:port",
                       "scheme://user@hostname:port");
  proxy_clean_testcase("scheme://****:****@hostname",
                       "scheme://user:password@hostname");
  proxy_clean_testcase("scheme://****:****@hostname:port",
                       "scheme://user:password@hostname:port");
  proxy_clean_testcase(NULL, NULL);
  proxy_clean_testcase(NULL, "");
  proxy_clean_testcase("****:****@", ":@");
  proxy_clean_testcase("****@:", "@:");
  proxy_clean_testcase("scheme://****:****@", "scheme://:@");
  proxy_clean_testcase("scheme://****@:", "scheme://@:");
  proxy_clean_testcase("scheme://", "scheme://");
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_clean_normal();
  test_clean_bad_params();
  test_extract_domain_normal();
  test_extract_domain_bad_params();
  test_proxy_clean();
}
