/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <ctype.h>
#include <locale.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "util_memory.h"
#include "util_strings.h"
#include "util_threads.h"

#include "tlib_main.h"

static void filldest(char* dest, int len) {
  int i;

  for (i = 0; i < len; i++) {
    dest[i] = i + 1;
  }
}

static void test_strxcpy(void) {
  char* rp;
  char dest[16];

  rp = nr_strxcpy(0, "abc", 3);
  tlib_pass_if_true("nr_strxcpy to NULL returns NULL", 0 == rp, "rp=%p", rp);

  rp = nr_strxcpy(dest, 0, 1234);
  tlib_pass_if_true("nr_strxcpy from NULL returns dest",
                    (0 != rp) && (rp == dest), "rp=%p dest=%p", rp, dest);

  dest[0] = 'x';
  rp = nr_strxcpy(dest, "abc", 0);
  tlib_pass_if_true("nr_strxcpy of 0 bytes returns dest",
                    (0 != rp) && (rp == dest) && (0 == dest[0]),
                    "rp=%p dest=%p dest[0]=%d", rp, dest, dest[0]);

  filldest(dest, sizeof(dest));
  rp = nr_strxcpy(dest, "abcdef", 3);
  tlib_pass_if_true("simple nr_strxcpy",
                    (0 != rp) && (0 == dest[3]) && (3 == nr_strlen(dest))
                        && (0 == nr_strcmp(dest, "abc")),
                    "rp=%p dest[3]=%d dest='%s'", rp, dest[3], dest);
  tlib_pass_if_true("return pointer is correct", rp == &dest[3],
                    "rp=%p &dest[3]=%p", rp, &dest[3]);
  tlib_pass_if_true("copy did not overwrite", 5 == dest[4], "dest[4]=%d",
                    dest[4]);
}

static void test_strlcpy(void) {
  char* rp;
  char dest[16];

  /*
   * Test bad input.
   */
  rp = nr_strlcpy(0, "abc", sizeof(dest));
  tlib_pass_if_true("nr_strlcpy to NULL returns NULL", 0 == rp, "rp=%p", rp);

  rp = nr_strlcpy(dest, 0, sizeof(dest));
  tlib_pass_if_true("nr_strlcpy from NULL returns dest",
                    (0 != rp) && (rp == dest), "rp=%p dest=%p", rp, dest);

  rp = nr_strlcpy(dest, "abc", 0);
  tlib_pass_if_true("nr_strlcpy with 0 length returns NULL", 0 == rp, "rp=%p",
                    rp);

  /*
   * Test simple nr_strlcpy test less than buffer length.
   */
  filldest(dest, sizeof(dest));
  rp = nr_strlcpy(dest, "abc", sizeof(dest));
  tlib_pass_if_true("simple nr_strlcpy",
                    (0 != rp) && (0 == dest[3]) && (3 == nr_strlen(dest))
                        && (0 == nr_strcmp(dest, "abc")),
                    "rp=%p dest[3]=%d dest='%s'", rp, dest[3], dest);
  tlib_pass_if_true("return pointer is correct", rp == &dest[3],
                    "rp=%p &dest[3]=%p", rp, &dest[3]);
  tlib_pass_if_true("copy did not overwrite", 5 == dest[4], "dest[4]=%d",
                    dest[4]);

  /*
   * Test empty string.
   */
  filldest(dest, sizeof(dest));
  rp = nr_strlcpy(dest, "", sizeof(dest));
  tlib_pass_if_true(
      "nr_strlcpy of empty string works",
      (0 != rp) && (rp == dest) && (0 == dest[0]) && (2 == dest[1]),
      "rp=%p dest=%p, dest[0]=%d dest[1]=%d", rp, dest, dest[0], dest[1]);

  /*
   * Test nr_strlcpy of string that is too long. Note that for this test we
   * pretend that the destination string is only 8 characters, so that we can
   * check to ensure that we do not overwrite memory.
   */
  filldest(dest, sizeof(dest));
  rp = nr_strlcpy(dest, "abcdefghij", 8);
  tlib_pass_if_true("nr_strlcpy of string that is too long",
                    (0 != rp) && (rp == &dest[7]) && (0 == dest[7])
                        && (9 == dest[8]) && (0 == nr_strcmp(dest, "abcdefg")),
                    "rp=%p &dest[7]=%p dest[7]=%d dest[0]=%d dest='%s'", rp,
                    &dest[7], dest[7], dest[8], dest);

  /*
   * Test nr_strlcpy() with source strings that are exactly the same length as
   * the destination buffer length, and that length - 1, to test the boundary
   * conditions. As with the test above we pretend that the destination is only
   * 8 bytes long so we can check for overwrites.
   */
  filldest(dest, sizeof(dest));
  rp = nr_strlcpy(dest, "abcdefgh", 8);
  tlib_pass_if_true("nr_strlcpy of string exactly length of test",
                    (0 != rp) && (rp == &dest[7]) && (0 == dest[7])
                        && (9 == dest[8]) && (0 == nr_strcmp(dest, "abcdefg")),
                    "rp=%p &dest[7]=%p dest[7]=%d dest[8]=%d dest='%s'", rp,
                    &dest[7], dest[7], dest[8], dest);
  filldest(dest, sizeof(dest));
  rp = nr_strlcpy(dest, "abcdefg", 8);
  tlib_pass_if_true("nr_strlcpy of string exactly length of test - 1",
                    (0 != rp) && (rp == &dest[7]) && (0 == dest[7])
                        && (9 == dest[8]) && (0 == nr_strcmp(dest, "abcdefg")),
                    "rp=%p &dest[7]=%p dest[7]=%d dest[8]=%d dest='%s'", rp,
                    &dest[7], dest[7], dest[8], dest);
}

static void test_strcpy(void) {
  char* rp;
  char dest[16];

  rp = nr_strcpy(0, "abc");
  tlib_pass_if_true("nr_strcpy to NULL returns NULL", 0 == rp, "rp=%p", rp);

  rp = nr_strcpy(dest, "abcd");
  tlib_pass_if_true("simple nr_strcpy works",
                    0 != rp && (0 == nr_strcmp(dest, "abcd")),
                    "rp=%p dest='%s'", rp, dest);

  rp = nr_strcpy(dest, NULL);
  tlib_pass_if_true("nr_strcpy of NULL returns empty string",
                    (0 != rp) && (rp == dest) && (0 == dest[0]),
                    "rp=%p rp[0]=%d", rp, rp[0]);

  nr_strcpy(dest, "abc");
  rp = nr_strcpy(dest, "");
  tlib_pass_if_true("nr_strcpy of empty returns empty string",
                    (0 != rp) && (rp == dest) && (0 == dest[0]),
                    "rp=%p rp[0]=%d", rp, rp[0]);
}

