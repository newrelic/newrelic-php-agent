/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "nr_attributes.h"
#include "nr_attributes_private.h"
#include "nr_txn.h"
#include "util_hash.h"
#include "util_memory.h"
#include "util_object.h"
#include "util_reply.h"
#include "util_strings.h"
#include "util_text.h"

#include "tlib_main.h"

static char* nr_attribute_destination_modifier_to_json(
    const nr_attribute_destination_modifier_t* modifier) {
  nrobj_t* obj;
  char* json;

  if (0 == modifier) {
    return 0;
  }

  obj = nro_new_hash();
  nro_set_hash_boolean(obj, "has_wildcard_suffix",
                       modifier->has_wildcard_suffix);
  nro_set_hash_string(obj, "match", modifier->match);
  nro_set_hash_int(obj, "match_len", modifier->match_len);
  nro_set_hash_long(obj, "match_hash", modifier->match_hash);
  nro_set_hash_long(obj, "include_destinations",
                    modifier->include_destinations);
  nro_set_hash_long(obj, "exclude_destinations",
                    modifier->exclude_destinations);

  json = nro_to_json(obj);
  nro_delete(obj);

  return json;
}

static char* nr_attribute_config_to_json(const nr_attribute_config_t* config) {
  nrobj_t* obj;
  nrobj_t* modifiers_array;
  const nr_attribute_destination_modifier_t* modifier;
  char* json;

  if (0 == config) {
    return 0;
  }
  obj = nro_new_hash();
  modifiers_array = nro_new_array();

  nro_set_hash_long(obj, "disabled_destinations",
                    config->disabled_destinations);

  for (modifier = config->modifier_list; modifier; modifier = modifier->next) {
    char* mod_json = nr_attribute_destination_modifier_to_json(modifier);

    nro_set_array_jstring(modifiers_array, 0, mod_json);
    nr_free(mod_json);
  }

  nro_set_hash(obj, "destination_modifiers", modifiers_array);
  nro_delete(modifiers_array);
  json = nro_to_json(obj);
  nro_delete(obj);

  return json;
}

#define test_modifier_as_json(...) \
  test_modifier_as_json_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_modifier_as_json_fn(
    const char* testname,
    const nr_attribute_destination_modifier_t* modifier,
    const char* expected_json,
    const char* file,
    int line) {
  char* actual_json = nr_attribute_destination_modifier_to_json(modifier);

  test_pass_if_true(testname, 0 == nr_strcmp(expected_json, actual_json),
                    "expected_json=%s actual_json=%s", expected_json,
                    actual_json);
  nr_free(actual_json);
}

#define test_config_as_json(...) \
  test_config_as_json_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_config_as_json_fn(const char* testname,
                                   const nr_attribute_config_t* config,
                                   const char* expected_json,
                                   const char* file,
                                   int line) {
  char* actual_json = nr_attribute_config_to_json(config);

  test_pass_if_true(testname, 0 == nr_strcmp(expected_json, actual_json),
                    "expected_json=%s actual_json=%s", expected_json,
                    actual_json);
  nr_free(actual_json);
}

static void test_destination_modifier_match(void) {
  nr_attribute_destination_modifier_t* modifier;
  uint32_t all = NR_ATTRIBUTE_DESTINATION_ALL;
  int rv;

  rv = nr_attribute_destination_modifier_match(0, "alpha",
                                               nr_mkhash("alpha", 0));
  tlib_pass_if_true("null modifier", 0 == rv, "rv=%d", rv);

  modifier = nr_attribute_destination_modifier_create("alpha", all, all);

  rv = nr_attribute_destination_modifier_match(modifier, "alpha",
                                               nr_mkhash("alpha", 0));
  tlib_pass_if_true("exact match success", 1 == rv, "rv=%d", rv);

  rv = nr_attribute_destination_modifier_match(modifier, "alpha",
                                               nr_mkhash("alpha", 0) + 1);
  tlib_pass_if_true("wrong hash", 0 == rv, "rv=%d", rv);

  rv = nr_attribute_destination_modifier_match(modifier, "alphaa",
                                               nr_mkhash("alpha", 0));
  tlib_pass_if_true("correct hash wrong string", 0 == rv, "rv=%d", rv);

  rv = nr_attribute_destination_modifier_match(modifier, "AlphA",
                                               nr_mkhash("AlphA", 0));
  tlib_pass_if_true("case sensitive", 0 == rv, "rv=%d", rv);

  nr_attribute_destination_modifier_destroy(&modifier);

  modifier = nr_attribute_destination_modifier_create("alpha.*", all, all);

  rv = nr_attribute_destination_modifier_match(modifier, "alpha.beta",
                                               nr_mkhash("alpha.beta", 0));
  tlib_pass_if_true("wildcard match success", 1 == rv, "rv=%d", rv);

  rv = nr_attribute_destination_modifier_match(modifier, "AlPhA.BeTa",
                                               nr_mkhash("AlPhA.BeTa", 0));
  tlib_pass_if_true("case sensitive wildcard match success", 0 == rv, "rv=%d",
                    rv);

  rv = nr_attribute_destination_modifier_match(modifier, "alpha.",
                                               nr_mkhash("alpha.", 0));
  tlib_pass_if_true("wildcard match success (no wildcard chars)", 1 == rv,
                    "rv=%d", rv);

  rv = nr_attribute_destination_modifier_match(modifier, "alpha",
                                               nr_mkhash("alpha", 0));
  tlib_pass_if_true("wildcard match failure", 0 == rv, "rv=%d", rv);

  nr_attribute_destination_modifier_destroy(&modifier);
}

