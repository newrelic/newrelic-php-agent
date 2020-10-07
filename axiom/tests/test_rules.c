/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_rules.h"
#include "nr_rules_private.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_text.h"

#include "tlib_main.h"

#define rules_apply_testcase(...) \
  rules_apply_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void rules_apply_testcase_fn(const char* testname,
                                    const nrrules_t* rules,
                                    const char* input,
                                    const char* expected,
                                    const char* file,
                                    int line) {
  nr_rules_result_t rv;
  char* output = 0;

  rv = nr_rules_apply(rules, input, &output);

  if (0 == nr_strcmp(input, expected)) {
    if (NR_RULES_RESULT_CHANGED == (int)rv) {
      /*
       * NR_RULES_RESULT_CHANGED expected and rule makes input and output the
       * same.
       */
      test_pass_if_true(testname, NR_RULES_RESULT_CHANGED == rv, "rv=%d",
                        (int)rv);
      test_pass_if_true(testname, 0 == nr_strcmp(output, expected),
                        "output=%s expected=%s", NRSAFESTR(output),
                        NRSAFESTR(expected));
    } else {
      /*
       * NR_RULES_RESULT_UNCHANGED expected.
       */
      test_pass_if_true(testname, NR_RULES_RESULT_UNCHANGED == rv, "rv=%d",
                        (int)rv);
      test_pass_if_true(testname, 0 == output, "output=%p", output);
    }

  } else if (0 == expected) {
    /*
     * NR_RULES_RESULT_IGNORE expected.
     */
    test_pass_if_true(testname, NR_RULES_RESULT_IGNORE == rv, "rv=%d", (int)rv);
    test_pass_if_true(testname, 0 == output, "output=%p", output);
  } else {
    /*
     * NR_RULES_RESULT_CHANGED expected.
     */
    test_pass_if_true(testname, NR_RULES_RESULT_CHANGED == rv, "rv=%d",
                      (int)rv);
    test_pass_if_true(testname, 0 == nr_strcmp(output, expected),
                      "output=%s expected=%s", NRSAFESTR(output),
                      NRSAFESTR(expected));
  }

  nr_free(output);
}

#define rule_parsing_testcase(N, R, M, P, O, F) \
  rule_parsing_testcase_fn((N), (R), (M), (P), (O), (F), __FILE__, __LINE__)

static void rule_parsing_testcase_fn(const char* testname,
                                     const nrrules_t* rules,
                                     const char* match,
                                     const char* replacement,
                                     int order,
                                     int rflags,
                                     const char* file,
                                     int line) {
  test_pass_if_true(testname, rules && rules->rules && (1 == rules->nrules),
                    "rules=%p", rules);

  if (rules && rules->rules && (1 == rules->nrules)) {
    nrrule_t* r = rules->rules;

    test_pass_if_true(testname, 0 == nr_strcmp(match, r->match),
                      "match=%s r->match=%s", match, r->match);
    test_pass_if_true(testname, 0 == nr_strcmp(replacement, r->replacement),
                      "replacement=%s r->replacement=%s", replacement,
                      r->replacement);
    test_pass_if_true(testname, order == r->order, "order=%d r->order=%d",
                      order, r->order);
    test_pass_if_true(testname, rflags == r->rflags, "rflags=%d r->rflags=%d",
                      rflags, r->rflags);
    test_pass_if_true(testname, NULL != r->regex, "r->regex=%p", r->regex);
  }
}

static nrrules_t* build_rules(const char* str) {
  nrobj_t* ob = nro_create_from_json(str);
  nrrules_t* rs = nr_rules_create_from_obj(ob);

  nro_delete(ob);
  return rs;
}

