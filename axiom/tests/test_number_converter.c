/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <limits.h>
#include <locale.h>
#include <stdio.h>

#include "util_number_converter.h"
#include "util_strings.h"
#include "util_threads.h"

#include "tlib_main.h"

#define test_format_double(l, v, e) \
  test_format_double_worker(l, v, e, __FILE__, __LINE__)
#define test_scan_double(l, s, e, n) \
  test_scan_double_worker(l, s, e, n, __FILE__, __LINE__)

static void test_scan_double_worker(const char* locale,
                                    const char* subject,
                                    double expect,
                                    const char* expect_end,
                                    const char* file,
                                    int line) {
  double actual;
  char* end = 0;

  actual = nr_strtod(subject, &end);
  test_pass_if_true("nr_strtod", actual == expect,
                    "locale=%s actual=%g expect=%g", locale, actual, expect);
  if (0 != expect_end && 0 != end) {
    test_pass_if_true("nr_strtod", 0 == nr_strcmp(end, expect_end),
                      "locale=%s end=%s expect_end=%s", locale, end,
                      expect_end);
  }
}

static void test_format_double_worker(const char* locale,
                                      double val,
                                      const char* expect,
                                      const char* file,
                                      int line) {
  int written;
  int expected_len;
  char actual[BUFSIZ];

  written = nr_double_to_str(actual, sizeof(actual), val);
  /*
   * Do a case insensitive comparison so we can handle Inf and NaN variations.
   */
  test_pass_if_true("nr_double_to_str", 0 == nr_stricmp(actual, expect),
                    "locale=%s actual=%s expect=%s", locale, actual, expect);
  expected_len = nr_strlen(expect);
  test_pass_if_true("nr_double_to_str", written == expected_len,
                    "written=%d expected_len=%d", written, expected_len);
}

static void test_format_doubles_buffering(void) {
  char actual[BUFSIZ];
  double val = 256.0;
  int actual_nwritten = 0;

  actual[0] = 'a';
  actual[1] = '\0';

  actual_nwritten = nr_double_to_str(0, 0, val);
  tlib_pass_if_str_equal("null buffer", actual, "a");
  tlib_pass_if_int_equal("null buffer", actual_nwritten, -1);

  actual_nwritten = nr_double_to_str(actual, 0, val);
  tlib_pass_if_str_equal("0 length buffer", actual, "a");
  tlib_pass_if_int_equal("0 length buffer", actual_nwritten, -1);

  actual_nwritten = nr_double_to_str(actual, 1, val);
  tlib_pass_if_str_equal("1 length buffer", actual, "");
  tlib_pass_if_int_equal("1 length buffer", actual_nwritten, 0);

  actual_nwritten = nr_double_to_str(actual, 2, val);
  tlib_pass_if_str_equal("2 length buffer", actual, "2");
  tlib_pass_if_int_equal("2 length buffer", actual_nwritten, 1);

  actual_nwritten = nr_double_to_str(actual, 20, val);
  tlib_pass_if_str_equal("typical usage", actual, "256.00000");
  tlib_pass_if_int_equal("typical usage", actual_nwritten, 9);
}

static void test_scan_doubles(const char* locale) {
  test_scan_double(locale, 0, 0.0, 0); /* special test of null buffer */

  test_scan_double(locale, "XX", 0.0, "XX");
  test_scan_double(locale, "2.0", 2.0, "");
  test_scan_double(locale, "2.0,", 2.0, ",");
  test_scan_double(locale, "2.0,000", 2.0, ",000");
  test_scan_double(locale, "2.0 000", 2.0, " 000");
  test_scan_double(locale, "65536.0", 65536.0, "");
  test_scan_double(locale, "2097152.0", 2097152.0, "");
  test_scan_double(locale, "65536.0.999", 65536.0, ".999");
  test_scan_double(locale, "65536.0,999", 65536.0, ",999");
  test_scan_double(locale, "65536,0,999", 65536.0,
                   ",0,999"); /* hits corner case */
  test_scan_double(locale, "65536,0.999", 65536.0,
                   ",0.999"); /* hits corner case */

  test_scan_double(locale, ",0.999", 0.0, ",0.999");
  test_scan_double(locale, ".1.999", 0.1, ".999");
  test_scan_double(locale, ".1e2.999", 0.1e2, ".999");

  test_scan_double(locale, "    \t\r\n2.0", 2.0, "");
  /*
   * probably undefined what strtod returns when first non space char isn't
   * legal
   */
  test_scan_double(locale, ";2.0", 0.0, ";2.0");
  test_scan_double(locale, " ;2.0", 0.0, " ;2.0");
  test_scan_double(locale, "2.0,", 2.0, ",");
  test_scan_double(locale, "-2.0,", -2.0, ",");
  test_scan_double(locale, "-2.0e+00,", -2.0, ",");
  test_scan_double(locale, "-2.0e-00,", -2.0, ",");
  test_scan_double(locale, "2.00000,", 2.0, ",");
  test_scan_double(locale, "2.00000 ,", 2.0, " ,");
  test_scan_double(locale, "2.00000,,", 2.0, ",,");
  test_scan_double(locale, "2.00000.,", 2.0, ".,");
  test_scan_double(locale, "2.00000[,", 2.0, "[,");
}