static void test_destination_modifier_apply(void) {
  uint32_t destinations;
  nr_attribute_destination_modifier_t* modifier;
  uint32_t all = NR_ATTRIBUTE_DESTINATION_ALL;

  destinations = nr_attribute_destination_modifier_apply(
      0, "alpha", nr_mkhash("alpha", 0), all);
  tlib_pass_if_true("null modifier", all == destinations, "destinations=%u",
                    destinations);

  modifier = nr_attribute_destination_modifier_create("alpha", all, 0);
  destinations = nr_attribute_destination_modifier_apply(
      modifier, "alpha", nr_mkhash("alpha", 0), 0);
  tlib_pass_if_true("include", all == destinations, "destinations=%u",
                    destinations);
  nr_attribute_destination_modifier_destroy(&modifier);

  modifier = nr_attribute_destination_modifier_create("alpha", 0, all);
  destinations = nr_attribute_destination_modifier_apply(
      modifier, "alpha", nr_mkhash("alpha", 0), all);
  tlib_pass_if_true("exclude", 0 == destinations, "destinations=%u",
                    destinations);
  nr_attribute_destination_modifier_destroy(&modifier);

  modifier = nr_attribute_destination_modifier_create(
      "alpha",
      NR_ATTRIBUTE_DESTINATION_ERROR | NR_ATTRIBUTE_DESTINATION_TXN_TRACE,
      NR_ATTRIBUTE_DESTINATION_ERROR | NR_ATTRIBUTE_DESTINATION_TXN_EVENT);
  destinations = nr_attribute_destination_modifier_apply(
      modifier, "alpha", nr_mkhash("alpha", 0),
      NR_ATTRIBUTE_DESTINATION_TXN_EVENT | NR_ATTRIBUTE_DESTINATION_BROWSER);
  tlib_pass_if_true(
      "include and exclude, exclude has priority",
      (NR_ATTRIBUTE_DESTINATION_TXN_TRACE | NR_ATTRIBUTE_DESTINATION_BROWSER)
          == destinations,
      "destinations=%u", destinations);
  nr_attribute_destination_modifier_destroy(&modifier);
}

static void test_modifier_destroy_bad_params(void) {
  nr_attribute_destination_modifier_t* modifier;

  /* Don't blow up! */
  nr_attribute_destination_modifier_destroy(0);
  modifier = 0;
  nr_attribute_destination_modifier_destroy(&modifier);
}

static void test_disable_destinations(void) {
  nr_attribute_config_t* config;
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  uint32_t error = NR_ATTRIBUTE_DESTINATION_ERROR;

  config = nr_attribute_config_create();
  tlib_pass_if_true("config has no starting disabled destinations",
                    0 == config->disabled_destinations,
                    "config->disabled_destinations=%u",
                    config->disabled_destinations);

  nr_attribute_config_disable_destinations(config, event | error);
  tlib_pass_if_true("destinations successfully disabled",
                    (event | error) == config->disabled_destinations,
                    "config->disabled_destinations=%u",
                    config->disabled_destinations);
  nr_attribute_config_destroy(&config);
}

static void test_destination_modifier_create(void) {
  nr_attribute_destination_modifier_t* modifier;
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  uint32_t error = NR_ATTRIBUTE_DESTINATION_ERROR;

  modifier = nr_attribute_destination_modifier_create(0, event, error);
  tlib_pass_if_true("null match string", 0 == modifier, "modifier=%p",
                    modifier);

  modifier = nr_attribute_destination_modifier_create("alpha", event, error);
  test_modifier_as_json("exact match modifier created", modifier,
                        "{"
                        "\"has_wildcard_suffix\":false,"
                        "\"match\":\"alpha\","
                        "\"match_len\":5,"
                        "\"match_hash\":2000440672,"
                        "\"include_destinations\":1,"
                        "\"exclude_destinations\":4"
                        "}");
  nr_attribute_destination_modifier_destroy(&modifier);

  modifier = nr_attribute_destination_modifier_create("alpha*", event, error);
  test_modifier_as_json("wildcard modifier created", modifier,
                        "{"
                        "\"has_wildcard_suffix\":true,"
                        "\"match\":\"alpha\","
                        "\"match_len\":5,"
                        "\"match_hash\":2000440672,"
                        "\"include_destinations\":1,"
                        "\"exclude_destinations\":4"
                        "}");
  nr_attribute_destination_modifier_destroy(&modifier);
}

static void test_config_modify_destinations(void) {
  nr_attribute_config_t* config;
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  uint32_t trace = NR_ATTRIBUTE_DESTINATION_TXN_TRACE;
  uint32_t error = NR_ATTRIBUTE_DESTINATION_ERROR;
  uint32_t browser = NR_ATTRIBUTE_DESTINATION_BROWSER;

  /* NULL config: Don't blow up! */
  nr_attribute_config_modify_destinations(0, "alpha", event, error);

  config = nr_attribute_config_create();

  nr_attribute_config_modify_destinations(config, "beta.a", event, 0);
  nr_attribute_config_modify_destinations(config, "beta.al", 0, event);
  nr_attribute_config_modify_destinations(config, "beta.a", error, 0);
  nr_attribute_config_modify_destinations(config, "beta.al", 0, error);

  nr_attribute_config_modify_destinations(config, "beta.", browser, 0);
  nr_attribute_config_modify_destinations(config, "beta.*", 0, trace);

  nr_attribute_config_modify_destinations(config, "beta.alpha", 0, browser);

  test_config_as_json("modifiers created and in correct order", config,
                      "{"
                      "\"disabled_destinations\":0,"
                      "\"destination_modifiers\":"
                      "["
                      "{"
                      "\"has_wildcard_suffix\":true,"
                      "\"match\":\"beta.\","
                      "\"match_len\":5,"
                      "\"match_hash\":1419915658,"
                      "\"include_destinations\":0,"
                      "\"exclude_destinations\":2"
                      "},"
                      "{"
                      "\"has_wildcard_suffix\":false,"
                      "\"match\":\"beta.\","
                      "\"match_len\":5,"
                      "\"match_hash\":1419915658,"
                      "\"include_destinations\":8,"
                      "\"exclude_destinations\":0"
                      "},"
                      "{"
                      "\"has_wildcard_suffix\":false,"
                      "\"match\":\"beta.a\","
                      "\"match_len\":6,"
                      "\"match_hash\":4222617845,"
                      "\"include_destinations\":5,"
                      "\"exclude_destinations\":0"
                      "},"
                      "{"
                      "\"has_wildcard_suffix\":false,"
                      "\"match\":\"beta.al\","
                      "\"match_len\":7,"
                      "\"match_hash\":3041978671,"
                      "\"include_destinations\":0,"
                      "\"exclude_destinations\":5"
                      "},"
                      "{"
                      "\"has_wildcard_suffix\":false,"
                      "\"match\":\"beta.alpha\","
                      "\"match_len\":10,"
                      "\"match_hash\":2601622409,"
                      "\"include_destinations\":0,"
                      "\"exclude_destinations\":8"
                      "}"
                      "]"
                      "}");

  nr_attribute_config_destroy(&config);
}

