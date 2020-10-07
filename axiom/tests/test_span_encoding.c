/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_span_encoding.h"
#include "nr_span_encoding_private.h"
#include "nr_span_event.h"
#include "nr_span_event_private.h"
#include "v1.pb-c.h"

#include "tlib_main.h"

static void add_values(nrobj_t* hash) {
  nro_set_hash_boolean(hash, "bool", true);
  nro_set_hash_double(hash, "double", 1.0);
  nro_set_hash_long(hash, "long", 12345);
  nro_set_hash_string(hash, "string", "foo");
}

#define check_values(ARRAY, NUM)                                               \
  do {                                                                         \
    size_t _check_i;                                                           \
    const size_t _check_num = (NUM);                                           \
    struct {                                                                   \
      size_t bools;                                                            \
      size_t doubles;                                                          \
      size_t longs;                                                            \
      size_t strings;                                                          \
    } _check_seen = {0, 0, 0, 0};                                              \
                                                                               \
    for (_check_i = 0; _check_i < _check_num; _check_i++) {                    \
      const char* _check_key = ARRAY[_check_i]->key;                           \
      const Com__Newrelic__Trace__V1__AttributeValue* _check_value             \
          = ARRAY[_check_i]->value;                                            \
                                                                               \
      if (nr_streq(_check_key, "bool")) {                                      \
        tlib_pass_if_int_equal(                                                \
            "bool value has the right type",                                   \
            (int)COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_BOOL_VALUE,  \
            (int)_check_value->value_case);                                    \
        tlib_pass_if_bool_equal("bool value has the right value", true,        \
                                (bool)_check_value->bool_value);               \
        _check_seen.bools++;                                                   \
      }                                                                        \
                                                                               \
      if (nr_streq(_check_key, "double")) {                                    \
        tlib_pass_if_int_equal(                                                \
            "double value has the right type",                                 \
            (int)                                                              \
                COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_DOUBLE_VALUE, \
            (int)_check_value->value_case);                                    \
        tlib_pass_if_double_equal("double value has the right value", 1.0,     \
                                  _check_value->double_value);                 \
        _check_seen.doubles++;                                                 \
      }                                                                        \
                                                                               \
      if (nr_streq(_check_key, "long")) {                                      \
        tlib_pass_if_int_equal(                                                \
            "long value has the right type",                                   \
            (int)COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_INT_VALUE,   \
            (int)_check_value->value_case);                                    \
        tlib_pass_if_int64_t_equal("long value has the right value", 12345,    \
                                   _check_value->int_value);                   \
        _check_seen.longs++;                                                   \
      }                                                                        \
                                                                               \
      if (nr_streq(_check_key, "string")) {                                    \
        tlib_pass_if_int_equal(                                                \
            "string value has the right type",                                 \
            (int)                                                              \
                COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_STRING_VALUE, \
            (int)_check_value->value_case);                                    \
        tlib_pass_if_str_equal("string value has the right value", "foo",      \
                               _check_value->string_value);                    \
        _check_seen.strings++;                                                 \
      }                                                                        \
    }                                                                          \
                                                                               \
    tlib_pass_if_size_t_equal("one bool was seen", 1, _check_seen.bools);      \
    tlib_pass_if_size_t_equal("one double was seen", 1, _check_seen.doubles);  \
    tlib_pass_if_size_t_equal("one long was seen", 1, _check_seen.longs);      \
    tlib_pass_if_size_t_equal("one string was seen", 1, _check_seen.strings);  \
  } while (0)