static void test_strempty(void) {
  int rp;
  int op;
  const char* emptystr = "";
  const char* nonemptystr = "abc";

  rp = nr_strempty(NULL);
  tlib_pass_if_true("nr_strempty NULL returns 1", 1 == rp, "rp=%d", rp);

  rp = nr_strempty(emptystr);
  tlib_pass_if_true("nr_strempty \"\" returns 1", 1 == rp, "rp=%d", rp);

  rp = nr_strempty(nonemptystr);
  tlib_pass_if_true("nr_strempty \"abc\" returns 0", 0 == rp, "rp=%d", rp);

  rp = nr_strempty("    ");
  tlib_pass_if_true("nr_strempty \"    \" returns 0", 0 == rp, "rp=%d", rp);

  rp = nr_strempty("a");
  tlib_pass_if_true("nr_strempty \"a\" returns 0", 0 == rp, "rp=%d", rp);

  rp = !nr_strempty(NULL);
  op = (NULL != NULL);
  tlib_pass_if_true("!nr_strempty NULL returns 0", op == rp, "rp=%d", rp);

  rp = !nr_strempty(emptystr);
  op = (NULL != emptystr && '\0' != *emptystr);
  tlib_pass_if_true("!nr_strempty \"\" returns 0", op == rp, "rp=%d", rp);

  rp = !nr_strempty(nonemptystr);
  op = (NULL != nonemptystr && '\0' != *nonemptystr);
  tlib_pass_if_true("!nr_strempty \"a\" returns 1", op == rp, "rp=%d", rp);
}

static void test_strcat(void) {
  char* rp;
  char dest[16];

  nr_strcpy(dest, "abc");
  rp = nr_strcat(dest, 0);
  tlib_pass_if_true("nr_strcat of NULL returns dest",
                    (0 != rp) && (rp == &dest[3]), "rp=%p &dest[3]=%p", rp,
                    &dest[3]);

  rp = nr_strcat(0, "abc");
  tlib_pass_if_true("srtcat to NULL returns NULL", 0 == rp, "rp=%p", rp);

  rp = nr_strcat(dest, "");
  tlib_pass_if_true("nr_strcat of empty string works",
                    (0 != rp) && (rp == &dest[3]), "rp=%p &dest[3]=%p", rp,
                    &dest[3]);

  rp = nr_strcat(dest, "def");
  tlib_pass_if_true("nr_strcat works",
                    (0 != rp) && (rp == &dest[6])
                        && (0 == nr_strcmp(dest, "abcdef")) && (0 == *rp),
                    "rp=%p &dest[6]=%p *rp=%d dest='%s'", rp, &dest[6], *rp,
                    dest);
}

static void test_strlen(void) {
  size_t rv;

  rv = nr_strlen(0);
  tlib_pass_if_true("nr_strlen of NULL returns 0", 0 == rv, "rv=%zu", rv);

  rv = nr_strlen("");
  tlib_pass_if_true("nr_strlen of empty returns 0", 0 == rv, "rv=%zu", rv);

  rv = nr_strlen("abc");
  tlib_pass_if_true("simple nr_strlen works", 3 == rv, "rv=%zu", rv);
}

static void test_strnlen(void) {
  char dest[16];
  size_t rv;

  rv = nr_strnlen(0, 8);
  tlib_pass_if_true("nr_strnlen of NULL returns 0", 0 == rv, "rv=%zu", rv);

  rv = nr_strnlen("", 8);
  tlib_pass_if_true("nr_strnlen of empty returns 0", 0 == rv, "rv=%zu", rv);

  nr_strcpy(dest, "abc");
  rv = nr_strnlen(dest, 8);
  tlib_pass_if_true("simple nr_strnlen works", 3 == rv, "rv=%zu", rv);

  rv = nr_strnlen(dest, 0);
  tlib_pass_if_true("nr_strnlen with 0 length returns 0", 0 == rv, "rv=%zu",
                    rv);
  nr_strcpy(dest, "abcdefghij");

  rv = nr_strnlen(dest, 8);
  tlib_pass_if_true("nr_strnlen with overlong string returns max", 8 == rv,
                    "rv=%zu", rv);

  nr_strcpy(dest, "abcdefgh");
  rv = nr_strnlen(dest, 8);
  tlib_pass_if_true("nr_strnlen with longest possible string works", 8 == rv,
                    "rv=%zu", rv);
}