static void test_rule_parsing(void) {
  nrrules_t* ur;

  /*
   * Test : Rule parsing and creation.
   *
   * These are the default url_rules as sent by the collector.
   *
   * One escape '\' is needed to create the string in the test source and a 
   * second '\' is needed for the JSON parsing.  Hence the \\\\1.
   */
  ur = build_rules(
      "["
      "{\"match_expression\":\"^(test_match_nothing)$\","
      "\"replacement\":\"\\\\1\","
      "\"each_segment\":false, "
      "\"eval_order\":0, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":true}]");
  rule_parsing_testcase("default rule", ur, "^(test_match_nothing)$", "\\1", 0,
                        NR_RULE_TERMINATE | NR_RULE_HAS_CAPTURES);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\"^(test_match_nothing)$\","
      "\"replacement\":\"\\\\1\","
      "\"each_segment\":false, "
      "\"eval_order\":0, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":true}]");
  rule_parsing_testcase("default rule", ur, "^(test_match_nothing)$", "\\1", 0,
                        NR_RULE_TERMINATE | NR_RULE_HAS_CAPTURES);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\".*\\\\.(css|gif|ico|jpe?g|js|png|swf)$\","
      "\"replacement\":\"\\/*.\\\\1\","
      "\"each_segment\":false, "
      "\"eval_order\":0, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":true}]");
  rule_parsing_testcase(
      "default rule", ur, ".*\\.(css|gif|ico|jpe?g|js|png|swf)$", "/*.\\1", 0,
      NR_RULE_TERMINATE | NR_RULE_HAS_CAPTURES | NR_RULE_HAS_ALTS);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\".*\\\\.(css|gif|ico|jpe?g|js|png|swf)$\","
      "\"replacement\":\"\\/*.\\\\1\","
      "\"each_segment\":false, "
      "\"eval_order\":0, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":true}]");
  rule_parsing_testcase(
      "default rule", ur, ".*\\.(css|gif|ico|jpe?g|js|png|swf)$", "/*.\\1", 0,
      NR_RULE_TERMINATE | NR_RULE_HAS_CAPTURES | NR_RULE_HAS_ALTS);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\"^(test_match_nothing)$\","
      "\"replacement\":\"\\\\1\","
      "\"each_segment\":false, "
      "\"eval_order\":0, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":true}]");
  rule_parsing_testcase("default rule", ur, "^(test_match_nothing)$", "\\1", 0,
                        NR_RULE_TERMINATE | NR_RULE_HAS_CAPTURES);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\".*\\\\.(css|gif|ico|jpe?g|js|png|swf)$\","
      "\"replacement\":\"\\/*.\\\\1\","
      "\"each_segment\":false, "
      "\"eval_order\":0, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":true}]");
  rule_parsing_testcase(
      "default rule", ur, ".*\\.(css|gif|ico|jpe?g|js|png|swf)$", "/*.\\1", 0,
      NR_RULE_TERMINATE | NR_RULE_HAS_CAPTURES | NR_RULE_HAS_ALTS);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\"^(test_match_nothing)$\","
      "\"replacement\":\"\\\\1\","
      "\"each_segment\":false, "
      "\"eval_order\":0, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":true}]");
  rule_parsing_testcase("default rule", ur, "^(test_match_nothing)$", "\\1", 0,
                        NR_RULE_TERMINATE | NR_RULE_HAS_CAPTURES);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\".*\\\\.(css|gif|ico|jpe?g|js|png|swf)$\","
      "\"replacement\":\"\\/*.\\\\1\","
      "\"each_segment\":false, "
      "\"eval_order\":0, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":true}]");
  rule_parsing_testcase(
      "default rule", ur, ".*\\.(css|gif|ico|jpe?g|js|png|swf)$", "/*.\\1", 0,
      NR_RULE_TERMINATE | NR_RULE_HAS_CAPTURES | NR_RULE_HAS_ALTS);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\"^[0-9][0-9a-f_,.-]*$\","
      "\"replacement\":\"*\","
      "\"each_segment\":true,  "
      "\"eval_order\":1, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":false}]");
  rule_parsing_testcase("default rule", ur, "^[0-9][0-9a-f_,.-]*$", "*", 1,
                        NR_RULE_EACH_SEGMENT);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\"^[0-9][0-9a-f_,.-]*$\","
      "\"replacement\":\"*\","
      "\"each_segment\":true,  "
      "\"eval_order\":1, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":false}]");
  rule_parsing_testcase("default rule", ur, "^[0-9][0-9a-f_,.-]*$", "*", 1,
                        NR_RULE_EACH_SEGMENT);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\"^[0-9][0-9a-f_,.-]*$\","
      "\"replacement\":\"*\","
      "\"each_segment\":true,  "
      "\"eval_order\":1, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":false}]");
  rule_parsing_testcase("default rule", ur, "^[0-9][0-9a-f_,.-]*$", "*", 1,
                        NR_RULE_EACH_SEGMENT);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\"^[0-9][0-9a-f_,.-]*$\","
      "\"replacement\":\"*\","
      "\"each_segment\":true,  "
      "\"eval_order\":1, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":false}]");
  rule_parsing_testcase("default rule", ur, "^[0-9][0-9a-f_,.-]*$", "*", 1,
                        NR_RULE_EACH_SEGMENT);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\"^(.*)\\/"
      "[0-9][0-9a-f_,-]*\\\\.([0-9a-z][0-9a-z]*)$\","
      "\"replacement\":\"\\\\1\\/.*\\\\2\","
      "\"each_segment\":false, "
      "\"eval_order\":2, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":false}]");
  rule_parsing_testcase("default rule", ur,
                        "^(.*)/[0-9][0-9a-f_,-]*\\.([0-9a-z][0-9a-z]*)$",
                        "\\1/.*\\2", 2, NR_RULE_HAS_CAPTURES);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\"^(.*)\\/"
      "[0-9][0-9a-f_,-]*\\\\.([0-9a-z][0-9a-z]*)$\","
      "\"replacement\":\"\\\\1\\/.*\\\\2\","
      "\"each_segment\":false, "
      "\"eval_order\":2, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":false}]");
  rule_parsing_testcase("default rule", ur,
                        "^(.*)/[0-9][0-9a-f_,-]*\\.([0-9a-z][0-9a-z]*)$",
                        "\\1/.*\\2", 2, NR_RULE_HAS_CAPTURES);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\"^(.*)\\/"
      "[0-9][0-9a-f_,-]*\\\\.([0-9a-z][0-9a-z]*)$\","
      "\"replacement\":\"\\\\1\\/.*\\\\2\","
      "\"each_segment\":false, "
      "\"eval_order\":2, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":false}]");
  rule_parsing_testcase("default rule", ur,
                        "^(.*)/[0-9][0-9a-f_,-]*\\.([0-9a-z][0-9a-z]*)$",
                        "\\1/.*\\2", 2, NR_RULE_HAS_CAPTURES);
  nr_rules_destroy(&ur);

  ur = build_rules(
      "["
      "{\"match_expression\":\"^(.*)\\/"
      "[0-9][0-9a-f_,-]*\\\\.([0-9a-z][0-9a-z]*)$\","
      "\"replacement\":\"\\\\1\\/.*\\\\2\","
      "\"each_segment\":false, "
      "\"eval_order\":2, "
      "\"ignore\":false, "
      "\"replace_all\":false, "
      "\"terminate_chain\":false}]");
  rule_parsing_testcase("default rule", ur,
                        "^(.*)/[0-9][0-9a-f_,-]*\\.([0-9a-z][0-9a-z]*)$",
                        "\\1/.*\\2", 2, NR_RULE_HAS_CAPTURES);
  nr_rules_destroy(&ur);
}

