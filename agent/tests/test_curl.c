/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_curl.h"
#include "php_curl_md.h"
#include "php_hash.h"
#include "nr_header.h"
#include "nr_span_encoding.h"
#include "nr_span_queue.h"
#include "nr_txn.h"
#include "util_minmax_heap.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_curl_get_url(TSRMLS_D) {
  tlib_php_request_start();

  char* url;
  zval* zurl = nr_php_zval_alloc();
  nr_php_zval_str(zurl, "https://newrelic.com");

  zval* ch = nr_php_call(NULL, "curl_init", zurl);
  tlib_pass_if_str_equal("curl url get when initialization includes URL",
                         "https://newrelic.com",
                         url = nr_php_curl_get_url(ch TSRMLS_CC));
  nr_php_zval_free(&ch);

  ch = nr_php_call(NULL, "curl_init");
  tlib_pass_if_str_equal("curl url get when initialization doesn't include URL",
                         0, nr_php_curl_get_url(ch TSRMLS_CC));
  nr_php_zval_free(&ch);

  nr_free(url);
  nr_php_zval_free(&zurl);
  tlib_php_request_end();
}

static void test_curl_should_instrument_proto() {
  tlib_pass_if_true(
      "nr_php_curl_should_instrument_proto returns true for various non-local "
      "resources",
      nr_php_curl_should_instrument_proto("http://newrelic.com")
          && nr_php_curl_should_instrument_proto("https://newrelic.com")
          && nr_php_curl_should_instrument_proto("newrelic.com"),
      "expected true");

  tlib_pass_if_false(
      "nr_php_curl_should_instrument_proto returns false for file:// urls",
      nr_php_curl_should_instrument_proto("file://newrelic.com"),
      "expected false");
}

static void test_curl_exec(TSRMLS_D) {
  zval* ch;
  zval* zurl;
  const char* url = "https://newrelic.com";
  nr_segment_t* segment;

  tlib_php_request_start();

  zurl = nr_php_zval_alloc();
  nr_php_zval_str(zurl, url);

  ch = nr_php_call(NULL, "curl_init", zurl);

  tlib_pass_if_null("no segment before curl_exec_pre",
                    nr_php_curl_md_get_segment(ch));

  /*
   * Calling nr_php_curl_exec_pre should assign a segment to the curl
   * metadata and set the start time of the segment.
   */
  nr_php_curl_exec_pre(ch, NULL, NULL TSRMLS_CC);

  segment = nr_php_curl_md_get_segment(ch TSRMLS_CC);
  tlib_pass_if_not_null("segment initialized", segment);
  tlib_pass_if_true("segment start time set", segment->start_time > 0,
                    "start_time=" NR_TIME_FMT, segment->start_time);
  tlib_pass_if_time_equal("segment stop time not set", segment->stop_time, 0);

  /*
   * Calling nr_php_curl_exec_post should end the segment as an external
   * segment.
   */
  nr_php_curl_exec_post(ch, false TSRMLS_CC);

  tlib_pass_if_null("the metadata does not keep the ended segment",
                    nr_php_curl_md_get_segment(ch));
  tlib_pass_if_not_null("segment initialized", segment);
  tlib_pass_if_true("segment stop time set",
                    segment->stop_time > segment->start_time,
                    "start_time=" NR_TIME_FMT " stop_time=" NR_TIME_FMT,
                    segment->start_time, segment->stop_time);
  tlib_pass_if_true("segment type is external",
                    segment->type == NR_SEGMENT_EXTERNAL, "segment type is %d",
                    segment->type);
  tlib_pass_if_not_null("typed attributes are initialized",
                        segment->typed_attributes);
  tlib_pass_if_str_equal("segment url is set",
                         segment->typed_attributes->external.uri, url);

  nr_php_zval_free(&ch);
  nr_php_zval_free(&zurl);
  tlib_php_request_end();
}