static void test_config_copy(void) {
  nr_attribute_config_t* config;
  nr_attribute_config_t* config_copy;
  char* config_json;
  char* config_copy_json;
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  uint32_t trace = NR_ATTRIBUTE_DESTINATION_TXN_TRACE;
  uint32_t error = NR_ATTRIBUTE_DESTINATION_ERROR;
  uint32_t browser = NR_ATTRIBUTE_DESTINATION_BROWSER;

  config_copy = nr_attribute_config_copy(0);
  tlib_pass_if_true("copy NULL config", 0 == config_copy, "config_copy=%p",
                    config_copy);

  config = nr_attribute_config_create();
  config_copy = nr_attribute_config_copy(config);
  config_json = nr_attribute_config_to_json(config);
  config_copy_json = nr_attribute_config_to_json(config_copy);
  tlib_pass_if_true("empty config copied exactly",
                    0 == nr_strcmp(config_json, config_copy_json),
                    "config_json=%s config_copy_json=%s",
                    NRSAFESTR(config_json), NRSAFESTR(config_copy_json));
  nr_free(config_json);
  nr_free(config_copy_json);
  nr_attribute_config_destroy(&config);
  nr_attribute_config_destroy(&config_copy);

  config = nr_attribute_config_create();

  nr_attribute_config_disable_destinations(config, error | event);

  nr_attribute_config_modify_destinations(config, "beta.a", event, 0);
  nr_attribute_config_modify_destinations(config, "beta.al", 0, event);
  nr_attribute_config_modify_destinations(config, "beta.a", error, 0);
  nr_attribute_config_modify_destinations(config, "beta.al", 0, error);

  nr_attribute_config_modify_destinations(config, "beta.", browser, 0);
  nr_attribute_config_modify_destinations(config, "beta.*", 0, trace);

  config_copy = nr_attribute_config_copy(config);
  config_copy_json = nr_attribute_config_to_json(config_copy);
  config_json = nr_attribute_config_to_json(config);
  tlib_pass_if_true("full config copied exactly",
                    0 == nr_strcmp(config_json, config_copy_json),
                    "config_json=%s config_copy_json=%s",
                    NRSAFESTR(config_json), NRSAFESTR(config_copy_json));
  nr_free(config_json);
  nr_free(config_copy_json);
  nr_attribute_config_destroy(&config);
  nr_attribute_config_destroy(&config_copy);
}

static void test_config_apply(void) {
  nr_attribute_config_t* config;
  uint32_t destinations;
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  uint32_t trace = NR_ATTRIBUTE_DESTINATION_TXN_TRACE;
  uint32_t error = NR_ATTRIBUTE_DESTINATION_ERROR;
  uint32_t browser = NR_ATTRIBUTE_DESTINATION_BROWSER;

  config = nr_attribute_config_create();

  destinations = nr_attribute_config_apply(0, 0, 0, 0);
  tlib_pass_if_true("zero input", 0 == destinations, "destinations=%u",
                    destinations);

  destinations
      = nr_attribute_config_apply(0, "alpha", nr_mkhash("alpha", 0), event);
  tlib_pass_if_true("null config", event == destinations, "destinations=%u",
                    destinations);

  destinations
      = nr_attribute_config_apply(config, 0, nr_mkhash("alpha", 0), event);
  tlib_pass_if_true("null key", 0 == destinations, "destinations=%u",
                    destinations);

  /*
   * Test that the destination modifier are applied in the correct order.
   */
  nr_attribute_config_modify_destinations(config, "alpha.*", browser | trace,
                                          0);
  nr_attribute_config_modify_destinations(config, "alpha.beta", error, browser);

  destinations = nr_attribute_config_apply(config, "alpha.beta",
                                           nr_mkhash("alpha.beta", 0), event);
  tlib_pass_if_true("destinations correctly modified",
                    (trace | error | event) == destinations, "destinations=%u",
                    destinations);

  /*
   * Test that the destination disable is applied after the modifiers.
   */
  nr_attribute_config_disable_destinations(config, trace);
  destinations = nr_attribute_config_apply(config, "alpha.beta",
                                           nr_mkhash("alpha.beta", 0), event);
  tlib_pass_if_true("destinations disabled after modification",
                    (error | event) == destinations, "destinations=%u",
                    destinations);

  nr_attribute_config_destroy(&config);
}

static void test_config_destroy_bad_params(void) {
  nr_attribute_config_t* config;

  /* Don't blow up! */
  nr_attribute_config_destroy(0);
  config = 0;
  nr_attribute_config_destroy(&config);
}

static void test_attribute_destroy_bad_params(void) {
  nr_attribute_t* attribute;

  nr_attribute_destroy(0);
  attribute = 0;
  nr_attribute_destroy(&attribute);
}

static void test_attributes_destroy_bad_params(void) {
  nr_attributes_t* attributes;

  nr_attributes_destroy(0);
  attributes = 0;
  nr_attributes_destroy(&attributes);
}

#define test_user_attributes_as_json(...) \
  test_attributes_as_json_fn(__VA_ARGS__, 1, __FILE__, __LINE__)
#define test_agent_attributes_as_json(...) \
  test_attributes_as_json_fn(__VA_ARGS__, 0, __FILE__, __LINE__)

static void test_attributes_as_json_fn(const char* testname,
                                       const nr_attributes_t* attributes,
                                       uint32_t destinations,
                                       const char* expected_json,
                                       int is_user,
                                       const char* file,
                                       int line) {
  nrobj_t* obj;

  if (is_user) {
    obj = nr_attributes_user_to_obj(attributes, destinations);
  } else {
    obj = nr_attributes_agent_to_obj(attributes, destinations);
  }

  test_obj_as_json_fn(testname, obj, expected_json, file, line);
  nro_delete(obj);
}