static void test_format_doubles(const char* locale) {
  double nan;
  uint64_t int_nan;

  /*
   * Some implementations of the C library will print a Nan with the sign bit
   * set as a negative Nan.  Others won't. So here we just smash the sign bit
   * away.
   */
  nan = 0.0 / 0.0;

#if defined(__clang__) \
    || ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5)))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
  int_nan = (*(uint64_t*)&nan) & ~(1LL << 63);
  nan = *(double*)&int_nan;
#if defined(__clang__) \
    || ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5)))
#pragma GCC diagnostic pop
#endif

  test_format_double(locale, -0.0, "-0.00000");
  test_format_double(locale, 3.0, "3.00000");
  test_format_double(locale, 65536.0, "65536.00000");
  test_format_double(locale, 2097152.0, "2097152.00000");
  test_format_double(locale, -3.0, "-3.00000");

  test_format_double(locale, 1.0 / 0.0, "inf");
  test_format_double(locale, nan, "nan");
}

/*
 * Alas, the locale is global state, and so we can't run this test
 * without locking the locale.
 */
static nrthread_mutex_t locale_lock = NRTHREAD_MUTEX_INITIALIZER;

static void test_format_doubles_locale(const char* new_locale) {
  nrt_mutex_lock(&locale_lock); /* { */
  setlocale(LC_NUMERIC, new_locale);

  test_format_doubles(new_locale);
  test_scan_doubles(new_locale);

  setlocale(LC_NUMERIC, "C");
  nrt_mutex_unlock(&locale_lock); /* } */
}

/*
 * This code was developed on macosx which has many more locales pre-loaded
 * than ubuntu appears to have.
 */
static void test_format_doubles_locales(void) {
  test_format_doubles_locale("");  /* native */
  test_format_doubles_locale("C"); /* native */
  test_format_doubles_locale("POSIX");
  test_format_doubles_locale("en_EN"); /* English */
  test_format_doubles_locale(
      "de_DE"); /* German (uses ',' for decimal radix separator) */
  test_format_doubles_locale("fr_FR"); /* French */
  test_format_doubles_locale("zh_CN"); /* China */
  test_format_doubles_locale("zh_TW"); /* Taiwan */
  test_format_doubles_locale("ja_JP"); /* Japan */
  test_format_doubles_locale("ko_KR"); /* Korean */
  test_format_doubles_locale("th_TH"); /* Thai (western digits) */
  test_format_doubles_locale("pt_BR"); /* Brazilian Portuguese */
  test_format_doubles_locale("ar_SA"); /* Saudia Arabia */
  test_format_doubles_locale("ru_RU"); /* Russia */
}

static void test_format_ints(void) {
  char actual[32];
  char expected[32];

  nr_itoa(actual, sizeof(actual), 0);
  tlib_pass_if_str_equal(__func__, "0", actual);

  nr_itoa(actual, sizeof(actual), -1);
  tlib_pass_if_str_equal(__func__, "-1", actual);

  nr_itoa(actual, sizeof(actual), 1);
  tlib_pass_if_str_equal(__func__, "1", actual);

  nr_itoa(actual, sizeof(actual), -999);
  tlib_pass_if_str_equal(__func__, "-999", actual);

  nr_itoa(actual, sizeof(actual), 999);
  tlib_pass_if_str_equal(__func__, "999", actual);

  nr_itoa(actual, sizeof(actual), 12345678);
  tlib_pass_if_str_equal(__func__, "12345678", actual);

  snprintf(expected, sizeof(expected), "%d", INT_MIN);
  nr_itoa(actual, sizeof(actual), INT_MIN);
  tlib_pass_if_str_equal(__func__, expected, actual);

  snprintf(expected, sizeof(expected), "%d", INT_MAX);
  nr_itoa(actual, sizeof(actual), INT_MAX);
  tlib_pass_if_str_equal(__func__, expected, actual);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_format_doubles_buffering();
  test_format_doubles_locales();
  test_format_ints();
}
