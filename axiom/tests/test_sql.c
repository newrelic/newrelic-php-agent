/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <unistd.h>

#include "util_memory.h"
#include "util_sql.h"
#include "util_sql_private.h"
#include "util_strings.h"
#include "util_text.h"

#include "tlib_main.h"

#define test_get_operation_and_table(...) \
  test_get_operation_and_table_f(__VA_ARGS__, __FILE__, __LINE__)

static void test_get_operation_and_table_f(const char* test,
                                           const char* sql,
                                           const char* expected_operation,
                                           const char* expected_table,
                                           const char* file,
                                           int line) {
  char* table = 0;
  const char* operation = 0;
  int show_sql_parsing = 1;

  if (NULL == test) {
    test = "nr_sql_find_table_name";
  }

  table = 0;
  operation = 0;
  nr_sql_get_operation_and_table(sql, &operation, &table, show_sql_parsing);

  if (expected_operation) {
    test_pass_if_true(test, 0 == nr_strcmp(expected_operation, operation),
                      "expected_operation=%s operation=%s", expected_operation,
                      NRSAFESTR(operation));
  } else {
    test_pass_if_true(test, 0 == operation, "operation=%p", operation);
  }

  if (expected_table) {
    test_pass_if_true(test, 0 == nr_strcmp(expected_table, table),
                      "expected_table=%s table=%s", expected_table,
                      NRSAFESTR(table));
  } else {
    test_pass_if_true(test, 0 == table, "table=%p", table);
  }
  nr_free(table);
}

#define STRLEN(X) (X), (sizeof(X) - 1)

#define whitespace_comment_testcase(...) \
  whitespace_comment_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void whitespace_comment_testcase_fn(const char* input,
                                           const char* expected_output,
                                           const char* file,
                                           int line) {
  const char* rv;
  int show_sql_parsing = 1;

  rv = nr_sql_whitespace_comment_prefix(input, show_sql_parsing);

  test_pass_if_true(NRSAFESTR(input), 0 == nr_strcmp(rv, expected_output),
                    "rv=%s expected_output=%s", NRSAFESTR(rv),
                    NRSAFESTR(expected_output));
}

static void test_whitespace_comment_prefix(void) {
  /*
   * Test : Bad Parameters
   */
  whitespace_comment_testcase(0, 0);
  whitespace_comment_testcase("", "");
  /*
   * Test : Whitespace
   */
  whitespace_comment_testcase(" SELECT", "SELECT");
  whitespace_comment_testcase("\rSELECT", "SELECT");
  whitespace_comment_testcase("\nSELECT", "SELECT");
  whitespace_comment_testcase("\tSELECT", "SELECT");
  whitespace_comment_testcase("\vSELECT", "SELECT");
  whitespace_comment_testcase("\fSELECT", "SELECT");
  /*
   * Test : Comments, Comments and Whitespace
   */
  whitespace_comment_testcase("/**/SELECT", "SELECT");
  whitespace_comment_testcase("/* hey */SELECT", "SELECT");
  whitespace_comment_testcase("/* \n*/SELECT", "SELECT");
  whitespace_comment_testcase("\t/**/SELECT", "SELECT");
  whitespace_comment_testcase("/**/\fSELECT", "SELECT");
  whitespace_comment_testcase("/*/**/*/\fSELECT",
                              "*/\fSELECT"); /* Nested comments not supported */
  whitespace_comment_testcase(" /* arma */ /* virumque */ /* cano */ SELECT",
                              "SELECT");
  whitespace_comment_testcase("/* * */ SELECT", "SELECT");
  /*
   * Test : Corner Cases, Incomplete Comments, Non-Terminated Strings
   */
  whitespace_comment_testcase(" ", "");
  whitespace_comment_testcase("\\", "\\");
  whitespace_comment_testcase("\\*", "\\*");
  whitespace_comment_testcase(" \\", "\\");
  whitespace_comment_testcase("  ", "");
  whitespace_comment_testcase("/*", 0);
  whitespace_comment_testcase("/**/", "");
  whitespace_comment_testcase("/*  ", 0);
  whitespace_comment_testcase("/* *", 0);
  whitespace_comment_testcase("  /*", 0);
}