static void test_cross_agent_rule_tests(void) {
  char* json = 0;
  nrobj_t* array = 0;
  nrotype_t otype;
  int i;

#define RULES_TESTS_FILE CROSS_AGENT_TESTS_DIR "/rules.json"
  json = nr_read_file_contents(RULES_TESTS_FILE, 10 * 1000 * 1000);
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
      int j;
      const nrobj_t* hash = nro_get_array_hash(array, i, 0);
      const char* testname = nro_get_hash_string(hash, "testname", 0);
      const nrobj_t* rules_obj = nro_get_hash_array(hash, "rules", 0);
      const nrobj_t* test_cases = nro_get_hash_array(hash, "tests", 0);
      nrrules_t* rules;

      rules = nr_rules_create_from_obj(rules_obj);

      tlib_pass_if_true("tests valid", 0 != rules, "rules=%p", rules);
      tlib_pass_if_true("tests valid", 0 != test_cases, "test_cases=%p",
                        test_cases);

      if (test_cases && (NR_OBJECT_ARRAY == nro_type(test_cases))) {
        for (j = 1; j <= nro_getsize(test_cases); j++) {
          const nrobj_t* h = nro_get_array_hash(test_cases, j, 0);
          const char* input = nro_get_hash_string(h, "input", 0);
          const char* expected = nro_get_hash_string(h, "expected", 0);

          tlib_pass_if_true("tests valid", 0 != h, "h=%p", h);
          tlib_pass_if_true("tests valid", 0 != input, "input=%p", input);

          rules_apply_testcase(testname ? testname : input, rules, input,
                               expected);
        }
      }
      nr_rules_destroy(&rules);
    }
  }

  nro_delete(array);
  nr_free(json);
}