static void test_remove_duplicate(void) {
  nr_attributes_t* attributes;
  nr_attribute_config_t* config;
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  uint32_t all = NR_ATTRIBUTE_DESTINATION_ALL;

  config = nr_attribute_config_create();

  attributes = nr_attributes_create(config);
  nr_attributes_user_add_long(attributes, event, "alpha", 1);
  nr_attributes_user_add_long(attributes, event, "alpha", 2);
  test_user_attributes_as_json("only replaced: user", attributes, all,
                               "{\"alpha\":2}");
  nr_attributes_destroy(&attributes);

  attributes = nr_attributes_create(config);
  nr_attributes_agent_add_long(attributes, event, "alpha", 1);
  nr_attributes_agent_add_long(attributes, event, "alpha", 2);
  test_agent_attributes_as_json("only replaced: agent", attributes, all,
                                "{\"alpha\":2}");
  nr_attributes_destroy(&attributes);

  attributes = nr_attributes_create(config);
  nr_attributes_user_add_long(attributes, event, "alpha", 1);
  nr_attributes_user_add_long(attributes, event, "zip", 1);
  nr_attributes_user_add_long(attributes, event, "zap", 1);
  nr_attributes_user_add_long(attributes, event, "alpha", 2);
  test_user_attributes_as_json("first in replaced: user", attributes, all,
                               "{\"alpha\":2,\"zap\":1,\"zip\":1}");
  nr_attributes_destroy(&attributes);

  attributes = nr_attributes_create(config);
  nr_attributes_agent_add_long(attributes, event, "alpha", 1);
  nr_attributes_agent_add_long(attributes, event, "zip", 1);
  nr_attributes_agent_add_long(attributes, event, "zap", 1);
  nr_attributes_agent_add_long(attributes, event, "alpha", 2);
  test_agent_attributes_as_json("first in replaced: agent", attributes, all,
                                "{\"alpha\":2,\"zap\":1,\"zip\":1}");
  nr_attributes_destroy(&attributes);

  attributes = nr_attributes_create(config);
  nr_attributes_user_add_long(attributes, event, "zip", 1);
  nr_attributes_user_add_long(attributes, event, "zap", 1);
  nr_attributes_user_add_long(attributes, event, "alpha", 1);
  nr_attributes_user_add_long(attributes, event, "alpha", 2);
  test_user_attributes_as_json("last in replaced: user", attributes, all,
                               "{\"alpha\":2,\"zap\":1,\"zip\":1}");
  nr_attributes_destroy(&attributes);

  attributes = nr_attributes_create(config);
  nr_attributes_agent_add_long(attributes, event, "zip", 1);
  nr_attributes_agent_add_long(attributes, event, "zap", 1);
  nr_attributes_agent_add_long(attributes, event, "alpha", 1);
  nr_attributes_agent_add_long(attributes, event, "alpha", 2);
  test_agent_attributes_as_json("last in replaced: agent", attributes, all,
                                "{\"alpha\":2,\"zap\":1,\"zip\":1}");
  nr_attributes_destroy(&attributes);

  attributes = nr_attributes_create(config);
  nr_attributes_agent_add_long(attributes, event, "zip", 1);
  nr_attributes_agent_add_long(attributes, event, "zap", 1);
  nr_attributes_agent_add_long(attributes, event, "alpha", 1);
  nr_attributes_remove_duplicate(attributes, "alpha", nr_mkhash("alpha", 0) + 1,
                                 0);
  test_agent_attributes_as_json("hash correctly used to find duplicated",
                                attributes, all,
                                "{\"alpha\":1,\"zap\":1,\"zip\":1}");
  nr_attributes_remove_duplicate(attributes, "alpha", nr_mkhash("alpha", 0), 0);
  test_agent_attributes_as_json("hash correctly used to find duplicated",
                                attributes, all, "{\"zap\":1,\"zip\":1}");
  nr_attributes_destroy(&attributes);

  nr_attribute_config_destroy(&config);
}

static void test_add(void) {
  nr_attributes_t* attributes;
  nr_attribute_config_t* config;
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  uint32_t trace = NR_ATTRIBUTE_DESTINATION_TXN_TRACE;
  uint32_t error = NR_ATTRIBUTE_DESTINATION_ERROR;
  uint32_t browser = NR_ATTRIBUTE_DESTINATION_BROWSER;
  uint32_t all = NR_ATTRIBUTE_DESTINATION_ALL;
  nr_status_t st;
  nrobj_t* obj;

  config = nr_attribute_config_create();
  attributes = nr_attributes_create(config);

  /*
   * Bad params, don't blow up!
   */
  obj = nro_new_string("hello");
  st = nr_attributes_user_add(0, error, "my_key", obj);
  tlib_pass_if_true("NULL attributes", NR_FAILURE == st, "st=%d", (int)st);
  st = nr_attributes_user_add(attributes, error, 0, obj);
  tlib_pass_if_true("NULL key", NR_FAILURE == st, "st=%d", (int)st);
  st = nr_attributes_user_add(attributes, error, "my_key", NULL);
  tlib_pass_if_true("NULL value", NR_FAILURE == st, "st=%d", (int)st);
  nro_delete(obj);

  st = nr_attributes_agent_add_long(0, browser | event, "psi", 123);
  tlib_pass_if_true("bad params", NR_FAILURE == st, "st=%d", (int)st);
  st = nr_attributes_agent_add_string(0, browser | error, "theta", "789");
  tlib_pass_if_true("bad params", NR_FAILURE == st, "st=%d", (int)st);
  st = nr_attributes_agent_add_long(attributes, browser | event, 0, 123);
  tlib_pass_if_true("bad params", NR_FAILURE == st, "st=%d", (int)st);
  st = nr_attributes_agent_add_string(attributes, browser | error, 0, "789");
  tlib_pass_if_true("bad params", NR_FAILURE == st, "st=%d", (int)st);

  /*
   * Valid parameters
   */
  st = nr_attributes_user_add_long(attributes, event, "alpha", 123);
  tlib_pass_if_true("add success", NR_SUCCESS == st, "st=%d", (int)st);
  st = nr_attributes_user_add_long(attributes, trace, "beta", 456);
  tlib_pass_if_true("add success", NR_SUCCESS == st, "st=%d", (int)st);
  st = nr_attributes_user_add_string(attributes, error, "gamma", "789");
  tlib_pass_if_true("add success", NR_SUCCESS == st, "st=%d", (int)st);

  st = nr_attributes_agent_add_long(attributes, browser | event, "psi", 123);
  tlib_pass_if_true("add success", NR_SUCCESS == st, "st=%d", (int)st);
  st = nr_attributes_agent_add_long(attributes, browser | trace, "omega", 456);
  tlib_pass_if_true("add success", NR_SUCCESS == st, "st=%d", (int)st);
  st = nr_attributes_agent_add_string(attributes, browser | error, "theta",
                                      "789");
  tlib_pass_if_true("add success", NR_SUCCESS == st, "st=%d", (int)st);

  st = nr_attributes_agent_add_string(attributes, 0,
                                      "no_destinations_ignore_me", "789");
  tlib_pass_if_true("attribute with no destinations", NR_FAILURE == st, "st=%d",
                    (int)st);

  st = nr_attributes_agent_add_string(attributes, 1 << 10,
                                      "no_valid_destinations_ignore_me", "789");
  tlib_pass_if_true("add success", NR_SUCCESS == st, "st=%d", (int)st);

  test_user_attributes_as_json(
      "user attributes: all", attributes, all,
      "{\"gamma\":\"789\",\"beta\":456,\"alpha\":123}");
  test_agent_attributes_as_json(
      "agent attributes: all", attributes, all,
      "{\"theta\":\"789\",\"omega\":456,\"psi\":123}");

  test_user_attributes_as_json("user attributes: event", attributes, event,
                               "{\"alpha\":123}");
  test_agent_attributes_as_json("agent attributes: event", attributes, event,
                                "{\"psi\":123}");

  test_user_attributes_as_json("user attributes: trace", attributes, trace,
                               "{\"beta\":456}");
  test_agent_attributes_as_json("agent attributes: trace", attributes, trace,
                                "{\"omega\":456}");

  test_user_attributes_as_json("user attributes: error", attributes, error,
                               "{\"gamma\":\"789\"}");
  test_agent_attributes_as_json("agent attributes: error", attributes, error,
                                "{\"theta\":\"789\"}");

  test_user_attributes_as_json("user attributes: browser", attributes, browser,
                               "{}");
  test_agent_attributes_as_json(
      "agent attributes: browser", attributes, browser,
      "{\"theta\":\"789\",\"omega\":456,\"psi\":123}");

  nr_attributes_destroy(&attributes);
  nr_attribute_config_destroy(&config);
}