#define sql_obfuscate_testcase(...) \
  sql_obfuscate_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void sql_obfuscate_testcase_fn(const char* testname,
                                      const char* sql,
                                      const char* expected,
                                      const char* file,
                                      int line) {
  char* output = nr_sql_obfuscate(sql);
  char* idempotent = output ? nr_sql_obfuscate(output) : 0;

  test_pass_if_true(testname, 0 == nr_strcmp(expected, output),
                    "expected=%s output=%s", NRSAFESTR(expected),
                    NRSAFESTR(output));
  test_pass_if_true(testname, 0 == nr_strcmp(idempotent, output),
                    "idempotent=%s output=%s", NRSAFESTR(idempotent),
                    NRSAFESTR(output));

  nr_free(idempotent);
  nr_free(output);
}

static void test_sql_obfuscate(void) {
  char* s1;
  const char* s2;
  int lg;

  sql_obfuscate_testcase("null sql", 0, 0);
  sql_obfuscate_testcase("empty sql", "", "");
  sql_obfuscate_testcase("single digit", "0", "?");
  sql_obfuscate_testcase("empty single quote string", "''", "?");
  sql_obfuscate_testcase("unterminated single quote", "'", "?");
  sql_obfuscate_testcase("unterminated double quote", "\"", "?");
  sql_obfuscate_testcase("adjacent empty single quote strings", "''''", "?");
  sql_obfuscate_testcase("empty double quote string", "\"\"", "?");
  sql_obfuscate_testcase("adjacent empty double quote strings", "\"\"\"\"",
                         "?");

  sql_obfuscate_testcase("multiple numbers",
                         "SELECT * FROM test WHERE foo IN (1,2,3)",
                         "SELECT * FROM test WHERE foo IN (?,?,?)");

  sql_obfuscate_testcase("single and double quotes (empty)",
                         "SELECT * FROM test WHERE foo IN (1,\"\",'')",
                         "SELECT * FROM test WHERE foo IN (?,?,?)");

  sql_obfuscate_testcase("single and double quotes (nonempty)",
                         "SELECT * FROM test WHERE foo IN (1,\"foo\",'baz')",
                         "SELECT * FROM test WHERE foo IN (?,?,?)");

  sql_obfuscate_testcase("escaped quotes",
                         "SELECT * FROM test WHERE foo IN (1,\"\\\"\",'\\'')",
                         "SELECT * FROM test WHERE foo IN (?,?,?)");

  sql_obfuscate_testcase("stuttered quotes",
                         "SELECT * FROM test WHERE foo IN (1,\"\"\"\",'''',14)",
                         "SELECT * FROM test WHERE foo IN (?,?,?,?)");

  sql_obfuscate_testcase(
      "missing closing double quote",
      "SELECT * FROM test WHERE foo IN (1,\"missing closing double quote)",
      "SELECT * FROM test WHERE foo IN (?,?");

  sql_obfuscate_testcase(
      "missing closing single quote",
      "SELECT * FROM test WHERE foo IN (1,\'missing closing single quote)",
      "SELECT * FROM test WHERE foo IN (?,?");

  sql_obfuscate_testcase(
      "digit strings", "SELECT 12345 FROM test WHERE foo IN (1,\"foo\",'baz')",
      "SELECT ? FROM test WHERE foo IN (?,?,?)");

  sql_obfuscate_testcase(
      "floating point number",
      "SELECT 12345.78 FROM test WHERE foo IN (1,\"foo\",'baz')",
      "SELECT ?.? FROM test WHERE foo IN (?,?,?)");

  sql_obfuscate_testcase(
      "floating point number with exponent",
      "SELECT 12345.78e01 FROM test WHERE foo IN (1,\"foo\",'baz')",
      "SELECT ?.?e? FROM test WHERE foo IN (?,?,?)");

  sql_obfuscate_testcase(
      "Comment, SQL style",
      "SELECT * FROM PASSWORDS -- hunter2 -- WHERE foo IN (1)",
      "SELECT * FROM PASSWORDS ");

    sql_obfuscate_testcase(
            "Comment, SQL style on two lines",
            "SELECT * FROM PASSWORDS -- hunter2\n -- WHERE foo IN (1)",
            "SELECT * FROM PASSWORDS  ");

    sql_obfuscate_testcase(
            "Comment, SQL style, next line ok",
            "SELECT * FROM PASSWORDS -- hunter2\nWHERE foo IN (1)",
            "SELECT * FROM PASSWORDS WHERE foo IN (?)");

  sql_obfuscate_testcase(
      "Comment, C style",
      "SELECT * FROM PASSWORDS /* hunter2 */ WHERE foo IN (1)",
      "SELECT * FROM PASSWORDS  WHERE foo IN (?)");

    sql_obfuscate_testcase(
            "Comment, C style, nested",
            "SELECT * FROM PASSWORDS /* /** hunter2 */ WHERE */ foo IN (1)",
            "SELECT * FROM PASSWORDS  WHERE */ foo IN (?)");

  sql_obfuscate_testcase("C-style comment start alone", "/*", "");
  sql_obfuscate_testcase("SQL-style comment start alone", "--", "");
  sql_obfuscate_testcase("Half of a C-style comment alone", "/", "/");
  sql_obfuscate_testcase("Half of a SQL-style comment alone", "-", "-");

  sql_obfuscate_testcase("Half of a C-style comment delimiter at end",
                         "some string /", "some string /");

  sql_obfuscate_testcase("Half of a SQL-style comment delimiter at end",
                         "some string -", "some string -");

  sql_obfuscate_testcase("Only comment start (C style)",
                         "SELECT * /* FROM PASSWORDS WHERE (\"\")",
                         "SELECT * ");

  sql_obfuscate_testcase(
      "Mixed comments",
      "SELECT * -- FROM PASSWORDS /* hunter2 */ WHERE foo IN (1)",
      "SELECT * ");

  sql_obfuscate_testcase("Half of a SQL comment delimiter.",
                         " not - - a-comment-", " not - - a-comment-");

  sql_obfuscate_testcase("Broken C-style comment delimiter.",
                         " not / *a/comment */", " not / *a/comment */");

  sql_obfuscate_testcase("Comment start inside double quotes",
                         "SELECT * /* FROM PASSWORDS WHERE foo IN (\"/*\")",
                         "SELECT * ");

  sql_obfuscate_testcase("Comment start inside double quotes, C-style",
                         "SELECT * FROM PASSWORDS WHERE foo IN (\"/*\")",
                         "SELECT * FROM PASSWORDS WHERE foo IN (?)");

  sql_obfuscate_testcase("Comment start inside double quotes, SQL-style",
                         "SELECT * FROM PASSWORDS WHERE foo IN (\"--\")",
                         "SELECT * FROM PASSWORDS WHERE foo IN (?)");

  sql_obfuscate_testcase(
      "C-style comment start inside single quotes, comment outside.",
      "SELECT * FROM PASSWORDS WHERE foo IN (\"/*\" /* HIDING */)",
      "SELECT * FROM PASSWORDS WHERE foo IN (? )");

  sql_obfuscate_testcase(
      "SQL-style comment start inside single quotes, comment outside.",
      "SELECT * FROM PASSWORDS WHERE foo IN (\"--\" --)",
      "SELECT * FROM PASSWORDS WHERE foo IN (? ");

  sql_obfuscate_testcase(
      "C-style comment start inside single quotes, comment end only outside.",
      "SELECT * FROM PASSWORDS WHERE foo IN (\"/*\" HIDING */)",
      "SELECT * FROM PASSWORDS WHERE foo IN (? HIDING */)");

  sql_obfuscate_testcase(
      "escaped quotes with comments",
      "SELECT * FROM test WHERE foo IN (1,\"--\\\"\",'/*\\'')",
      "SELECT * FROM test WHERE foo IN (?,?,?)");

  sql_obfuscate_testcase(
      "stuttered quotes with comments",
      "SELECT * FROM test WHERE foo IN (1,\"--,/*\"\"\",'''/*',14)",
      "SELECT * FROM test WHERE foo IN (?,?,?,?)");

  /*
   * Monstrous integers don't cause us to topple over.
   */
  for (lg = 1; lg < 20; lg++) {
    int byte_length = 1 << lg;
    char* buffer = (char*)nr_malloc(byte_length);

    nr_memset(buffer, '1', byte_length);
    buffer[byte_length - 1] = 0;
    s1 = nr_sql_obfuscate(buffer);
    s2 = "?";
    tlib_pass_if_true("monstrous obfuscation", 0 == nr_strcmp(s1, s2),
                      "s1=%s s2=%s", NRSAFESTR(s1), NRSAFESTR(s2));
    nr_free(buffer);
    nr_free(s1);
  }
}