#define process_rule_testcase(...) \
  process_rule_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void process_rule_testcase_fn(const char* json,
                                     int flags,
                                     int order,
                                     const char* match,
                                     const char* replacement,
                                     const char* file,
                                     int line) {
  nrobj_t* rule_obj = nro_create_from_json(json);
  nrrules_t* rules = nr_rules_create(100);

  nr_rules_process_rule(rules, rule_obj);
  test_pass_if_true("test valid", 0 != rule_obj, "rule_obj=%p", rule_obj);

  if (0 == match) {
    /*
     * If 0 == match then it is expected that adding the rule should fail.
     */
    test_pass_if_true("no rule added", 0 == rules->nrules, "json=%s nrules=%d",
                      json, rules->nrules);
  } else {
    nrrule_t* rule = &rules->rules[0];

    test_pass_if_true("number rules increased", 1 == rules->nrules,
                      "json=%s nrules=%d", json, rules->nrules);
    test_pass_if_true("rule added", 0 != rule, "json=%s rule=%p", json, rule);
    if (rule) {
      test_pass_if_true("correct flags", flags == rule->rflags,
                        "json=%s flags=%d rule->rflags=%d", json, flags,
                        rule->rflags);
      test_pass_if_true("correct order", order == rule->order,
                        "json=%s order=%d rule->order=%d", json, order,
                        rule->order);
      test_pass_if_true("correct match", 0 == nr_strcmp(match, rule->match),
                        "json=%s match=%s rule->match=%s", json,
                        NRSAFESTR(match), NRSAFESTR(rule->match));
      test_pass_if_true("correct replacement",
                        0 == nr_strcmp(replacement, rule->replacement),
                        "json=%s replacement=%s rule->replacement=%s", json,
                        NRSAFESTR(replacement), NRSAFESTR(rule->replacement));
    }
  }

  nr_rules_destroy(&rules);
  nro_delete(rule_obj);
}

static void test_process_rule(void) {
  /*
   * Test : Bad Parameters
   */
  /* Don't blow up! */
  nr_rules_process_rule(0, 0);
  /* Wrong type */
  process_rule_testcase("[1,2,3]", 0, 0, 0, 0);
  /* Missing match_expression */
  process_rule_testcase(
      "{\"replace_all\":false,"
      "\"terminate_chain\":true,"
      "\"eval_order\":0,"
      "\"replacement\":\"\\1\","
      "\"each_segment\":false,"
      "\"ignore\":false}",
      0, 0, 0, 0);
  /* Missing replacement */
  process_rule_testcase(
      "{\"match_expression\":\"^(test_match_nothing)$\","
      "\"replace_all\":false,"
      "\"terminate_chain\":true,"
      "\"eval_order\":0,"
      "\"each_segment\":false,"
      "\"ignore\":false}",
      0, 0, 0, 0);
  /*
   * Test : Success
   */
  /* Basic */
  process_rule_testcase(
      "{\"match_expression\":\"alpha\","
      "\"replacement\":\"beta\"}",
      0, NR_RULE_DEFAULT_ORDER, "alpha", "beta");
  /* Each Segment */
  process_rule_testcase(
      "{\"match_expression\":\"alpha\","
      "\"replacement\":\"beta\","
      "\"each_segment\":true}",
      NR_RULE_EACH_SEGMENT, NR_RULE_DEFAULT_ORDER, "alpha", "beta");
  /* Replace All */
  process_rule_testcase(
      "{\"match_expression\":\"alpha\","
      "\"replacement\":\"beta\","
      "\"replace_all\":true}",
      NR_RULE_REPLACE_ALL, NR_RULE_DEFAULT_ORDER, "alpha", "beta");
  /* Each Segment */
  process_rule_testcase(
      "{\"match_expression\":\"alpha\","
      "\"replacement\":\"beta\","
      "\"each_segment\":true}",
      NR_RULE_EACH_SEGMENT, NR_RULE_DEFAULT_ORDER, "alpha", "beta");
  /* Ignore (no replacement) */
  process_rule_testcase(
      "{\"match_expression\":\"alpha\","
      "\"ignore\":true}",
      NR_RULE_IGNORE, NR_RULE_DEFAULT_ORDER, "alpha", 0);
  /* Terminate Chain */
  process_rule_testcase(
      "{\"match_expression\":\"alpha\","
      "\"replacement\":\"beta\","
      "\"terminate_chain\":true}",
      NR_RULE_TERMINATE, NR_RULE_DEFAULT_ORDER, "alpha", "beta");
  /* Eval Order */
  process_rule_testcase(
      "{\"match_expression\":\"alpha\","
      "\"replacement\":\"beta\","
      "\"eval_order\":55}",
      0, 55, "alpha", "beta");
  /* Has Alts */
  process_rule_testcase(
      "{\"match_expression\":\"alpha|gamma\","
      "\"replacement\":\"beta\"}",
      NR_RULE_HAS_ALTS, NR_RULE_DEFAULT_ORDER, "alpha|gamma", "beta");
  /* Has Captures */
  process_rule_testcase(
      "{\"match_expression\":\"alpha\","
      "\"replacement\":\"\\\\0\"}",
      NR_RULE_HAS_CAPTURES, NR_RULE_DEFAULT_ORDER, "alpha", "\\0");
}