static void test_curl_multi_exec_reused_handle(void) {
  zval* mh;
  nr_segment_t* segment;
  nr_segment_t* current;
  size_t ended_before;
  int burst;

  tlib_php_request_start();

  mh = nr_php_call(NULL, "curl_multi_init");
  ended_before = NRPRG(txn)->segment_count;

  /*
   * A multi handle that runs several bursts of transfers has one multi
   * segment, which stays open between the bursts.
   */
  nr_php_curl_multi_exec_pre(mh);
  segment = nr_php_curl_multi_md_get_segment(mh);
  tlib_pass_if_not_null("the first burst creates the multi segment", segment);

  for (burst = 1; burst <= 5; burst++) {
    if (burst > 1) {
      nr_php_curl_multi_exec_pre(mh);
    }
    nr_php_curl_multi_exec_post(mh);
    nr_php_curl_multi_exec_finalize(mh);

    current = nr_php_curl_multi_md_get_segment(mh);
    tlib_pass_if_ptr_equal("every burst uses the same multi segment", segment,
                           current);
  }

  tlib_pass_if_size_t_equal("no burst ended the multi segment", ended_before,
                            NRPRG(txn)->segment_count);

  /*
   * The cleanup at the end of the transaction ends the segment once.
   */
  nr_txn_finalize_parent_stacks(NRPRG(txn));
  tlib_pass_if_size_t_equal("the multi segment is ended once", ended_before + 1,
                            NRPRG(txn)->segment_count);

  nr_php_zval_free(&mh);
  tlib_php_request_end();
}

/*
 * Run bursts of transfers on a multi handle, as a loop of curl_multi_exec()
 * calls does, and return the multi segment.
 */
static nr_segment_t* run_multi_bursts(zval* mh, int bursts) {
  int burst;

  for (burst = 0; burst < bursts; burst++) {
    nr_php_curl_multi_exec_pre(mh);
    nr_php_curl_multi_exec_post(mh);
    nr_php_curl_multi_exec_finalize(mh);
  }

  return nr_php_curl_multi_md_get_segment(mh);
}

static void test_curl_multi_exec_keeps_async_context(void) {
  zval* mh;
  zval* ch;
  zval* zurl;
  int context_before;

  tlib_php_request_start();

  zurl = nr_php_zval_alloc();
  nr_php_zval_str(zurl, "https://newrelic.com");
  ch = nr_php_call(NULL, "curl_init", zurl);
  mh = nr_php_call(NULL, "curl_multi_init");
  nr_php_curl_multi_md_add(mh, ch);

  /*
   * The segments of a multi handle have an async context of their own. The
   * code that runs after the transfers must keep the context it ran in:
   * its segments and span attributes belong to the caller, not to the multi
   * segment.
   */
  context_before = NRTXN(current_async_context);

  nr_php_curl_multi_exec_pre(mh);
  tlib_pass_if_int_equal("starting the transfers keeps the async context",
                         context_before, NRTXN(current_async_context));

  nr_php_curl_multi_exec_post(mh);
  nr_php_curl_multi_exec_finalize(mh);
  tlib_pass_if_int_equal("ending the transfers keeps the async context",
                         context_before, NRTXN(current_async_context));

  nr_php_zval_free(&mh);
  nr_php_zval_free(&ch);
  nr_php_zval_free(&zurl);
  tlib_php_request_end();
}

static void test_curl_multi_exec_many_bursts(void) {
  zval* mh;
  ssize_t heap_before;

  tlib_php_request_start();

  /* A small segment heap, which evicts when it is full. */
  NRPRG(txn)->segment_heap = nr_minmax_heap_create(
      8, nr_segment_wrapped_span_priority_comparator, NULL, NULL, NULL);

  mh = nr_php_call(NULL, "curl_multi_init");
  heap_before = nr_minmax_heap_size(NRPRG(txn)->segment_heap);

  run_multi_bursts(mh, 300);
  tlib_pass_if_int_equal("the bursts put no segment in the segment heap",
                         heap_before,
                         nr_minmax_heap_size(NRPRG(txn)->segment_heap));

  nr_txn_finalize_parent_stacks(NRPRG(txn));
  tlib_pass_if_int_equal(
      "the transaction cleanup puts the multi segment in "
      "the segment heap once",
      heap_before + 1, nr_minmax_heap_size(NRPRG(txn)->segment_heap));

  nr_php_zval_free(&mh);
  tlib_php_request_end();
}

static void test_curl_multi_exec_duration_kept(void) {
  zval* mh;
  nr_segment_t* segment;
  nrtime_t stop_time;

  tlib_php_request_start();

  mh = nr_php_call(NULL, "curl_multi_init");
  segment = run_multi_bursts(mh, 3);
  tlib_pass_if_not_null("the bursts leave a multi segment", segment);
  stop_time = segment->stop_time;

  nr_txn_finalize_parent_stacks(NRPRG(txn));
  tlib_pass_if_time_equal(
      "ending the multi segment with the transaction keeps the stop time that "
      "the last burst set",
      stop_time, segment->stop_time);

  nr_php_zval_free(&mh);
  tlib_php_request_end();
}

static bool count_spans(nr_span_encoding_result_t* encoded, void* userdata) {
  *(size_t*)userdata += encoded->span_count;
  nr_span_encoding_result_deinit(encoded);

  return true;
}