static void test_sql_normalize(void) {
  char* s1;
  const char* s2;

  s1 = nr_sql_normalize(0);
  tlib_pass_if_true("null sql", 0 == s1, "s1=%p", s1);

  s1 = nr_sql_normalize("");
  tlib_pass_if_true("null sql", 0 == s1, "s1=%p", s1);

  s1 = nr_sql_normalize("SELECT * FROM test WHERE foo IN (?,?,?)");
  s2 = "SELECT * FROM test WHERE foo IN (?)";
  tlib_pass_if_true("nr_sql_normalize", (0 == nr_strcmp(s1, s2)), "s1=%s s2=%s",
                    s1, s2);
  nr_free(s1);

  s1 = nr_sql_normalize("SELECT * FROM test WHERE foo IN(?,?,?)");
  s2 = "SELECT * FROM test WHERE foo IN(?)";
  tlib_pass_if_true("nr_sql_normalize", (0 == nr_strcmp(s1, s2)), "s1=%s s2=%s",
                    s1, s2);
  nr_free(s1);

  s1 = nr_sql_normalize("SELECT * FROM test WHERE foo IN ( ?, ?    )");
  s2 = "SELECT * FROM test WHERE foo IN (?)";
  tlib_pass_if_true("nr_sql_normalize", (0 == nr_strcmp(s1, s2)), "s1=%s s2=%s",
                    s1, s2);
  nr_free(s1);

  s1 = nr_sql_normalize("IN(1,?,?,1)");
  s2 = "IN(1,?,?,1)";
  tlib_pass_if_true("nr_sql_normalize", (0 == nr_strcmp(s1, s2)), "s1=%s s2=%s",
                    s1, s2);
  nr_free(s1);
}