static void test_create_from_obj_bad_params(void) {
  nrrules_t* rules;
  nrobj_t* not_hash = nro_new(NR_OBJECT_INT);

  rules = nr_rules_create_from_obj(0);
  tlib_pass_if_true("null param", 0 == rules, "rules=%p", rules);
  rules = nr_rules_create_from_obj(not_hash);
  tlib_pass_if_true("wrong type", 0 == rules, "rules=%p", rules);

  nro_delete(not_hash);
}

static void test_replace_string(void) {
  int study = 1;
  char dest[64];

  const char* pattern = "^.*(abc).*(stu)";
  nr_regex_t* regex = nr_regex_create(
      pattern, NR_REGEX_CASELESS | NR_REGEX_DOLLAR_ENDONLY | NR_REGEX_DOTALL,
      study);

  const char* subject;
  int slen;
  nr_regex_substrings_t* ss = 0;

  subject = "rrabcbqrstuas111";
  slen = nr_strlen(subject);
  ss = nr_regex_match_capture(regex, subject, slen);
  nr_regex_destroy(&regex);

  nr_rule_replace_string("QQ\\2RRR\\1STUV", dest, sizeof(dest), ss);
  tlib_pass_if_str_equal("basic", "QQstuRRRabcSTUV", dest);

  nr_rule_replace_string("\\2\\1", dest, sizeof(dest), ss);
  tlib_pass_if_str_equal("basic", "stuabc", dest);

  nr_rule_replace_string("\\1\\1\\1", dest, sizeof(dest), ss);
  tlib_pass_if_str_equal("basic", "abcabcabc", dest);

  nr_rule_replace_string("", dest, sizeof(dest), ss);
  tlib_pass_if_str_equal("basic", "", dest);

  /*
   * Back substitute everything that matched
   */
  nr_rule_replace_string("\\0", dest, sizeof(dest), ss);
  tlib_pass_if_str_equal("basic", "rrabcbqrstu", dest);

  /*
   * Dest is too small to receive the value
   */
  nr_rule_replace_string("\\1\\1\\1", dest, 2, ss);
  tlib_pass_if_str_equal("basic", "a", dest);

  /*
   * dest is null
   */
  dest[0] = 0;
  nr_rule_replace_string("\\1\\1\\1", 0, sizeof(dest), ss);
  tlib_pass_if_str_equal("basic", "", dest);

  dest[0] = 0;
  nr_rule_replace_string("\\1\\1\\1", dest, 0, ss);
  tlib_pass_if_str_equal("basic", "", dest);

  /*
   * Out of range selector number
   */
  nr_rule_replace_string("\\3", dest, sizeof(dest), ss);
  tlib_pass_if_str_equal("basic", "\\3", dest);

  /*
   * Out of range selector number
   */
  nr_rule_replace_string("\\13", dest, sizeof(dest), ss);
  tlib_pass_if_str_equal("basic", "\\13", dest);

  nr_regex_substrings_destroy(&ss);

  /*
   * 0-length subject
   */
  subject = "";
  slen = nr_strlen(subject);
  ss = nr_regex_match_capture(regex, subject, slen);
  nr_rule_replace_string("\\0", dest, sizeof(dest), ss);
  tlib_pass_if_str_equal("0-length subject", "\\0", dest);
  nr_regex_substrings_destroy(&ss);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_rule_parsing();
  test_process_rule();
  test_create_from_obj_bad_params();
  test_cross_agent_rule_tests();
  test_replace_string();
}