/*
 * nr_txn_attributes_set_long_attribute and
nr_txn_attributes_set_string_attribute are wrappers for
nr_attributes_agent_add_long nr_attributes_agent_add_string which already have
unit tests which do data validation. This test case will just be verify the
checks the wrappers employ.
 */
static void test_nr_txn_attributes_set_attribute(void) {
  nr_attributes_t* attributes;
  nr_attribute_config_t* config;
  //  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  // uint32_t trace = NR_ATTRIBUTE_DESTINATION_TXN_TRACE;
  // uint32_t error = NR_ATTRIBUTE_DESTINATION_ERROR;
  // uint32_t browser = NR_ATTRIBUTE_DESTINATION_BROWSER;
  uint32_t all = NR_ATTRIBUTE_DESTINATION_ALL;
  nrobj_t* obj = NULL;

  config = nr_attribute_config_create();
  attributes = nr_attributes_create(config);

  /*
   * Invalid values are attribute=NULL, value = NULL, value = empty string.
   */
  nr_txn_attributes_set_string_attribute(attributes, NULL, "value");
  obj = nr_attributes_agent_to_obj(attributes, all);
  tlib_pass_if_null("Shouldn't have any attributes", obj);
  nro_delete(obj);
  nr_txn_attributes_set_string_attribute(attributes, nr_txn_clm_code_function,
                                         NULL);
  obj = nr_attributes_agent_to_obj(attributes, all);
  tlib_pass_if_null("Shouldn't have any attributes", obj);
  nro_delete(obj);
  nr_txn_attributes_set_string_attribute(attributes, nr_txn_clm_code_function,
                                         "");
  obj = nr_attributes_agent_to_obj(attributes, all);
  tlib_pass_if_null("Shouldn't have any attributes", obj);
  nro_delete(obj);

  /*
   * Invalid values are attribute=NULL.
   */
  nr_txn_attributes_set_long_attribute(attributes, NULL, 1);
  obj = nr_attributes_agent_to_obj(attributes, all);
  tlib_pass_if_null("Shouldn't have any attributes", obj);
  nro_delete(obj);

  /*
   * Attributes added for valid value.
   */
  nr_txn_attributes_set_string_attribute(attributes, nr_txn_clm_code_function,
                                         "value");
  test_agent_attributes_as_json("attributes added", attributes, all,
                                "{\"code.function\":\"value\"}");

  /*
   * Attributes added for valid value.
   */
  nr_txn_attributes_set_long_attribute(attributes, nr_txn_clm_code_lineno, 123);
  test_agent_attributes_as_json(
      "attributes added", attributes, all,
      "{\"code.lineno\":123,\"code.function\":\"value\"}");

  nr_attributes_destroy(&attributes);
  nr_attribute_config_destroy(&config);
}

static void test_attributes_to_obj_bad_params(void) {
  /* Don't blow up! */
  nr_attributes_user_to_obj(0, NR_ATTRIBUTE_DESTINATION_BROWSER);
  nr_attributes_agent_to_obj(0, NR_ATTRIBUTE_DESTINATION_BROWSER);
}

static void test_attribute_string_length_limits(void) {
  nr_attributes_t* attributes;
  nr_attribute_config_t* config;
  uint32_t all = NR_ATTRIBUTE_DESTINATION_ALL;
  nr_status_t st;

  config = nr_attribute_config_create();
  attributes = nr_attributes_create(config);

  tlib_pass_if_true("tests valid", NR_ATTRIBUTE_KEY_LENGTH_LIMIT == 255,
                    "NR_ATTRIBUTE_KEY_LENGTH_LIMIT=%d",
                    NR_ATTRIBUTE_KEY_LENGTH_LIMIT);
  tlib_pass_if_true("tests valid", NR_ATTRIBUTE_VALUE_LENGTH_LIMIT == 255,
                    "NR_ATTRIBUTE_VALUE_LENGTH_LIMIT=%d",
                    NR_ATTRIBUTE_VALUE_LENGTH_LIMIT);

  st = nr_attributes_user_add_string(attributes, all,
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaa", /* 320 chars */
                                     "alpha");
  tlib_pass_if_true("key exceeds limit", NR_FAILURE == st, "st=%d", (int)st);
  test_user_attributes_as_json("key exceeds limit", attributes, all, "null");

  st = nr_attributes_user_add_string(attributes, all, "alpha",
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                     "aaaaaaaaaaaaaaaaaaaaaaa"); /* 320 chars */
  tlib_pass_if_true("value gets truncated", NR_SUCCESS == st, "st=%d", (int)st);
  test_user_attributes_as_json(
      "value gets truncated", attributes, all,
      "{"
      "\""
      "alpha"
      "\":\""
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "\""
      "}");

  nr_attributes_destroy(&attributes);
  nr_attribute_config_destroy(&config);
}