static void test_find_table_with_from(void) {
  /* Empty table name */
  test_get_operation_and_table(NULL, "SELECT * FROM `` WHERE x > y", "select",
                               NULL);
}

static void test_real_world_things(void) {
  const char* sql;

  /* Real-world stuff that has caused errors */
  sql
      = "(SELECT SQL_CALC_FOUND_ROWS c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` "
        "FROM `hp_comments` c, `mt_entry` e , mt_entry_extra ee WHERE "
        "e.`entry_id` = c.`entry_id` AND `ee`.`entry_extra_id` = "
        "`e`.`entry_id` AND `published` = ? AND `removed` = ? AND `user_id` = "
        "? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "UNION ALL (SELECT c., e.entry_id, e.entry_title, "
        "`ee`.`entry_extra_image` AS `entry_image` FROM `HPCommentsArchive?` "
        "c, "
        "`mt_entry` e , mt_entry_extra ee WHERE e.`entry_id` = c.`entry_id` "
        "AND `ee`.`entry_extra_id` = `e`.`entry_id` AND `published` = ? AND "
        "`removed` = ? AND `user_id` = ? ) "
        "ORDER BY `created_on` DESC LIMIT ?, ? / app?.nyc.huffpo.net, slave-db "
        "/";
  /*
   * This test does not find "select", "hp_comments" because the SQL string
   * does not start with the operation.
   */
  test_get_operation_and_table("Huffington Post Bad Parse 1", sql, NULL, NULL);

  sql
      = "SELECT `mt_entry`.`entry_id`, `mt_entry`.`entry_title`, "
        "`mt_entry`.`entry_blog_id`, `mt_entry`.`entry_basename`, "
        "`mt_entry_extra`.`entry_extra_image`, `mt_entry`.`entry_author_id`, "
        "`mt_entry`.`entry_created_on`, "
        "`mt_author`.`author_name`, `mt_author`.`author_nickname` FROM "
        "`hp_prod`.`mt_entry` as `mt_entry` "
        "INNER JOIN `hp_prod`.`mt_objecttag` as `mt_objecttag` ON "
        "`mt_objecttag`.`objecttag_object_id` = `mt_entry`.`entry_id` "
        "AND `mt_objecttag`.`objecttag_tag_id` = ? AND "
        "`mt_objecttag`.`objecttag_object_id` "
        "NOT IN ("
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?) "
        "INNER JOIN `hp_prod`.`mt_author` as `mt_author` ON "
        "`mt_author`.`author_id` = `mt_entry`.`entry_author_id` "
        "INNER JOIN `hp_prod`.`mt_entry_extra` as `mt_entry_extra` ON "
        "`mt_entry_extra`.`entry_extra_id` = `mt_entry`.`entry_id` "
        "AND `mt_entry_extra`.`entry_extra_image` LIKE \"?\" INNER JOIN "
        "`hp_prod`.`mt_placement` as `mt_placement` "
        "ON `mt_placement`.`placement_entry_id` = `mt_entry`.`entry_id` AND "
        "`mt_placement`.`placement_category_id` "
        "IN (?, ?) AND `mt_placement`.`placement_is_primary` = ? WHERE "
        "`mt_entry`.`entry_created_on` > \"?\" "
        "AND `mt_entry`.`entry_status` = ? GROUP BY `mt_entry`.`entry_id` "
        "ORDER BY `mt_entry`.`entry_created_on` DESC LIMIT ? / "
        "app?.ewr.huffpo.net, slave-db /";
  test_get_operation_and_table("Huffington Post Bad Parse 2", sql, "select",
                               "mt_entry");

  sql
      = "SELECT SQL_NO_CACHE `stats`.`entity_id` as `entry_id`, "
        "COUNT(`stats`.`count`) as `c`, "
        "GROUP_CONCAT(DISTINCT `stats`.`user_id`) as `friends_ids`, "
        "COUNT(DISTINCT `stats`.`user_id`) "
        "as `friends_count`, `ee`.`entry_extra_image` as `entry_image` FROM "
        "`hp_prod`.`stats_user_actions` "
        "as `stats` INNER JOIN `hp_prod`.`mt_entry` as `e` ON `e`.`entry_id` = "
        "`stats`.`entity_id` AND "
        "`e`.`entry_created_on` > NOW() - INTERVAL ? DAY LEFT JOIN "
        "`hp_prod`.`mt_entry_extra` as "
        "`ee` ON `ee`.`entry_extra_id` = `stats`.`entity_id` AND "
        "`ee`.`entry_extra_image` LIKE \"?\" WHERE `stats`.`user_id` IN";
  test_get_operation_and_table("Huffington Post Bad Parse 3", sql, "select",
                               "stats_user_actions");

  sql
      = "select City.name, Country.name, City.population from (Country, City) "
        "where Country.population > ? and City.population > ?";
  test_get_operation_and_table("Richard multiple-table test", sql, "select",
                               "Country");

  sql
      = "SELECT CONV(SUBSTRING(MD5(LOWER('what breast pathology involves "
        "malignant cells with halos invading the epidermis of the skin?')), 1, "
        "8), 16, 10)";
  test_get_operation_and_table("Quizlet sub-string bad parse 1", sql, "select",
                               NULL);

  sql
      = "SELECT CONV(SUBSTRING(MD5(LOWER('Who is the Thrilla from Manila?')), "
        "1, 8), 16, 10)";
  test_get_operation_and_table("Quizlet sub-string bad parse 2", sql, "select",
                               NULL);

  sql
      = " SELECT n.message, n.subject, ne.name as email_name, ne.email as "
        "email_from FROM notifications "
        "n INNER JOIN notification_emails ne ON ne.id = "
        "n.notification_email_id WHERE n.notification_event_id = :event_id AND "
        "n.locale_id = :locale_id AND n.is_active = ? LIMIT ? ";
  test_get_operation_and_table("_from in alias", sql, "select",
                               "notifications");
}