/*
 * Queue the spans of the transaction, as Infinite Tracing does, and count them
 * when the queue is flushed.
 */
static void queue_spans(size_t* spans) {
  NRPRG(txn)->options.distributed_tracing_enabled = true;
  NRPRG(txn)->options.span_events_enabled = true;
  NRPRG(txn)->span_queue
      = nr_span_queue_create(1000, 1 * NR_TIME_DIVISOR, count_spans, spans);
}

static void test_curl_multi_exec_span_queue_with_handles(void) {
  zval* mh;
  zval* ch;
  zval* zurl;
  nr_segment_t* segment;
  size_t spans = 0;
  int burst;

  tlib_php_request_start();
  queue_spans(&spans);

  zurl = nr_php_zval_alloc();
  nr_php_zval_str(zurl, "https://newrelic.com");
  ch = nr_php_call(NULL, "curl_init", zurl);
  mh = nr_php_call(NULL, "curl_multi_init");

  /* Two bursts, each with one transfer. */
  for (burst = 0; burst < 2; burst++) {
    nr_php_curl_multi_md_add(mh, ch);
    if (0 == burst) {
      nr_php_curl_multi_exec_pre(mh);
    } else {
      segment = nr_php_curl_multi_md_get_segment(mh);
      nr_php_curl_exec_pre(ch, segment,
                           nr_php_curl_multi_md_get_async_context(mh));
    }
    nr_php_curl_multi_exec_post(mh);
    nr_php_curl_multi_exec_finalize(mh);
  }

  nr_span_queue_flush(NRPRG(txn)->span_queue);
  tlib_pass_if_size_t_equal("each transfer queues its span once", 2, spans);

  nr_txn_finalize_parent_stacks(NRPRG(txn));
  nr_span_queue_flush(NRPRG(txn)->span_queue);
  tlib_pass_if_size_t_equal(
      "the transaction cleanup queues the span of the multi segment once", 3,
      spans);

  nr_php_zval_free(&mh);
  nr_php_zval_free(&ch);
  nr_php_zval_free(&zurl);
  tlib_php_request_end();
}

static void test_curl_exec_reused_handle_span_queue(void) {
  zval* ch;
  zval* zurl;
  zval* zopt;
  zval* zfile;
  zval* retval;
  size_t spans = 0;

  tlib_php_request_start();
  queue_spans(&spans);

  zurl = nr_php_zval_alloc();
  nr_php_zval_str(zurl, "https://newrelic.com");
  ch = nr_php_call(NULL, "curl_init", zurl);

  nr_php_curl_exec_pre(ch, NULL, NULL);
  nr_php_curl_exec_post(ch, false);

  zopt = nr_php_zval_alloc();
  ZVAL_LONG(zopt, 10002); /* CURLOPT_URL */
  zfile = nr_php_zval_alloc();
  nr_php_zval_str(zfile, "file:///tmp/newrelic_curl_reuse");
  retval = nr_php_call(NULL, "curl_setopt", ch, zopt, zfile);
  nr_php_zval_free(&retval);

  nr_php_curl_exec_pre(ch, NULL, NULL);
  nr_php_curl_exec_post(ch, false);

  nr_span_queue_flush(NRPRG(txn)->span_queue);
  tlib_pass_if_size_t_equal(
      "a request that is not instrumented queues no span, and the earlier "
      "request queues its span once",
      1, spans);

  nr_php_zval_free(&zfile);
  nr_php_zval_free(&zopt);
  nr_php_zval_free(&ch);
  nr_php_zval_free(&zurl);
  tlib_php_request_end();
}

static void test_curl_multi_exec_span_queue(void) {
  zval* mh;
  size_t spans = 0;

  tlib_php_request_start();

  queue_spans(&spans);

  mh = nr_php_call(NULL, "curl_multi_init");
  run_multi_bursts(mh, 5);
  nr_span_queue_flush(NRPRG(txn)->span_queue);
  tlib_pass_if_size_t_equal("the bursts queue no span for the multi segment", 0,
                            spans);

  nr_txn_finalize_parent_stacks(NRPRG(txn));
  nr_span_queue_flush(NRPRG(txn)->span_queue);
  tlib_pass_if_size_t_equal(
      "the transaction cleanup queues the span of the multi segment once", 1,
      spans);

  nr_php_zval_free(&mh);
  tlib_php_request_end();
}