static void test_user_attribute_limit(void) {
  int i;
  char buf[128];
  nr_attributes_t* attributes;
  nr_attribute_config_t* config;
  uint32_t all = NR_ATTRIBUTE_DESTINATION_ALL;
  nr_status_t st;

  config = nr_attribute_config_create();
  attributes = nr_attributes_create(config);

  for (i = 0; i < NR_ATTRIBUTE_USER_LIMIT; i++) {
    buf[0] = 0;
    snprintf(buf, sizeof(buf), "%d", i);
    st = nr_attributes_user_add_string(attributes, all, buf, buf);
    tlib_pass_if_true("add success", NR_SUCCESS == st, "st=%d", (int)st);
  }

  st = nr_attributes_user_add_string(attributes, all, "cant_add_me",
                                     "cant_add_me");
  tlib_pass_if_true("user attribute limit upheld", NR_FAILURE == st, "st=%d",
                    (int)st);

  test_user_attributes_as_json("user attribute limit upheld", attributes, all,
                               "{\"63\":\"63\",\"62\":\"62\",\"61\":\"61\","
                               "\"60\":\"60\",\"59\":\"59\",\"58\":\"58\","
                               "\"57\":\"57\",\"56\":\"56\",\"55\":\"55\","
                               "\"54\":\"54\",\"53\":\"53\",\"52\":\"52\","
                               "\"51\":\"51\",\"50\":\"50\",\"49\":\"49\","
                               "\"48\":\"48\",\"47\":\"47\",\"46\":\"46\","
                               "\"45\":\"45\",\"44\":\"44\",\"43\":\"43\","
                               "\"42\":\"42\",\"41\":\"41\",\"40\":\"40\","
                               "\"39\":\"39\",\"38\":\"38\",\"37\":\"37\","
                               "\"36\":\"36\",\"35\":\"35\",\"34\":\"34\","
                               "\"33\":\"33\",\"32\":\"32\",\"31\":\"31\","
                               "\"30\":\"30\",\"29\":\"29\",\"28\":\"28\","
                               "\"27\":\"27\",\"26\":\"26\",\"25\":\"25\","
                               "\"24\":\"24\",\"23\":\"23\",\"22\":\"22\","
                               "\"21\":\"21\",\"20\":\"20\",\"19\":\"19\","
                               "\"18\":\"18\",\"17\":\"17\",\"16\":\"16\","
                               "\"15\":\"15\",\"14\":\"14\",\"13\":\"13\","
                               "\"12\":\"12\",\"11\":\"11\",\"10\":\"10\","
                               "\"9\":\"9\",\"8\":\"8\",\"7\":\"7\",\"6\":"
                               "\"6\",\"5\":\"5\",\"4\":\"4\",\"3\":\"3\","
                               "\"2\":\"2\",\"1\":\"1\",\"0\":\"0\"}");

  st = nr_attributes_user_add_string(attributes, all, "0", "BEEN_REPLACED");
  tlib_pass_if_true("replacement still works when limit reached",
                    NR_SUCCESS == st, "st=%d", (int)st);

  test_user_attributes_as_json("replacement still works when limit reached",
                               attributes, all,
                               "{\"0\":\"BEEN_REPLACED\",\"63\":\"63\",\"62\":"
                               "\"62\",\"61\":\"61\",\"60\":\"60\","
                               "\"59\":\"59\",\"58\":\"58\","
                               "\"57\":\"57\",\"56\":\"56\",\"55\":\"55\","
                               "\"54\":\"54\",\"53\":\"53\",\"52\":\"52\","
                               "\"51\":\"51\",\"50\":\"50\",\"49\":\"49\","
                               "\"48\":\"48\",\"47\":\"47\",\"46\":\"46\","
                               "\"45\":\"45\",\"44\":\"44\",\"43\":\"43\","
                               "\"42\":\"42\",\"41\":\"41\",\"40\":\"40\","
                               "\"39\":\"39\",\"38\":\"38\",\"37\":\"37\","
                               "\"36\":\"36\",\"35\":\"35\",\"34\":\"34\","
                               "\"33\":\"33\",\"32\":\"32\",\"31\":\"31\","
                               "\"30\":\"30\",\"29\":\"29\",\"28\":\"28\","
                               "\"27\":\"27\",\"26\":\"26\",\"25\":\"25\","
                               "\"24\":\"24\",\"23\":\"23\",\"22\":\"22\","
                               "\"21\":\"21\",\"20\":\"20\",\"19\":\"19\","
                               "\"18\":\"18\",\"17\":\"17\",\"16\":\"16\","
                               "\"15\":\"15\",\"14\":\"14\",\"13\":\"13\","
                               "\"12\":\"12\",\"11\":\"11\",\"10\":\"10\","
                               "\"9\":\"9\",\"8\":\"8\",\"7\":\"7\",\"6\":"
                               "\"6\",\"5\":\"5\",\"4\":\"4\",\"3\":\"3\","
                               "\"2\":\"2\",\"1\":\"1\"}");

  nr_attributes_destroy(&attributes);
  nr_attribute_config_destroy(&config);
}