/*
 * Read the section on identifiers and comments carefully:
 *   https://dev.mysql.com/doc/refman/5.0/en/identifiers.html
 *   https://dev.mysql.com/doc/refman/5.1/en/comments.html
 */
static void test_diabolical_quoting(void) {
  const char* sql;

  sql = " SELECT foo from (Country, City);";
  test_get_operation_and_table("other test 1", sql, "select", "Country");

  sql = " SELECT foo /* from (County) */ from (Country, City);";
  test_get_operation_and_table("other test 1a", sql, "select", "Country");

  sql = " /* SELECT foo from (City)*/ SELECT foo from (County, City);";
  test_get_operation_and_table("other test 1b", sql, "select", "County");

#if 0 /* does not handle comments in arbitrary places */
  sql = " SELECT foo from (/*Country*/County, City);";
  test_get_operation_and_table ("other test 1c", sql, "select", "County");
#endif

#if 0 /* does not handle -- or # comment to end of line syntax */
  sql = " SELECT foo -- from (County) \n from (Country, City);";
  test_get_operation_and_table ("other test 1d", sql, "select", "Country");

  sql = " SELECT foo # from (County) \n from (Country, City);";
  test_get_operation_and_table ("other test 1e", sql, "select", "Country");
#endif

  sql = " SELECT ffrom from (Country, City);";
  test_get_operation_and_table("other test 2", sql, "select", "Country");

  sql = " SELECT fffrom from (Country, City);";
  test_get_operation_and_table("fffrom", sql, "select", "Country");

  sql = " SELECT fromm from (Country, City);";
  test_get_operation_and_table("other test 3", sql, "select", "Country");

  sql = " SELECT `from` from (Country, City);";
  test_get_operation_and_table("other test 4", sql, "select", "Country");

  sql = " SELECT `from` from (`from`, `select`);";
  test_get_operation_and_table("other test 5a", sql, "select", "from");

#if 0 /* does not handle spaces in ` quoted identifiers */
  sql = " SELECT `from` from (`A A A`, `select`);";
  test_get_operation_and_table ("other test 5b", sql, "select", "A A A");
#endif

  /* test a little non ASCII (Cryllic in this case) */
  sql = " SELECT `колонка` from (стол, City);";
  test_get_operation_and_table("other test 6", sql, "select", "стол");

  sql = " SELECT `a b` from (Country, City);";
  test_get_operation_and_table("other test 7", sql, "select", "Country");

  sql = " SELECT `afrom fromb` from (Country, City);";
  test_get_operation_and_table("other test 8", sql, "select", "Country");

  sql = " SELECT `from a` from (Country, City);";
  test_get_operation_and_table("other test 9", sql, "select", "Country");

#if 0 /* spaces in quotes before keywords are not handled */
  sql = " SELECT `a from` from (Country, City);";
  test_get_operation_and_table ("other test 10", sql, "select", "Country");

  sql = " SELECT `from from` from (Country, City);";
  test_get_operation_and_table ("other test 11", sql, "select", "Country");
#endif

#if 0 /* ANSI_QUOTES are not handled */
  sql = " SELECT \"foobar\" from (Country, City);";  /* test of behavior when ANSI_QUOTES SQL mode is enabled */
  test_get_operation_and_table ("other test 12", sql, "select", "Country");

  sql = " SELECT \"from\" from (Country, City);";  /* test of behavior when ANSI_QUOTES SQL mode is enabled */
  test_get_operation_and_table ("other test 13", sql, "select", "Country");
#endif

  sql = " SELECT foo from (\"Region\", City);"; /* oddly, accepts this */
  test_get_operation_and_table("other test 13b", sql, "select", "Region");

#if 0 /* does not accept spaces in ANSI_QUOTES strings */
  sql = " SELECT foo from (\"Region Continent\", City);";
  test_get_operation_and_table ("other test 13b", sql, "select", "Region Continent");
#endif

  sql = " SELECT foo from (`Country`, City);";
  test_get_operation_and_table("other test 1", sql, "select", "Country");

  sql = " SELECT foo from (7UP, City);";
  test_get_operation_and_table("other test 1", sql, "select", "7UP");

  sql = " SELECT foo from (7, City);"; /* not really a legal mysql identifier */
  test_get_operation_and_table("other test 1", sql, "select", "7");

  sql = " SELECT foo from (`7`, City);";
  test_get_operation_and_table("other test 1", sql, "select", "7");
}