static void test_single(void) {
  Com__Newrelic__Trace__V1__Span* encoded;
  nr_span_encoding_result_t result = NR_SPAN_ENCODING_RESULT_INIT;
  nr_span_event_t* span = nr_span_event_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("NULL span", false,
                          nr_span_encoding_single_v1(NULL, &result));
  tlib_pass_if_bool_equal("NULL result", false,
                          nr_span_encoding_single_v1(span, NULL));

  /*
   * Test : Normal operation.
   *
   * It's unclear to me how stable the protobuf encoding is for a given
   * object, so for now we'll just poke around at the generated objects.
   */
  nr_span_event_set_trace_id(span, "abcdefgh");
  tlib_pass_if_bool_equal("empty span", true,
                          nr_span_encoding_single_v1(span, &result));
  tlib_pass_if_not_null("span data", result.data);
  tlib_fail_if_size_t_equal("span size", 0, result.len);
  tlib_pass_if_size_t_equal("span count", 1, result.span_count);
  encoded
      = com__newrelic__trace__v1__span__unpack(NULL, result.len, result.data);
  tlib_pass_if_not_null("span can be unpacked", encoded);
  tlib_pass_if_str_equal("span has the correct trace ID", "abcdefgh",
                         encoded->trace_id);
  com__newrelic__trace__v1__span__free_unpacked(encoded, NULL);
  nr_span_encoding_result_deinit(&result);

  // Now we'll put one of every attribute value type into each of the objects.
  add_values(span->agent_attributes);
  add_values(span->intrinsics);
  add_values(span->user_attributes);

  tlib_pass_if_bool_equal("full span", true,
                          nr_span_encoding_single_v1(span, &result));
  tlib_pass_if_not_null("span data", result.data);
  tlib_fail_if_size_t_equal("span size", 0, result.len);
  tlib_pass_if_size_t_equal("span count", 1, result.span_count);
  encoded
      = com__newrelic__trace__v1__span__unpack(NULL, result.len, result.data);
  tlib_pass_if_not_null("span can be unpacked", encoded);
  tlib_pass_if_str_equal("span has the correct trace ID", "abcdefgh",
                         encoded->trace_id);
  check_values(encoded->agent_attributes, encoded->n_agent_attributes);
  check_values(encoded->intrinsics, encoded->n_intrinsics);
  check_values(encoded->user_attributes, encoded->n_user_attributes);
  com__newrelic__trace__v1__span__free_unpacked(encoded, NULL);
  nr_span_encoding_result_deinit(&result);

  nr_span_event_destroy(&span);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
static void test_batch(void) {
  Com__Newrelic__Trace__V1__SpanBatch* encoded;
  nr_span_encoding_result_t result = NR_SPAN_ENCODING_RESULT_INIT;
  nr_span_event_t* spans[2] = {nr_span_event_create(), nr_span_event_create()};

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("NULL spans", false,
                          nr_span_encoding_batch_v1(NULL, 2, &result));
  tlib_pass_if_bool_equal(
      "NULL result", false,
      nr_span_encoding_batch_v1((const nr_span_event_t**)spans, 2, NULL));

  /*
   * Test : Empty batch.
   */
  tlib_pass_if_bool_equal(
      "empty batch", true,
      nr_span_encoding_batch_v1((const nr_span_event_t**)spans, 0, &result));
  tlib_pass_if_size_t_equal("empty batch size", 0, result.len);
  nr_span_encoding_result_deinit(&result);

  /*
   * Test : Normal operation.
   */
  nr_span_event_set_trace_id(spans[0], "abcdefgh");
  nr_span_event_set_trace_id(spans[1], "01234567");
  add_values(spans[1]->agent_attributes);
  add_values(spans[1]->intrinsics);
  add_values(spans[1]->user_attributes);

  tlib_pass_if_bool_equal(
      "normal batch", true,
      nr_span_encoding_batch_v1((const nr_span_event_t**)spans, 2, &result));
  tlib_fail_if_size_t_equal("normal batch size", 0, result.len);
  tlib_pass_if_not_null("normal batch data", result.data);
  tlib_pass_if_size_t_equal("span count", 2, result.span_count);
  encoded = com__newrelic__trace__v1__span_batch__unpack(NULL, result.len,
                                                         result.data);
  tlib_pass_if_not_null("batch can be unpacked", encoded);
  tlib_pass_if_size_t_equal("spans contained in the batch", 2,
                            encoded->n_spans);

  tlib_pass_if_str_equal("span 0 trace ID", "abcdefgh",
                         encoded->spans[0]->trace_id);

  tlib_pass_if_str_equal("span 1 trace ID", "01234567",
                         encoded->spans[1]->trace_id);
  check_values(encoded->spans[1]->agent_attributes,
               encoded->spans[1]->n_agent_attributes);
  check_values(encoded->spans[1]->intrinsics, encoded->spans[1]->n_intrinsics);
  check_values(encoded->spans[1]->user_attributes,
               encoded->spans[1]->n_user_attributes);

  com__newrelic__trace__v1__span_batch__free_unpacked(encoded, NULL);
  nr_span_encoding_result_deinit(&result);

  nr_span_event_destroy(&spans[0]);
  nr_span_event_destroy(&spans[1]);
}
#pragma GCC diagnostic pop

static void test_result_deinit(void) {
  nr_span_encoding_result_t result = NR_SPAN_ENCODING_RESULT_INIT;

  /*
   * Test : Bad parameters.
   */
  nr_span_encoding_result_deinit(NULL);

  /*
   * Test : Initialised, but unused result.
   */
  nr_span_encoding_result_deinit(&result);

  /*
   * Test : Used result.
   */
  result.data = nr_malloc(4);
  result.len = 4;
  result.span_count = 1;
  nr_span_encoding_result_deinit(&result);
  tlib_pass_if_null("data pointer", result.data);
}

#define test_encoded_attribute_value(M, OBJ, EXPECTED_TYPE, VALUE_MACRO, \
                                     EXPECTED_VALUE, FIELD)              \
  do {                                                                   \
    nrobj_t* _av_in = (OBJ);                                             \
    Com__Newrelic__Trace__V1__AttributeValue _av_out;                    \
                                                                         \
    tlib_pass_if_bool_equal(                                             \
        M " encoding", true,                                             \
        nr_span_encoding_encode_attribute_value_v1(_av_in, &_av_out));   \
    tlib_pass_if_int_equal(M " type", (int)(EXPECTED_TYPE),              \
                           (int)_av_out.value_case);                     \
    VALUE_MACRO(M " value", (EXPECTED_VALUE), _av_out.FIELD);            \
                                                                         \
    nro_delete(_av_in);                                                  \
  } while (0)