static nr_attribute_config_t* cross_agent_attribute_config_from_obj(
    const nrobj_t* obj) {
  int i;
  nr_attribute_config_t* attribute_config;
  struct {
    const char* name;
    int dflt;
    uint32_t destinations;
  } enabled_settings[] = {
      {"attributes.enabled", 1, NR_ATTRIBUTE_DESTINATION_ALL},
      {"transaction_events.attributes.enabled", 1,
       NR_ATTRIBUTE_DESTINATION_TXN_EVENT},
      {"transaction_tracer.attributes.enabled", 1,
       NR_ATTRIBUTE_DESTINATION_TXN_TRACE},
      {"error_collector.attributes.enabled", 1, NR_ATTRIBUTE_DESTINATION_ERROR},
      {"browser_monitoring.attributes.enabled", 0,
       NR_ATTRIBUTE_DESTINATION_BROWSER},
      {0, 0, 0}};
  struct {
    const char* name;
    uint32_t exclude_destinations;
    uint32_t include_destinations;
  } include_exclude_settings[] = {
      {"attributes.exclude", NR_ATTRIBUTE_DESTINATION_ALL, 0},
      {"transaction_events.attributes.exclude",
       NR_ATTRIBUTE_DESTINATION_TXN_EVENT, 0},
      {"transaction_tracer.attributes.exclude",
       NR_ATTRIBUTE_DESTINATION_TXN_TRACE, 0},
      {"error_collector.attributes.exclude", NR_ATTRIBUTE_DESTINATION_ERROR, 0},
      {"browser_monitoring.attributes.exclude",
       NR_ATTRIBUTE_DESTINATION_BROWSER, 0},

      {"attributes.include", 0, NR_ATTRIBUTE_DESTINATION_ALL},
      {"transaction_events.attributes.include", 0,
       NR_ATTRIBUTE_DESTINATION_TXN_EVENT},
      {"transaction_tracer.attributes.include", 0,
       NR_ATTRIBUTE_DESTINATION_TXN_TRACE},
      {"error_collector.attributes.include", 0, NR_ATTRIBUTE_DESTINATION_ERROR},
      {"browser_monitoring.attributes.include", 0,
       NR_ATTRIBUTE_DESTINATION_BROWSER},
      {0, 0, 0}};

  attribute_config = nr_attribute_config_create();

  /*
   * Enabled / Disabled Settings
   */
  for (i = 0; enabled_settings[i].name; i++) {
    int enabled = nr_reply_get_bool(obj, enabled_settings[i].name,
                                    enabled_settings[i].dflt);

    if (0 == enabled) {
      nr_attribute_config_disable_destinations(
          attribute_config, enabled_settings[i].destinations);
    }
  }

  /*
   * Include / Exclude Settings
   */
  for (i = 0; include_exclude_settings[i].name; i++) {
    int j;
    const nrobj_t* arr
        = nro_get_hash_array(obj, include_exclude_settings[i].name, 0);
    int arr_size = nro_getsize(arr);

    if (0 == arr) {
      continue;
    }

    tlib_pass_if_true("tests valid", 0 != arr_size, "arr_size=%d", arr_size);

    for (j = 0; j < arr_size; j++) {
      const char* entry = nro_get_string(nro_get_array_value(arr, j + 1, 0), 0);

      if (entry) {
        nr_attribute_config_modify_destinations(
            attribute_config, entry,
            include_exclude_settings[i].include_destinations,
            include_exclude_settings[i].exclude_destinations);
      }
    }
  }

  return attribute_config;
}

static uint32_t cross_agent_destinations_from_array(const nrobj_t* arr) {
  uint32_t destinations = 0;
  int arr_size = nro_getsize(arr);
  int i;
  int j;
  struct {
    const char* name;
    uint32_t destination_flag;
  } destinations_from_string[]
      = {{"transaction_events", NR_ATTRIBUTE_DESTINATION_TXN_EVENT},
         {"transaction_tracer", NR_ATTRIBUTE_DESTINATION_TXN_TRACE},
         {"error_collector", NR_ATTRIBUTE_DESTINATION_ERROR},
         {"browser_monitoring", NR_ATTRIBUTE_DESTINATION_BROWSER},
         {0, 0}};

  tlib_pass_if_true("tests valid", 0 != arr, "arr=%p", arr);

  for (i = 0; i < arr_size; i++) {
    const char* name = nro_get_string(nro_get_array_value(arr, i + 1, 0), 0);

    for (j = 0; destinations_from_string[j].name; j++) {
      if (0 == nr_strcmp(name, destinations_from_string[j].name)) {
        destinations |= destinations_from_string[j].destination_flag;
        break;
      }
    }
  }

  return destinations;
}

static uint32_t cross_agent_tests_get_actual_destinations(
    const nr_attributes_t* attributes,
    const char* input_key) {
  uint32_t actual_destinations = 0;
  int i;
  uint32_t dests[]
      = {NR_ATTRIBUTE_DESTINATION_TXN_EVENT, NR_ATTRIBUTE_DESTINATION_TXN_TRACE,
         NR_ATTRIBUTE_DESTINATION_ERROR, NR_ATTRIBUTE_DESTINATION_BROWSER, 0};

  for (i = 0; dests[i]; i++) {
    nrobj_t* obj = nr_attributes_agent_to_obj(attributes, dests[i]);

    if (nro_get_hash_value(obj, input_key, 0)) {
      actual_destinations |= dests[i];
    }
    nro_delete(obj);
  }

  return actual_destinations;
}

static void test_cross_agent_attribute_configuration(void) {
  char* json = 0;
  nrobj_t* array = 0;
  nrotype_t otype;
  int i;

#define TEST_ATTRIBUTES_CONFIGURATION_TEST_FILE \
  CROSS_AGENT_TESTS_DIR "/attribute_configuration.json"
  json = nr_read_file_contents(TEST_ATTRIBUTES_CONFIGURATION_TEST_FILE,
                               10 * 1000 * 1000);
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
      const nrobj_t* config = nro_get_hash_hash(hash, "config", 0);
      const char* input_key = nro_get_hash_string(hash, "input_key", 0);
      nr_attribute_config_t* attribute_config;
      nr_attributes_t* attributes;
      uint32_t input_default_destinations;
      uint32_t expected_destinations;
      uint32_t actual_destinations;

      tlib_pass_if_true("tests valid", 0 != hash, "hash=%p", hash);
      tlib_pass_if_true("tests valid", 0 != config, "config=%p", config);
      tlib_pass_if_true("tests valid", 0 != testname, "testname=%p", testname);
      tlib_pass_if_true("tests valid", 0 != input_key, "input_key=%p",
                        input_key);

      attribute_config = cross_agent_attribute_config_from_obj(config);
      expected_destinations = cross_agent_destinations_from_array(
          nro_get_hash_array(hash, "expected_destinations", 0));
      input_default_destinations = cross_agent_destinations_from_array(
          nro_get_hash_array(hash, "input_default_destinations", 0));

      attributes = nr_attributes_create(attribute_config);

      nr_attributes_agent_add_long(attributes, input_default_destinations,
                                   input_key, 12345);
      actual_destinations
          = cross_agent_tests_get_actual_destinations(attributes, input_key);

      tlib_pass_if_true(testname ? testname : "unknown",
                        expected_destinations == actual_destinations,
                        "expected_destinations=%u actual_destinations=%u",
                        expected_destinations, actual_destinations);

      nr_attribute_config_destroy(&attribute_config);
      nr_attributes_destroy(&attributes);
    }
  }

  nro_delete(array);
  nr_free(json);
}