static void test_get_operation_and_table_in_sql_with_info(void) {
  const char* sql;

  /*
   * This test does not find "insert", "baz" because the operation must be at
   * the beginning of the SQL.
   */
  test_get_operation_and_table(
      NULL, "IINTO foobar INSERT InTo baz(a,b) VALUES(1,2)", NULL, NULL);

  /* Real-world stuff that has caused errors */
  sql
      = "INSERT INTO "
        "gm1_gross_margin_report_audit(id,parent_id,field_name,data_type,"
        "before_value_string,after_value_string,date_created,created_by) "
        "VALUES('?','?','?','?','?','?','?','?')";
  test_get_operation_and_table("Bjorn's customer test", sql, "insert",
                               "gm1_gross_margin_report_audit");
}

static void test_weird_and_wonderful(void) {
  const char* sql;

  /* Caused Magento SIGSEGV */
  sql
      = "SELECT `main_table`.*, `main_table`.`total_item_count` AS "
        "`items_count`, CONCAT(main_table.customer_firstname,\" \", "
        "main_table.customer_lastname) AS `customer`, "
        "(main_table.base_grand_total * main_table.base_to_global_rate) AS "
        "`revenue` "
        "FROM `sales_flat_order` AS `main_table` ORDER BY created_at DESC "
        "LIMIT 5";
  test_get_operation_and_table("Magento SIGSEGV", sql, "select",
                               "sales_flat_order");

  /* Caused SIGSEGV for user barry in a support ticket */
  sql
      = "select imageclass,concat(imageclass,' [',c,']') from category_stat "
        "where c > 15 order by rand() limit 5";
  test_get_operation_and_table("Barry SIGSEGV", sql, "select", "category_stat");

  sql
      = "SELECT r.nid, MATCH(r.body, r.title) AGAINST ('%s') AS score FROM "
        "{node_revisions} r "
        "INNER JOIN {node} n ON r.nid = n.nid AND r.vid = n.vid INNER JOIN "
        "{term_node} t ON n.nid = t.nid AND t.tid IN (%s) "
        "WHERE n.status <> 0 AND r.nid <> %d AND n.type IN ($types) GROUP BY "
        "n.nid HAVING score > 0 ORDER BY score DESC, r.vid DESC";
  test_get_operation_and_table("Table in braces", sql, "select",
                               "node_revisions");

  /* Causes lots of warnings about illegal characters - need to parse comments
   */
  sql
      = "UPDATE /* 1.2.3.4 */ `iw_page` SET page_counter = page_counter + 1 "
        "WHERE page_id = 824' rv=''";
  test_get_operation_and_table("C-style comments in SQL", sql, "update",
                               "iw_page");

  sql = "SELECT * FROM /* zip */ /* zap */ /* zop */  alpha";
  test_get_operation_and_table("multiple comments before table name in select",
                               sql, "select", "alpha");

  sql
      = "UPDATE /* zip */ /* zap */ /* zop */ alpha SET page_counter = "
        "page_counter + 1 WHERE page_id = 824' rv=''";
  test_get_operation_and_table("multiple comments before table name in update",
                               sql, "update", "alpha");

  sql
      = "/* zip */ /* zap */ /* zop */ UPDATE alpha SET page_counter = "
        "page_counter + 1 WHERE page_id = 824' rv=''";
  test_get_operation_and_table("multiple comments before update in update", sql,
                               "update", "alpha");

  sql
      = "/* zip */ UPDATE /* zap */ alpha SET page_counter = page_counter + 1 "
        "WHERE page_id = 824' rv=''";
  test_get_operation_and_table("comment before and after update", sql, "update",
                               "alpha");

  sql = "update";
  test_get_operation_and_table("single update", sql, "update", NULL);

  sql = "update alpha";
  test_get_operation_and_table("simple update", sql, "update", "alpha");

  sql = "update /* alpha";
  test_get_operation_and_table("unterminated comment before update tablename",
                               sql, "update", NULL);

  sql = "select from";
  test_get_operation_and_table("select from", sql, "select", NULL);

  sql = "insert into";
  test_get_operation_and_table("insert into", sql, "insert", NULL);

  sql = "insert";
  test_get_operation_and_table("insert", sql, "insert", NULL);

  sql = "select";
  test_get_operation_and_table("select", sql, "select", NULL);

  sql = "alpha";
  test_get_operation_and_table("alpha", sql, NULL, NULL);
}