static void test_curl_remove_unfinished_handle(void) {
  zval* ch;
  zval* zurl;
  nr_segment_t* segment;
  size_t count_before;

  tlib_php_request_start();

  zurl = nr_php_zval_alloc();
  nr_php_zval_str(zurl, "https://newrelic.com");
  ch = nr_php_call(NULL, "curl_init", zurl);

  /*
   * A handle removed from a multi handle before its transfer finished: the
   * wrapper discards the segment it takes from the metadata.
   */
  nr_php_curl_exec_pre(ch, NULL, NULL);
  count_before = NRPRG(txn)->segment_count;

  segment = nr_php_curl_md_take_segment(ch);
  tlib_pass_if_not_null("the segment of the unfinished transfer is taken",
                        segment);
  nr_segment_discard(&segment);

  tlib_pass_if_null("the metadata forgot the discarded segment",
                    nr_php_curl_md_get_segment(ch));
  nr_php_curl_exec_post(ch, false);
  tlib_pass_if_size_t_equal("no segment is ended for the removed handle",
                            count_before, NRPRG(txn)->segment_count);

  nr_php_zval_free(&ch);
  nr_php_zval_free(&zurl);
  tlib_php_request_end();
}

#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO /* PHP 7 */
static void test_curl_multi_close(void) {
  zval* mh;
  size_t ended_before;

  tlib_php_request_start();

  mh = nr_php_call(NULL, "curl_multi_init");
  nr_php_curl_multi_exec_pre(mh);
  ended_before = NRPRG(txn)->segment_count;

  nr_php_curl_multi_close(mh);
  tlib_pass_if_size_t_equal("closing the multi handle ends its multi segment",
                            ended_before + 1, NRPRG(txn)->segment_count);

  /* The application can call curl_multi_close() again on the same resource. */
  nr_php_curl_multi_close(mh);
  tlib_pass_if_size_t_equal("closing it again does not end the segment again",
                            ended_before + 1, NRPRG(txn)->segment_count);

  nr_txn_finalize_parent_stacks(NRPRG(txn));
  tlib_pass_if_size_t_equal("the transaction cleanup does not end it again",
                            ended_before + 1, NRPRG(txn)->segment_count);

  nr_php_zval_free(&mh);
  tlib_php_request_end();
}
#endif

static void test_curl_exec_reused_handle(void) {
  zval* ch;
  zval* zurl;
  zval* zopt;
  zval* zfile;
  zval* retval;
  size_t ended_after_request;

  tlib_php_request_start();

  zurl = nr_php_zval_alloc();
  nr_php_zval_str(zurl, "https://newrelic.com");
  ch = nr_php_call(NULL, "curl_init", zurl);

  nr_php_curl_exec_pre(ch, NULL, NULL);
  nr_php_curl_exec_post(ch, false);
  ended_after_request = NRPRG(txn)->segment_count;

  /*
   * The handle is used again, for a URL that is not instrumented.
   */
  zopt = nr_php_zval_alloc();
  ZVAL_LONG(zopt, 10002); /* CURLOPT_URL */
  zfile = nr_php_zval_alloc();
  nr_php_zval_str(zfile, "file:///tmp/newrelic_curl_reuse");
  retval = nr_php_call(NULL, "curl_setopt", ch, zopt, zfile);
  nr_php_zval_free(&retval);

  nr_php_curl_exec_pre(ch, NULL, NULL);
  tlib_pass_if_null("a request that is not instrumented has no segment",
                    nr_php_curl_md_get_segment(ch));

  nr_php_curl_exec_post(ch, false);
  tlib_pass_if_size_t_equal(
      "the segment of the earlier request is not ended again",
      ended_after_request, NRPRG(txn)->segment_count);

  nr_php_zval_free(&zfile);
  nr_php_zval_free(&zopt);
  nr_php_zval_free(&ch);
  nr_php_zval_free(&zurl);
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {

  tlib_php_engine_create("" PTSRMLS_CC);

  if (tlib_php_require_extension("curl" TSRMLS_CC)) {
    test_curl_get_url(TSRMLS_C);
    test_curl_should_instrument_proto();
    test_curl_exec(TSRMLS_C);
    test_curl_multi_exec_reused_handle();
    test_curl_exec_reused_handle();
    test_curl_multi_exec_keeps_async_context();
    test_curl_multi_exec_many_bursts();
    test_curl_multi_exec_duration_kept();
    test_curl_multi_exec_span_queue();
    test_curl_multi_exec_span_queue_with_handles();
    test_curl_exec_reused_handle_span_queue();
    test_curl_remove_unfinished_handle();
#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO /* PHP 7 */
    test_curl_multi_close();
#endif
  }

  tlib_php_engine_destroy(TSRMLS_C);
}