static void test_double_nan(void) {
  nr_status_t rv;
  nrobj_t* obj = nro_new_double(NAN);
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  nr_attributes_t* atts = nr_attributes_create(NULL);

  rv = nr_attributes_user_add(atts, event, "my_key", obj);
  tlib_pass_if_status_failure("double nan", rv);

  nr_attributes_destroy(&atts);
  nro_delete(obj);
}

static void test_double_inf(void) {
  nr_status_t rv;
  nrobj_t* obj = nro_new_double(INFINITY);
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  nr_attributes_t* atts = nr_attributes_create(NULL);

  rv = nr_attributes_user_add(atts, event, "my_key", obj);
  tlib_pass_if_status_failure("double inf", rv);

  nr_attributes_destroy(&atts);
  nro_delete(obj);
}

static void test_empty_string(void) {
  nr_status_t rv;
  nrobj_t* obj = nro_new_string("");
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  nr_attributes_t* atts = nr_attributes_create(NULL);

  rv = nr_attributes_user_add(atts, event, "my_key", obj);
  tlib_pass_if_status_success("empty string", rv);
  nro_delete(obj);

  obj = nr_attributes_user_to_obj(atts, event);
  test_obj_as_json("empty string", obj, "{\"my_key\":\"\"}");
  nro_delete(obj);

  nr_attributes_destroy(&atts);
}

static void test_invalid_object(void) {
  nr_status_t rv;
  nrobj_t* obj = nro_new_array();
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  nr_attributes_t* atts = nr_attributes_create(NULL);

  rv = nr_attributes_user_add(atts, event, "my_key", obj);
  tlib_pass_if_status_failure("invalid value type", rv);

  nr_attributes_destroy(&atts);
  nro_delete(obj);
}

static void test_null_and_bools_and_double(void) {
  nrobj_t* true_obj = nro_new_boolean(1);
  nrobj_t* false_obj = nro_new_boolean(0);
  nrobj_t* null_obj = nro_new_none();
  nrobj_t* double_obj = nro_new_double(4.56);
  nr_status_t rv;
  uint32_t event = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;
  nr_attributes_t* atts = nr_attributes_create(NULL);
  nrobj_t* obj;

  rv = nr_attributes_user_add(atts, event, "true", true_obj);
  tlib_pass_if_status_success("added true", rv);
  rv = nr_attributes_user_add(atts, event, "false", false_obj);
  tlib_pass_if_status_success("added false", rv);
  rv = nr_attributes_user_add(atts, event, "null", null_obj);
  tlib_pass_if_status_success("added null", rv);
  rv = nr_attributes_user_add(atts, event, "double", double_obj);
  tlib_pass_if_status_success("added double", rv);

  obj = nr_attributes_user_to_obj(atts, event);
  test_obj_as_json(
      "true, false, null", obj,
      "{\"double\":4.56000,\"null\":null,\"false\":false,\"true\":true}");

  nro_delete(obj);
  nr_attributes_destroy(&atts);
  nro_delete(double_obj);
  nro_delete(true_obj);
  nro_delete(false_obj);
  nro_delete(null_obj);
}

static void test_user_exists(void) {
  nr_attributes_t* atts = nr_attributes_create(NULL);

  tlib_pass_if_bool_equal("no int attribute", false,
                          nr_attributes_user_exists(atts, "int"));

  nr_attributes_user_add_long(atts, NR_ATTRIBUTE_DESTINATION_TXN_EVENT, "int",
                              3);
  tlib_pass_if_bool_equal("no int attribute", true,
                          nr_attributes_user_exists(atts, "int"));

  nr_attributes_destroy(&atts);
}

static void test_remove_attribute(void) {
  nr_attributes_t* atts = nr_attributes_create(NULL);

  nr_attributes_user_add_long(atts, NR_ATTRIBUTE_DESTINATION_TXN_EVENT, "int",
                              3);
  tlib_pass_if_bool_equal("int attribute exists before remove", true,
                          nr_attributes_user_exists(atts, "int"));

  nr_attributes_remove_attribute(NULL, "int", 1);
  tlib_pass_if_bool_equal("int attribute exists after NULL attributes", true,
                          nr_attributes_user_exists(atts, "int"));

  nr_attributes_remove_attribute(atts, NULL, 1);

  tlib_pass_if_bool_equal("int attribute exists after NULL key", true,
                          nr_attributes_user_exists(atts, "int"));

  nr_attributes_remove_attribute(atts, "int", 1);

  tlib_pass_if_bool_equal("no int attribute exists after remove", false,
                          nr_attributes_user_exists(atts, "int"));

  nr_attributes_destroy(&atts);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_destination_modifier_match();
  test_destination_modifier_apply();
  test_modifier_destroy_bad_params();
  test_disable_destinations();
  test_destination_modifier_create();
  test_config_modify_destinations();
  test_config_copy();
  test_config_apply();
  test_config_destroy_bad_params();
  test_attribute_destroy_bad_params();
  test_attributes_destroy_bad_params();
  test_remove_duplicate();
  test_add();
  test_attribute_string_length_limits();
  test_user_attribute_limit();
  test_attributes_to_obj_bad_params();
  test_double_nan();
  test_double_inf();
  test_empty_string();
  test_invalid_object();
  test_null_and_bools_and_double();
  test_user_exists();
  test_remove_attribute();
  test_nr_txn_attributes_set_attribute();

  test_cross_agent_attribute_configuration();
}