static void test_unterminated(void) {
  const char* sql;

  sql = " /* SELECT * FROM alpha";
  test_get_operation_and_table("unterminated comment", sql, NULL, NULL);

  sql = " SELECT /* * FROM alpha";
  test_get_operation_and_table("unterminated comment", sql, "select", NULL);

  sql = " SELECT * /* FROM alpha";
  test_get_operation_and_table("unterminated comment", sql, "select", NULL);

  sql = " SELECT * FROM /* alpha";
  test_get_operation_and_table("unterminated comment", sql, "select", NULL);

  sql = " SELECT * \" FROM alpha";
  test_get_operation_and_table("unterminated \"", sql, "select", NULL);

  sql = " SELECT * ' FROM alpha";
  test_get_operation_and_table("unterminated '", sql, "select", NULL);
}

static void test_get_operation_and_table_bad_params(void) {
  const char* sql = "SELECT * FROM alpha";
  const char* operation = 0;
  char* table = 0;
  int show_sql_parsing = 1;

  nr_sql_get_operation_and_table(0, 0, 0, show_sql_parsing); /* Don't blow up */

  nr_sql_get_operation_and_table(sql, 0, 0,
                                 show_sql_parsing); /* Don't blow up */

  nr_sql_get_operation_and_table(sql, &operation, 0, show_sql_parsing);
  tlib_pass_if_true("null table ptr", 0 == operation, "operation=%p",
                    operation);
  tlib_pass_if_true("null table ptr", 0 == table, "table=%p", table);

  nr_sql_get_operation_and_table(sql, 0, &table, show_sql_parsing);
  tlib_pass_if_true("null operation ptr", 0 == operation, "operation=%p",
                    operation);
  tlib_pass_if_true("null operation ptr", 0 == table, "table=%p", table);

  nr_sql_get_operation_and_table(0, &operation, &table, show_sql_parsing);
  tlib_pass_if_true("null sql", 0 == operation, "operation=%p", operation);
  tlib_pass_if_true("null sql", 0 == table, "table=%p", table);

  nr_sql_get_operation_and_table(sql, &operation, &table, show_sql_parsing);
  tlib_pass_if_true("tests valid", 0 != operation, "operation=%p", operation);
  tlib_pass_if_true("tests valid", 0 != table, "table=%p", table);
  nr_free(table);
}

static void test_sql_parsing(void) {
  char* json = 0;
  nrobj_t* array = 0;
  nrotype_t otype;
  int i;

#define SQL_PARSING_TEST_FILE CROSS_AGENT_TESTS_DIR "/sql_parsing.json"
  json = nr_read_file_contents(SQL_PARSING_TEST_FILE, 10 * 1000 * 1000);
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
      const char* table = nro_get_hash_string(hash, "table", 0);
      const char* operation = nro_get_hash_string(hash, "operation", 0);

      if (input && table && operation) {
        test_get_operation_and_table(testname ? testname : input, input,
                                     operation, table);
      }
    }
  }

  nro_delete(array);
  nr_free(json);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_find_table_with_from();
  test_real_world_things();
  test_diabolical_quoting();
  test_get_operation_and_table_in_sql_with_info();
  test_weird_and_wonderful();
  test_whitespace_comment_prefix();
  test_sql_obfuscate();
  test_sql_normalize();
  test_unterminated();
  test_get_operation_and_table_bad_params();
  test_sql_parsing();
}