static void test_strcmp(void) {
  int rv;

  rv = nr_strcmp(NULL, NULL);
  tlib_pass_if_true("nr_strcmp NULLS returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strcmp(NULL, "abc");
  tlib_pass_if_true("nr_strcmp (0, rv) returns <0", rv < 0, "rv=%d", rv);

  rv = nr_strcmp("abc", NULL);
  tlib_pass_if_true("nr_strcmp (rv, 0) return >0", rv > 0, "rv=%d", rv);

  rv = nr_strcmp("", "");
  tlib_pass_if_true("nr_strcmp empty strings returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strcmp("abc", "abc");
  tlib_pass_if_true("simple comparison return 0", 0 == rv, "rv=%d", rv);

  rv = nr_strcmp("abc", "abd");
  tlib_pass_if_true("simple comparison return < 0", rv < 0, "rv=%d", rv);

  rv = nr_strcmp("abd", "abc");
  tlib_pass_if_true("simple comparison return > 0", rv > 0, "rv=%d", rv);

  rv = nr_strcmp("abc", "");
  tlib_pass_if_true("comparison against empty > 0", rv > 0, "rv=%d", rv);

  rv = nr_strcmp("", "abc");
  tlib_pass_if_true("comparison against empty < 0", rv < 0, "rv=%d", rv);
}

static void test_stricmp(void) {
  int rv;

  rv = nr_stricmp(NULL, NULL);
  tlib_pass_if_true("nr_stricmp NULLS returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_stricmp(NULL, "abc");
  tlib_pass_if_true("nr_stricmp (0, rv) returns -1", rv < 0, "rv=%d", rv);

  rv = nr_stricmp("abc", NULL);
  tlib_pass_if_true("nr_stricmp (rv, 0) returns 1", rv > 0, "rv=%d", rv);

  rv = nr_stricmp("", "");
  tlib_pass_if_true("nr_stricmp empty strings returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_stricmp("abc", "abc");
  tlib_pass_if_true("simple comparison returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_stricmp("aBc", "AbC");
  tlib_pass_if_true("simple comparison returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_stricmp("abc", "abd");
  tlib_pass_if_true("simple comparison returns < 0", rv < 0, "rv=%d", rv);

  rv = nr_stricmp("AbC", "aBd");
  tlib_pass_if_true("simple comparison returns < 0", rv < 0, "rv=%d", rv);

  rv = nr_stricmp("abd", "abc");
  tlib_pass_if_true("simple comparison returns > 0", rv > 0, "rv=%d", rv);

  rv = nr_stricmp("aBd", "AbC");
  tlib_pass_if_true("simple comparison returns > 0", rv > 0, "rv=%d", rv);

  rv = nr_stricmp("abc", "");
  tlib_pass_if_true("comparison against empty > 0", rv > 0, "rv=%d", rv);

  rv = nr_stricmp("", "abc");
  tlib_pass_if_true("comparison against empty < 0", rv < 0, "rv=%d", rv);
}

static void test_strncmp(void) {
  int rv;

  rv = nr_strncmp(NULL, NULL, 1);
  tlib_pass_if_true("nr_strncmp NULLS returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strncmp(NULL, NULL, 0);
  tlib_pass_if_true("nr_strncmp NULLS returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strncmp(NULL, "abc", 1);
  tlib_pass_if_true("nr_strncmp (NULL, rv, 1) returns <0", rv < 0, "rv=%d", rv);

  rv = nr_strncmp(NULL, "abc", 0);
  tlib_pass_if_true("nr_strncmp (NULL, rv, 0) returns 0", rv == 0, "rv=%d", rv);

  rv = nr_strncmp("abc", NULL, 1);
  tlib_pass_if_true("nr_strncmp (rv, NULL, 1) returns >0", rv > 0, "rv=%d", rv);

  rv = nr_strncmp("abc", NULL, 0);
  tlib_pass_if_true("nr_strncmp (rv, NULL, 0) returns 0", rv == 0, "rv=%d", rv);

  rv = nr_strncmp("", "", 0);
  tlib_pass_if_true("nr_strncmp empty strings returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strncmp("abc", "abc", 3);
  tlib_pass_if_true("simple comparison returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strncmp("abc", "abd", 2);
  tlib_pass_if_true("prefix comparison returns 0", rv == 0, "rv=%d", rv);

  rv = nr_strncmp("abc", "abd", 3);
  tlib_pass_if_true("simple comparison return < 0", rv < 0, "rv=%d", rv);

  rv = nr_strncmp("abd", "abc", 3);
  tlib_pass_if_true("simple comparison return > 0", rv > 0, "rv=%d", rv);

  rv = nr_strncmp("abc", "", 3);
  tlib_pass_if_true("comparison against empty > 0", rv > 0, "rv=%d", rv);

  rv = nr_strncmp("", "abc", 3);
  tlib_pass_if_true("comparison against empty < 0", rv < 0, "rv=%d", rv);
}

static void test_strnicmp(void) {
  int rv;

  rv = nr_strnicmp(NULL, NULL, 1);
  tlib_pass_if_true("nr_strnicmp NULLS returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strnicmp(NULL, NULL, 0);
  tlib_pass_if_true("nr_strnicmp NULLS returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strnicmp(NULL, "abc", 1);
  tlib_pass_if_true("nr_strnicmp (NULL, rv, 1) < 0", rv < 0, "rv=%d", rv);

  rv = nr_strnicmp(NULL, "abc", 0);
  tlib_pass_if_true("nr_strnicmp (NULL, rv, 0) == 0", rv == 0, "rv=%d", rv);

  rv = nr_strnicmp("abc", NULL, 1);
  tlib_pass_if_true("nr_strnicmp (rv, NULL, 1) > 0", rv > 0, "rv=%d", rv);

  rv = nr_strnicmp("abc", NULL, 0);
  tlib_pass_if_true("nr_strnicmp (rv, NULL, 0) == 0", rv == 0, "rv=%d", rv);

  rv = nr_strnicmp("", "", 1);
  tlib_pass_if_true("nr_strnicmp empty strings returns 0", 0 == rv, "rv=%d",
                    rv);

  rv = nr_strnicmp("", "", 0);
  tlib_pass_if_true("nr_strnicmp empty strings returns 0", 0 == rv, "rv=%d",
                    rv);

  rv = nr_strnicmp("abc", "abc", 3);
  tlib_pass_if_true("simple comparison returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strnicmp("aBc", "AbC", 3);
  tlib_pass_if_true("simple comparison returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strnicmp("abc", "abd", 3);
  tlib_pass_if_true("simple comparison returns < 0", rv < 0, "rv=%d", rv);

  rv = nr_strnicmp("AbC", "aBd", 3);
  tlib_pass_if_true("simple comparison returns < 0", rv < 0, "rv=%d", rv);

  rv = nr_strnicmp("abd", "abc", 3);
  tlib_pass_if_true("simple comparison returns > 0", rv > 0, "rv=%d", rv);

  rv = nr_strnicmp("abd", "abc", 2);
  tlib_pass_if_true("prefix comparison returns 0", rv == 0, "rv=%d", rv);

  rv = nr_strnicmp("aBd", "AbC", 3);
  tlib_pass_if_true("simple comparison returns > 0", rv > 0, "rv=%d", rv);

  rv = nr_strnicmp("aBd", "AbC", 2);
  tlib_pass_if_true("prefix comparison returns 0", rv == 0, "rv=%d", rv);

  rv = nr_strnicmp("abc", "", 3);
  tlib_pass_if_true("comparison against empty > 0", rv > 0, "rv=%d", rv);

  rv = nr_strnicmp("", "abc", 3);
  tlib_pass_if_true("comparison against empty < 0", rv < 0, "rv=%d", rv);
}

static void test_streq(void) {
  int rv;

  rv = nr_streq(0, 0);
  tlib_pass_if_true("nr_streq NULLS returns 1", 1 == rv, "rv=%d", rv);

  rv = nr_streq("", "");
  tlib_pass_if_true("nr_streq empty strings returns 1", 1 == rv, "rv=%d", rv);

  rv = nr_streq("abc", "abc");
  tlib_pass_if_true("simple comparison returns 1", 1 == rv, "rv=%d", rv);

  rv = nr_streq("abc", "abd");
  tlib_pass_if_true("simple comparison returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_streq(0, "abc");
  tlib_pass_if_true("nr_streq (0, rv) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_streq("abc", 0);
  tlib_pass_if_true("nr_streq (rv, 0) returns 0", 0 == rv, "rv=%d", rv);
}

static void test_strieq(void) {
  int rv;

  rv = nr_strieq(0, 0);
  tlib_pass_if_true("nr_strieq NULLS returns 1", 1 == rv, "rv=%d", rv);

  rv = nr_strieq("", "");
  tlib_pass_if_true("nr_strieq empty strings returns 1", 1 == rv, "rv=%d", rv);

  rv = nr_strieq("abc", "abc");
  tlib_pass_if_true("simple comparison returns 1", 1 == rv, "rv=%d", rv);

  rv = nr_strieq("aBc", "AbC");
  tlib_pass_if_true("simple comparison returns 1", 1 == rv, "rv=%d", rv);

  rv = nr_strieq("abc", "abd");
  tlib_pass_if_true("simple comparison returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strieq("AbC", "aBd");
  tlib_pass_if_true("simple comparison returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strieq(0, "abc");
  tlib_pass_if_true("nr_strieq (0, rv) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strieq("abc", 0);
  tlib_pass_if_true("nr_strieq (rv, 0) returns 0", 0 == rv, "rv=%d", rv);
}

static void test_strchr(void) {
  char dest[16];
  char* rp;

  rp = nr_strchr(0, 0);
  tlib_pass_if_true("nr_strchr (0,0) returns 0", 0 == rp, "rp=%p", rp);

  nr_strcpy(dest, "abc");
  rp = nr_strchr(dest, 0);
  tlib_pass_if_true("nr_strchr (str, 0) returns EOS", dest + 3 == rp, "rp=%p",
                    rp);

  rp = nr_strchr(dest, 'd');
  tlib_pass_if_true("nr_strchr (str, bad) returns 0", 0 == rp, "rp=%p", rp);

  rp = nr_strchr(dest, 'b');
  tlib_pass_if_true("nr_strchr (str, good) return OK", dest + 1 == rp, "rp=%p",
                    rp);
}

static void test_strrchr(void) {
  char dest[16];
  char* rp;

  rp = nr_strrchr(0, 0);
  tlib_pass_if_true("nr_strrchr (0,0) returns 0", 0 == rp, "rp=%p", rp);

  nr_strcpy(dest, "abc");
  rp = nr_strrchr(dest, 0);
  tlib_pass_if_true("nr_strrchr (str, 0) returns EOS", dest + 3 == rp, "rp=%p",
                    rp);

  rp = nr_strrchr(dest, 'd');
  tlib_pass_if_true("nr_strrchr (str, bad) returns 0", 0 == rp, "rp=%p", rp);

  rp = nr_strrchr(dest, 'b');
  tlib_pass_if_true("nr_strrchr (str, good) return OK", dest + 1 == rp, "rp=%p",
                    rp);
}

static void test_strspn(void) {
  int rv;

  rv = nr_strspn(0, 0);
  tlib_pass_if_true("nr_strspn (0, 0) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strspn("abc", 0);
  tlib_pass_if_true("nr_strspn (str, 0) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strspn(0, "abc");
  tlib_pass_if_true("nr_strspn (0, str) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strspn("abcdef", "abc");
  tlib_pass_if_true("nr_strspn (str, str) works", 3 == rv, "rv=%d", rv);

  rv = nr_strspn("abcdef", "abcdef");
  tlib_pass_if_true("nr_strspn (same, same) returns EOS", 6 == rv, "rv=%d", rv);

  rv = nr_strspn("abcdef", "ghij");
  tlib_pass_if_true("nr_strspn (str, missing) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strspn("abcdef", "ghijklmn");
  tlib_pass_if_true("nr_strspn (str, missing) returns 0", 0 == rv, "rv=%d", rv);
}

static void test_strcspn(void) {
  int rv;

  rv = nr_strcspn(0, 0);
  tlib_pass_if_true("nr_strcspn (0, 0) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strcspn("abc", 0);
  tlib_pass_if_true("nr_strcspn (str, 0) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strcspn(0, "abc");
  tlib_pass_if_true("nr_strcspn (0, str) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strcspn("abcdef", "def");
  tlib_pass_if_true("nr_strcspn (str, str) works", 3 == rv, "rv=%d", rv);

  rv = nr_strcspn("abcdef", "abc");
  tlib_pass_if_true("nr_strcspn (str, str) works", 0 == rv, "rv=%d", rv);

  rv = nr_strcspn("abcdef", "abcdef");
  tlib_pass_if_true("nr_strspn (same, same) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strcspn("abcdef", "ghij");
  tlib_pass_if_true("nr_strspn (str, missing) return OK", 6 == rv, "rv=%d", rv);

  rv = nr_strcspn("abcdef", "ghijklmn");
  tlib_pass_if_true("nr_strspn (str, missing) return OK", 6 == rv, "rv=%d", rv);
}

static void test_strnspn(void) {
  int rv;

  rv = nr_strnspn(0, 0, 0, 0);
  tlib_pass_if_true("nr_strnspn (0, 0, 0, 0) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strnspn("abc", 3, 0, 0);
  tlib_pass_if_true("nr_strspn (str, 0) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strnspn(0, 0, "abc", 3);
  tlib_pass_if_true("nr_strnspn (0, str) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strnspn("abcdef", 6, "abc", 3);
  tlib_pass_if_true("nr_strnspn (str, str) works", 3 == rv, "rv=%d", rv);

  rv = nr_strnspn("abcdef", 6, "cba", 3);
  tlib_pass_if_true("nr_strnspn (str, rts) works", 3 == rv, "rv=%d", rv);

  rv = nr_strnspn("abcdef", 6, "abcdef", 6);
  tlib_pass_if_true("nr_strnspn (same, same) returns EOS", 6 == rv, "rv=%d",
                    rv);

  rv = nr_strnspn("abcdef", 6, "ghij", 4);
  tlib_pass_if_true("nr_strnspn (str, missing) returns 0", 0 == rv, "rv=%d",
                    rv);

  rv = nr_strnspn("abcdef", 6, "ghijklmn", 8);
  tlib_pass_if_true("nr_strnspn (str, missing) returns 0", 0 == rv, "rv=%d",
                    rv);
}

static void test_strncspn(void) {
  int rv;

  rv = nr_strncspn(0, 0, 0, 0);
  tlib_pass_if_true("nr_strncspn (0, 0, 0, 0) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strncspn("abc", 3, 0, 0);
  tlib_pass_if_true("nr_strncspn (str, 0) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strncspn(0, 0, "abc", 3);
  tlib_pass_if_true("nr_strncspn (0, str) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strncspn("abcdef", 6, "def", 3);
  tlib_pass_if_true("nr_strcspn (str, str) works", 3 == rv, "rv=%d", rv);

  rv = nr_strncspn("abcdef", 6, "abc", 3);
  tlib_pass_if_true("nr_strcspn (str, str) works", 0 == rv, "rv=%d", rv);

  rv = nr_strncspn("abcdef", 6, "abcdef", 6);
  tlib_pass_if_true("nr_strspn (same, same) returns 0", 0 == rv, "rv=%d", rv);

  rv = nr_strncspn("abcdef", 6, "ghij", 4);
  tlib_pass_if_true("nr_strspn (str, missing) return OK", 6 == rv, "rv=%d", rv);

  rv = nr_strncspn("abcdef", 6, "ghijklmn", 8);
  tlib_pass_if_true("nr_strspn (str, missing) return OK", 6 == rv, "rv=%d", rv);
}

static void test_stridx(void) {
  int rv;

  /*
   * Test : Bad Parameters
   */
  rv = nr_stridx(0, 0);
  tlib_pass_if_true("zero inputs", -1 == rv, "rv=%d", rv);

  rv = nr_stridx("alpha beta gamma", 0);
  tlib_pass_if_true("null needle", -1 == rv, "rv=%d", rv);

  rv = nr_stridx(0, "beta");
  tlib_pass_if_true("null str", -1 == rv, "rv=%d", rv);

  /*
   * Test : Not Found
   */
  rv = nr_stridx("alpha beta gamma", "psi");
  tlib_pass_if_true("not found", -1 == rv, "rv=%d", rv);

  rv = nr_stridx("alph", "alpha");
  tlib_pass_if_true("longer needle", -1 == rv, "rv=%d", rv);

  /*
   * Test : Success
   */
  rv = nr_stridx("alpha beta gamma", "");
  tlib_pass_if_true("empty needle", 0 == rv, "rv=%d", rv);

  rv = nr_stridx("\0", "");
  tlib_pass_if_true("empty needle in empty string", 0 == rv, "rv=%d", rv);

  rv = nr_stridx("alpha beta gamma", "gamma");
  tlib_pass_if_true("end of string", 11 == rv, "rv=%d", rv);

  rv = nr_stridx("alpha beta gamma", "alpha");
  tlib_pass_if_true("beginning of string", 0 == rv, "rv=%d", rv);
}

static void test_strcaseidx(void) {
  int rv;

  /*
   * Test : Bad Parameters
   */
  rv = nr_strcaseidx(0, 0);
  tlib_pass_if_true("zero inputs", -1 == rv, "rv=%d", rv);

  rv = nr_strcaseidx("alpha beta gamma", 0);
  tlib_pass_if_true("null needle", -1 == rv, "rv=%d", rv);

  rv = nr_strcaseidx(0, "beta");
  tlib_pass_if_true("null str", -1 == rv, "rv=%d", rv);

  /*
   * Test : Not Found
   */
  rv = nr_strcaseidx("alpha beta gamma", "psi");
  tlib_pass_if_true("not found", -1 == rv, "rv=%d", rv);

  rv = nr_strcaseidx("alph", "alpha");
  tlib_pass_if_true("longer needle", -1 == rv, "rv=%d", rv);

  rv = nr_strcaseidx("Alph", "alpha");
  tlib_pass_if_true("longer needle", -1 == rv, "rv=%d", rv);

  /*
   * Test : Success
   */
  rv = nr_strcaseidx("alpha beta gamma", "");
  tlib_pass_if_true("empty needle", 0 == rv, "rv=%d", rv);

  rv = nr_strcaseidx("\0", "");
  tlib_pass_if_true("empty needle in empty string", 0 == rv, "rv=%d", rv);

  rv = nr_strcaseidx("alpha beta gamma", "gamma");
  tlib_pass_if_true("case 1 end of string", 11 == rv, "rv=%d", rv);

  rv = nr_strcaseidx("alpha beta Gamma", "gamma");
  tlib_pass_if_true("case 2 end of string", 11 == rv, "rv=%d", rv);

  rv = nr_strcaseidx("alpha beta gamma", "Gamma");
  tlib_pass_if_true("case 3 end of string", 11 == rv, "rv=%d", rv);

  rv = nr_strcaseidx("alpha beta gamma", "alpha");
  tlib_pass_if_true("case 1 beginning of string", 0 == rv, "rv=%d", rv);

  rv = nr_strcaseidx("Alpha beta gamma", "alpha");
  tlib_pass_if_true("case 2 beginning of string", 0 == rv, "rv=%d", rv);

  rv = nr_strcaseidx("alpha beta gamma", "Alpha");
  tlib_pass_if_true("case 3 beginning of string", 0 == rv, "rv=%d", rv);
}

static void test_strnidx(void) {
  int rv;

  /*
   * Test : Bad Parameters
   */
  rv = nr_strnidx(0, 0, 0);
  tlib_pass_if_true("zero inputs", -1 == rv, "rv=%d", rv);

  rv = nr_strnidx("alpha beta gamma", 0, 16);
  tlib_pass_if_true("null needle", -1 == rv, "rv=%d", rv);

  rv = nr_strnidx(0, "beta", 16);
  tlib_pass_if_true("null str", -1 == rv, "rv=%d", rv);

  rv = nr_strnidx("alpha beta gamma", "beta", 0);
  tlib_pass_if_true("zero len", -1 == rv, "rv=%d", rv);

  rv = nr_strnidx("alpha beta gamma", "beta", -1);
  tlib_pass_if_true("negative len", -1 == rv, "rv=%d", rv);

  /*
   * Test : Not Found
   */
  rv = nr_strnidx("alpha beta gamma", "psi", 16);
  tlib_pass_if_true("not found", -1 == rv, "rv=%d", rv);

  rv = nr_strnidx("alpha beta\0gamma", "gamma", 16);
  tlib_pass_if_true("not found after \\0", -1 == rv, "rv=%d", rv);

  rv = nr_strnidx("alpha beta gamma", "gamma", 9);
  tlib_pass_if_true("len obeyed", -1 == rv, "rv=%d", rv);

  rv = nr_strnidx("alph", "alpha", 4);
  tlib_pass_if_true("longer needle", -1 == rv, "rv=%d", rv);

  /*
   * Test : Success
   */
  rv = nr_strnidx("alpha beta gamma", "", 16);
  tlib_pass_if_true("empty needle", 0 == rv, "rv=%d", rv);

  rv = nr_strnidx("\0", "", 16);
  tlib_pass_if_true("empty needle in empty string", 0 == rv, "rv=%d", rv);

  rv = nr_strnidx("alpha beta gamma", "gamma", 16);
  tlib_pass_if_true("end of string", 11 == rv, "rv=%d", rv);

  rv = nr_strnidx("alpha beta gamma", "alpha", 16);
  tlib_pass_if_true("beginning of string", 0 == rv, "rv=%d", rv);
}

#define STRLEN(X) (X), (sizeof(X) - 1)

/*
 * These tests are put into a table since they apply to both nr_strncaseidx
 * and nr_strncaseidx_last_match.
 */
typedef struct _nr_strncaseidx_test_case {
  const char* testname;
  const char* input_string;
  int input_string_len;
  const char* input_needle;
  int expected;
} nr_strncaseidx_test_case;
nr_strncaseidx_test_case nr_strncaseidx_test_cases[] = {
    /* Bad parameters */
    {"zero inputs", 0, 0, 0, -1},
    {"null needle", STRLEN("alpha beta gamma"), 0, -1},
    {"null str", 0, 16, "beta", -1},
    {"zero str len", "alpha beta gamma", 0, "beta", -1},
    {"negative str len", "alpha beta gamma", -1, "beta", -1},
    {"empty needle", STRLEN("alpha beta gamma"), "\0", -1},
    {"empty needle empty str", STRLEN(""), "\0", -1},
    /* Not found */
    {"not found", STRLEN("alpha beta gamma"), "psi", -1},
    {"not found before \\0", STRLEN("alpha beta\0gamma"), "psi", -1},
    {"len obeyed", "alpha beta gamma", 9, "gamma", -1},
    {"len obeyed", "  gamma", 6, "gamma", -1},
    {"longer needle", STRLEN("alph"), "alpha", -1},
    /* Success */
    {"end of string", STRLEN("alpha beta gamma"), "gamma", 11},
    {"beginning of string", STRLEN("alpha beta gamma"), "alpha", 0},
    {"needle matches str", STRLEN("gamma"), "gamma", 0},
    /* Case Insensivity */
    {"case insensitive", STRLEN("  gamma  "), "gAmMa", 2},
    {"case insensitive", STRLEN("  gamma  "), "Gamma", 2},
    {"case insensitive", STRLEN("  Gamma  "), "gamma", 2},
    {"case insensitive", STRLEN("  GAMMA  "), "gamma", 2},
    /* End of test cases with NULL test name */
    {0, 0, 0, 0, 0},
};

static void test_strncaseidx(void) {
  int rv;
  int i;

  for (i = 0; nr_strncaseidx_test_cases[i].testname; i++) {
    const nr_strncaseidx_test_case* tc = nr_strncaseidx_test_cases + i;

    rv = nr_strncaseidx(tc->input_string, tc->input_needle,
                        tc->input_string_len);
    tlib_pass_if_true(tc->testname, rv == tc->expected, "rv=%d tc->expected=%d",
                      rv, tc->expected);
  }

  /*
   * Test : First Match Found
   */
  rv = nr_strncaseidx("alpha beta alpha gamma", "alpha", 22);
  tlib_pass_if_true("first match found", 0 == rv, "rv=%d", rv);
}

static void test_nr_strncaseidx_last_match(void) {
  int rv;
  int i;

  for (i = 0; nr_strncaseidx_test_cases[i].testname; i++) {
    const nr_strncaseidx_test_case* tc = nr_strncaseidx_test_cases + i;

    rv = nr_strncaseidx_last_match(tc->input_string, tc->input_needle,
                                   tc->input_string_len);
    tlib_pass_if_true(tc->testname, rv == tc->expected, "rv=%d tc->expected=%d",
                      rv, tc->expected);
  }

  /*
   * Test : Last Match Found
   */
  rv = nr_strncaseidx_last_match("alpha beta alpha gamma", "alpha", 22);
  tlib_pass_if_true("first match found", 11 == rv, "rv=%d", rv);
}

static void test_str_char_count(void) {
  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal("NULL string", 0, nr_str_char_count(NULL, '\0'));

  /*
   * Test : Empty string.
   */
  tlib_pass_if_int_equal("empty string", 0, nr_str_char_count("", 'a'));

  /*
   * Test : Not found.
   */
  tlib_pass_if_int_equal("not found", 0, nr_str_char_count("foo", '\0'));

  /*
   * Test : Found.
   */
  tlib_pass_if_int_equal("empty string", 2, nr_str_char_count("foo", 'o'));
}

static void test_formatf(void) {
  char* rp;

  rp = nr_formatf("zip=%d zap=%s", 123, "zop");
  tlib_pass_if_str_equal("normal use", rp, "zip=123 zap=zop");
  nr_free(rp);

  rp = nr_formatf("zip");
  tlib_pass_if_str_equal("no extra args", rp, "zip");
  nr_free(rp);

  rp = nr_formatf("%s", "");
  tlib_pass_if_str_equal("empty string formatted", rp, "");
  nr_free(rp);

  rp = nr_formatf(NULL);
  tlib_pass_if_str_equal("NULL format string", rp, NULL);
  nr_free(rp);
}

struct nr_strsplit_testcase {
  const char* input;
  const char* delim;
  const char* expected;
  int use_empty;
} testcases[] = {
    {.input = NULL, .delim = ";", .expected = "null", .use_empty = 0},
    {.input = "a,b", .delim = NULL, .expected = "null", .use_empty = 0},
    {.input = "abc", .delim = "", .expected = "[\"abc\"]", .use_empty = 0},
    {.input = "", .delim = ";", .expected = "[\"\"]", .use_empty = 0},
    {.input = "", .delim = "", .expected = "[\"\"]", .use_empty = 0},
    {.input = "abc", .delim = ";", .expected = "[\"abc\"]", .use_empty = 0},
    {.input = "a,b,c",
     .delim = ",",
     .expected = "[\"a\",\"b\",\"c\"]",
     .use_empty = 0},
    {.input = "abc;def",
     .delim = ";",
     .expected = "[\"abc\",\"def\"]",
     .use_empty = 0},
    {.input = "abc   ;def \t ",
     .delim = ";",
     .expected = "[\"abc\",\"def\"]",
     .use_empty = 0},
    {.input = "  abc \t ; \t  def  \t",
     .delim = ";",
     .expected = "[\"abc\",\"def\"]",
     .use_empty = 0},
    {.input = "abc \t  ",
     .delim = ";",
     .expected = "[\"abc\"]",
     .use_empty = 0},
    {.input = " \t\t  abc",
     .delim = ";",
     .expected = "[\"abc\"]",
     .use_empty = 0},
    {.input = " \t\t  abc\t  \t",
     .delim = ";",
     .expected = "[\"abc\"]",
     .use_empty = 0},
    {.input = "a1,b2;c3",
     .delim = ",;",
     .expected = "[\"a1\",\"b2\",\"c3\"]",
     .use_empty = 0},
    {.input = "a1,,b2,c3",
     .delim = ",",
     .expected = "[\"a1\",\"b2\",\"c3\"]",
     .use_empty = 0},
    {.input = "a1,,b2,c3",
     .delim = ",",
     .expected = "[\"a1\",\"\",\"b2\",\"c3\"]",
     .use_empty = 1},
    {.input = ",a1,,b2,c3",
     .delim = ",",
     .expected = "[\"a1\",\"b2\",\"c3\"]",
     .use_empty = 0},
    {.input = ",a1,,b2,c3",
     .delim = ",",
     .expected = "[\"\",\"a1\",\"\",\"b2\",\"c3\"]",
     .use_empty = 1},
    {.input = ",a1,,b2,c3,",
     .delim = ",",
     .expected = "[\"a1\",\"b2\",\"c3\"]",
     .use_empty = 0},
    {.input = ",a1,,b2,c3,",
     .delim = ",",
     .expected = "[\"\",\"a1\",\"\",\"b2\",\"c3\",\"\"]",
     .use_empty = 1},
    {.input = ",;;,", .delim = ",;", .expected = "[]", .use_empty = 0},
    {.input = ",;;,",
     .delim = ",;",
     .expected = "[\"\",\"\",\"\",\"\",\"\"]",
     .use_empty = 1},
};

static void test_strsplit(void) {
  int i;
  int count = sizeof(testcases) / sizeof(struct nr_strsplit_testcase);

  for (i = 0; i < count; i++) {
    nrobj_t* arr;
    char* json;

    arr = nr_strsplit(testcases[i].input, testcases[i].delim,
                      testcases[i].use_empty);
    json = nro_to_json(arr);
    tlib_pass_if_str_equal("strsplit", json, testcases[i].expected);
    nr_free(json);
    nro_delete(arr);
  }
}

static void test_isalnum(void) {
  tlib_pass_if_int_equal(__func__, 0, nr_isalnum(EOF));

  /*
   * For the "C" locale, we should agree with libc
   */
  for (int i = 0; i <= 255; i++) {
    tlib_pass_if_int_equal(__func__, !!isalnum(i), !!nr_isalnum(i));
  }

  /*
   * Test some inputs that result from a signed char being promoted
   * (and therefore sign-extended) to an int. Unlike isalnum, these inputs
   * should NOT result in undefined behavior.
   */
  for (int i = -127; i < 0; i++) {
    tlib_pass_if_int_equal(__func__, 0, nr_isalnum(i));
  }
}

static void test_isalpha(void) {
  tlib_pass_if_int_equal(__func__, 0, nr_isalpha(EOF));

  /*
   * For the "C" locale, we should agree with libc
   */
  for (int i = 0; i <= 255; i++) {
    tlib_pass_if_int_equal(__func__, !!isalpha(i), !!nr_isalpha(i));
  }

  /*
   * Test some inputs that result from a signed char being promoted
   * (and therefore sign-extended) to an int. Unlike isalpha, these inputs
   * should NOT result in undefined behavior.
   */
  for (int i = -127; i < 0; i++) {
    tlib_pass_if_int_equal(__func__, 0, nr_isalpha(i));
  }
}

static void test_isblank(void) {
  tlib_pass_if_int_equal(__func__, 0, nr_isblank(EOF));

  /*
   * For the "C" locale, we should agree with libc
   */
  for (int i = 0; i <= 255; i++) {
    tlib_pass_if_int_equal(__func__, !!isblank(i), !!nr_isblank(i));
  }

  /*
   * Test some inputs that result from a signed char being promoted
   * (and therefore sign-extended) to an int. Unlike isblank, these inputs
   * should NOT result in undefined behavior.
   */
  for (int i = -127; i < 0; i++) {
    tlib_pass_if_int_equal(__func__, 0, nr_isblank(i));
  }
}

static void test_isdigit(void) {
  tlib_pass_if_int_equal(__func__, 0, nr_isdigit(EOF));

  /*
   * For the "C" locale, we should agree with libc
   */
  for (int i = 0; i <= 255; i++) {
    tlib_pass_if_int_equal(__func__, !!isdigit(i), !!nr_isdigit(i));
  }

  /*
   * Test some inputs that result from a signed char being promoted
   * (and therefore sign-extended) to an int. Unlike isdigit, these inputs
   * should NOT result in undefined behavior.
   */
  for (int i = -127; i < 0; i++) {
    tlib_pass_if_int_equal(__func__, 0, nr_isdigit(i));
  }
}

static void test_islower(void) {
  tlib_pass_if_int_equal(__func__, 0, nr_islower(EOF));

  /*
   * For the "C" locale, we should agree with libc
   */
  for (int i = 0; i <= 255; i++) {
    tlib_pass_if_int_equal(__func__, !!islower(i), !!nr_islower(i));
  }

  /*
   * Test some inputs that result from a signed char being promoted
   * (and therefore sign-extended) to an int. Unlike islower, these inputs
   * should NOT result in undefined behavior.
   */
  for (int i = -127; i < 0; i++) {
    tlib_pass_if_int_equal(__func__, 0, nr_islower(i));
  }
}

static void test_isspace(void) {
  tlib_pass_if_int_equal(__func__, 0, nr_isspace(EOF));

  /*
   * For the "C" locale, we should agree with libc
   */
  for (int i = 0; i <= 255; i++) {
    tlib_pass_if_int_equal(__func__, !!isspace(i), !!nr_isspace(i));
  }

  /*
   * Test some inputs that result from a signed char being promoted
   * (and therefore sign-extended) to an int. Unlike isspace, these inputs
   * should NOT result in undefined behavior.
   */
  for (int i = -127; i < 0; i++) {
    tlib_pass_if_int_equal(__func__, 0, nr_isspace(i));
  }
}

static void test_isupper(void) {
  tlib_pass_if_int_equal(__func__, 0, nr_isupper(EOF));

  /*
   * For the "C" locale, we should agree with libc
   */
  for (int i = 0; i <= 255; i++) {
    tlib_pass_if_int_equal(__func__, !!isupper(i), !!nr_isupper(i));
  }

  /*
   * Test some inputs that result from a signed char being promoted
   * (and therefore sign-extended) to an int. Unlike isupper, these inputs
   * should NOT result in undefined behavior.
   */
  for (int i = -127; i < 0; i++) {
    tlib_pass_if_int_equal(__func__, 0, nr_isupper(i));
  }
}

static void test_isxdigit(void) {
  tlib_pass_if_int_equal(__func__, 0, nr_isxdigit(EOF));

  /*
   * For the "C" locale, we should agree with libc
   */
  for (int i = 0; i <= 255; i++) {
    tlib_pass_if_int_equal(__func__, !!isxdigit(i), !!nr_isxdigit(i));
  }

  /*
   * Test some inputs that result from a signed char being promoted
   * (and therefore sign-extended) to an int. Unlike isxdigit, these inputs
   * should NOT result in undefined behavior.
   */
  for (int i = -127; i < 0; i++) {
    tlib_pass_if_int_equal(__func__, 0, nr_isxdigit(i));
  }
}

static void test_tolower(void) {
  tlib_pass_if_int_equal(__func__, EOF, nr_tolower(EOF));

  /*
   * For the "C" locale, we should agree with libc
   */
  for (int i = 0; i <= 255; i++) {
    tlib_pass_if_int_equal(__func__, tolower(i), nr_tolower(i));
  }

  /*
   * Test some inputs that result from a signed char being promoted
   * (and therefore sign-extended) to an int. Unlike tolower, these inputs
   * should NOT result in undefined behavior.
   */
  for (int i = -127; i < 0; i++) {
    tlib_pass_if_int_equal(__func__, i, nr_tolower(i));
  }
}

static void test_toupper(void) {
  tlib_pass_if_int_equal(__func__, EOF, nr_toupper(EOF));

  /*
   * For the "C" locale, we should agree with libc
   */
  for (int i = 0; i <= 255; i++) {
    tlib_pass_if_int_equal(__func__, toupper(i), nr_toupper(i));
  }

  /*
   * Test some inputs that result from a signed char being promoted
   * (and therefore sign-extended) to an int. Unlike toupper, these inputs
   * should NOT result in undefined behavior.
   */
  for (int i = -127; i < 0; i++) {
    tlib_pass_if_int_equal(__func__, i, nr_toupper(i));
  }
}

static void test_str_append(void) {
  char* str = NULL;
  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("null dest and src strings",
                    nr_str_append(NULL, NULL, ","));
  tlib_pass_if_str_equal("null src string", nr_str_append("dest", NULL, ","),
                         "dest");

  str = nr_str_append(str, "string1", ",");
  tlib_pass_if_str_equal("null dest string", str, "string1");
  /*
   * Test : Valid destination and source strings.
   */
  str = nr_str_append(str, "string2", ",");
  tlib_pass_if_str_equal("valid dest and src strings", str, "string1,string2");
  nr_free(str);

  /*
   * Test : Delimiters.
   */
  str = nr_str_append(str, "string1", NULL);
  str = nr_str_append(str, "string2", ":");
  tlib_pass_if_str_equal("valid dest and src strings", str, "string1:string2");
  nr_free(str);

  str = nr_str_append(str, "string1", ",");
  str = nr_str_append(str, "string2", NULL);
  tlib_pass_if_str_equal("valid dest and src strings", str, "string1string2");
  nr_free(str);
}

static void test_iendswith(void) {
  tlib_pass_if_bool_equal("input is NULL", false,
                          nr_striendswith(NULL, 4, NR_PSTR("bar")));

  tlib_pass_if_bool_equal("input is empty", false,
                          nr_striendswith(NR_PSTR(""), NR_PSTR("bar")));

  tlib_pass_if_bool_equal("input is too short", false,
                          nr_striendswith(NR_PSTR("ar"), NR_PSTR("bar")));

  tlib_pass_if_bool_equal(
      "no match", false, nr_striendswith(NR_PSTR("foobarbaz"), NR_PSTR("bar")));

  tlib_pass_if_bool_equal("not quite match", false,
                          nr_striendswith(NR_PSTR("foobarr"), NR_PSTR("bar")));

  tlib_pass_if_bool_equal("suffix match", true,
                          nr_striendswith(NR_PSTR("foobar"), NR_PSTR("bar")));

  tlib_pass_if_bool_equal("exact match", true,
                          nr_striendswith(NR_PSTR("bar"), NR_PSTR("bar")));
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  static nrthread_mutex_t locale_lock = NRTHREAD_MUTEX_INITIALIZER;

  nrt_mutex_lock(&locale_lock);
  setlocale(LC_CTYPE, "C");
  nrt_mutex_unlock(&locale_lock);

  test_strxcpy();
  test_strlcpy();
  test_strcpy();
  test_strcat();
  test_strempty();
  test_strlen();
  test_strnlen();
  test_strcmp();
  test_stricmp();
  test_strncmp();
  test_strnicmp();
  test_streq();
  test_strieq();
  test_strchr();
  test_strrchr();
  test_strspn();
  test_strcspn();
  test_strnspn();
  test_strncspn();
  test_stridx();
  test_strcaseidx();
  test_strnidx();
  test_strncaseidx();
  test_nr_strncaseidx_last_match();
  test_str_char_count();
  test_formatf();
  test_strsplit();
  test_str_append();
  test_iendswith();

  /*
   * character tests
   */

  test_isalnum();
  test_isalpha();
  test_isblank();
  test_isdigit();
  test_islower();
  test_isspace();
  test_isupper();
  test_isxdigit();
  test_tolower();
  test_toupper();
}