static void test_encode_attribute_value(void) {
  size_t i;
  const nrotype_t unhandled_types[] = {
      NR_OBJECT_ARRAY,   NR_OBJECT_HASH, NR_OBJECT_INVALID,
      NR_OBJECT_JSTRING, NR_OBJECT_NONE,
  };

  /*
   * Test : Unhandled types.
   */
  for (i = 0; i < sizeof(unhandled_types) / sizeof(unhandled_types[0]); i++) {
    nrobj_t* in = nro_new(unhandled_types[i]);
    char* message = nr_formatf("unhandled type %d", (int)unhandled_types[i]);
    Com__Newrelic__Trace__V1__AttributeValue out;

    tlib_pass_if_bool_equal(
        message, false, nr_span_encoding_encode_attribute_value_v1(in, &out));
    tlib_pass_if_int_equal(
        message, (int)COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE__NOT_SET,
        (int)out.value_case);

    nro_delete(in);
    nr_free(message);
  }

  /*
   * Test : Handled types.
   */
  test_encoded_attribute_value(
      "bool", nro_new_boolean(true),
      COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_BOOL_VALUE,
      tlib_pass_if_bool_equal, true, bool_value);

  test_encoded_attribute_value(
      "double", nro_new_double(1.0),
      COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_DOUBLE_VALUE,
      tlib_pass_if_double_equal, 1.0, double_value);

  test_encoded_attribute_value(
      "int", nro_new_int(42),
      COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_INT_VALUE,
      tlib_pass_if_int64_t_equal, 42, int_value);

  test_encoded_attribute_value(
      "long", nro_new_long(42),
      COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_INT_VALUE,
      tlib_pass_if_int64_t_equal, 42, int_value);

  test_encoded_attribute_value(
      "ulong", nro_new_ulong(42),
      COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_INT_VALUE,
      tlib_pass_if_int64_t_equal, 42, int_value);

  test_encoded_attribute_value(
      "string", nro_new_string("foo"),
      COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_STRING_VALUE,
      tlib_pass_if_str_equal, "foo", string_value);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 8, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_single();
  test_batch();
  test_result_deinit();
  test_encode_attribute_value();
}