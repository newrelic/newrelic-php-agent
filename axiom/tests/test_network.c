/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <errno.h>

#include "util_memory.h"
#include "util_network.h"
#include "util_strings.h"
#include "util_syscalls.h"

#include "tlib_main.h"

static void setup_pair(int socks[2]) {
  int rv;
  nr_status_t st;

  rv = socketpair(AF_UNIX, SOCK_STREAM, 0, socks);
  tlib_pass_if_true("socketpair works", 0 == rv, "rv=%d errno=%d", rv, errno);

  st = nr_network_set_non_blocking(socks[0]);
  tlib_pass_if_status_success("socks[0] set to nonblocking", st);

  st = nr_network_set_non_blocking(socks[1]);
  tlib_pass_if_status_success("socks[1] set to nonblocking", st);
}

static void test_write_bad_params(void) {
  nr_status_t rv;
  nrtime_t deadline = 0;
  const char* ts1 = "Test";

  rv = nr_write_full(123, ts1, 0, deadline);
  tlib_pass_if_status_success("0 len", rv);

  rv = nr_write_full(123, 0, 5, deadline);
  tlib_pass_if_status_failure("NULL buffer fails", rv);

  rv = nr_write_full(-1, ts1, 5, deadline);
  tlib_pass_if_status_failure("bad fd fails", rv);
}

static void test_read_write(void) {
  int rv;
  int socks[2];
  nrbuf_t* buf;
  char tmp[128];
  const char* ts1 = "Test";
  nr_status_t st;
  nrtime_t deadline = 0;

  setup_pair(socks);

  st = nr_write_full(socks[0], ts1, 5, deadline);
  tlib_pass_if_status_success("basic write works", st);

  buf = nrn_read(socks[1], 5, deadline);
  tlib_pass_if_int_equal("basic read works", 5, nr_buffer_len(buf));

  nr_memset(tmp, 0, 5);
  rv = nr_buffer_use(buf, tmp, 5);
  tlib_pass_if_int_equal("use read buffer works", 5, rv);

  rv = nr_strcmp(tmp, ts1);
  tlib_pass_if_int_equal("read correct", 0, rv);

  nr_buffer_destroy(&buf);
  nr_close(socks[0]);
  nr_close(socks[1]);
}

static void test_read_after_close(void) {
  int rv;
  int socks[2];
  nrbuf_t* buf;
  char tmp[128];
  const char* ts1 = "Test";
  nr_status_t st;
  nrtime_t deadline = 0;

  setup_pair(socks);

  /*
   * Write some bytes, close one end of the socket.
   */
  st = nr_write_full(socks[0], ts1, 5, deadline);
  tlib_pass_if_status_success("basic write works", st);

  nr_close(socks[0]);

  buf = nrn_read(socks[1], 5, deadline);
  tlib_pass_if_int_equal("read after works", 5, nr_buffer_len(buf));

  nr_memset(tmp, 0, 5);
  rv = nr_buffer_use(buf, tmp, 5);
  tlib_pass_if_int_equal("use read buffer works", 5, rv);

  rv = nr_strcmp(tmp, ts1);
  tlib_pass_if_int_equal("read after close correct", 0, rv);

  nr_buffer_destroy(&buf);

  buf = nrn_read(socks[1], 5, deadline);
  tlib_pass_if_null("read after close fails", buf);
  nr_buffer_destroy(&buf);

  nr_close(socks[1]);
}

static void test_multi_read_write(void) {
  int socks[2];
  nrbuf_t* buf;
  const char* ts1 = "Test";
  nr_status_t st;
  nrtime_t deadline = 0;

  /*
   * Multiple writes, count and reads work.
   */
  setup_pair(socks);

  st = nr_write_full(socks[0], ts1, 5, deadline);
  tlib_pass_if_status_success("1st write ok", st);
  st = nr_write_full(socks[0], ts1, 5, deadline);
  tlib_pass_if_status_success("2nd write ok", st);
  st = nr_write_full(socks[0], ts1, 5, deadline);
  tlib_pass_if_status_success("3rd write ok", st);

  buf = nrn_read(socks[1], 10, deadline);
  tlib_pass_if_int_equal("partial read ok", 10, nr_buffer_len(buf));
  nr_buffer_destroy(&buf);

  buf = nrn_read(socks[1], 5, deadline);
  tlib_pass_if_int_equal("read the rest ok", 5, nr_buffer_len(buf));
  nr_buffer_destroy(&buf);

  nr_close(socks[0]);
  nr_close(socks[1]);
}

static void test_write_after_close(void) {
  int socks[2];
  const char* ts1 = "Test";
  nr_status_t st;
  nrtime_t deadline = 0;

  setup_pair(socks);

  /*
   * Write then close reader and second write should fail.
   */
  st = nr_write_full(socks[0], ts1, 5, deadline);
  tlib_pass_if_status_success("1st write ok", st);

  nr_close(socks[1]);

  st = nr_write_full(socks[0], ts1, 5, deadline);
  tlib_pass_if_status_failure("2nd write fails", st);

  nr_close(socks[0]);
}

static void test_write_parse_preamble(void) {
  nrbuf_t* buf = nr_buffer_create(0, 0);
  uint32_t datalen;
  nr_status_t rv;

  datalen = 12345;
  nr_protocol_write_preamble(buf, datalen);

  datalen = 0;
  rv = nr_protocol_parse_preamble(buf, &datalen);
  tlib_pass_if_status_success("parse preamble success", rv);
  tlib_pass_if_int_equal("parse preamble success", 12345, datalen);
  tlib_pass_if_int_equal("parse preamble success", 0, nr_buffer_len(buf));

  nr_buffer_destroy(&buf);
}

static void test_parse_preamble_bad_params(void) {
  nrbuf_t* buf = nr_buffer_create(0, 0);
  uint32_t datalen;
  nr_status_t rv;

  datalen = 12345;
  nr_protocol_write_preamble(buf, datalen);

  rv = nr_protocol_parse_preamble(0, 0);
  tlib_pass_if_status_failure("null params", rv);
  rv = nr_protocol_parse_preamble(0, &datalen);
  tlib_pass_if_status_failure("null buf", rv);
  rv = nr_protocol_parse_preamble(buf, 0);
  tlib_pass_if_status_failure("null datalen ptr", rv);

  rv = nr_protocol_parse_preamble(buf, &datalen);
  tlib_pass_if_status_success("success", rv);

  nr_buffer_destroy(&buf);
}

static void test_parse_preamble_corrupted(void) {
  nrbuf_t* buf;
  uint32_t datalen = 0;
  nr_status_t rv;

  buf = nr_buffer_create(0, 0);
  nr_buffer_write_uint32_t_le(buf, 1);
  rv = nr_protocol_parse_preamble(buf, &datalen);
  tlib_pass_if_status_failure("too short", rv);
  tlib_pass_if_int_equal("too short", 0, datalen);
  nr_buffer_destroy(&buf);

  buf = nr_buffer_create(0, 0);
  nr_buffer_write_uint32_t_le(buf, 1);
  nr_buffer_write_uint32_t_le(buf, NR_PREAMBLE_FORMAT + 1);
  rv = nr_protocol_parse_preamble(buf, &datalen);
  tlib_pass_if_status_failure("bad format", rv);
  tlib_pass_if_int_equal("bad format", 0, datalen);
  nr_buffer_destroy(&buf);

  buf = nr_buffer_create(0, 0);
  nr_buffer_write_uint32_t_le(buf, NR_PROTOCOL_CMDLEN_MAX_BYTES + 1);
  nr_buffer_write_uint32_t_le(buf, NR_PREAMBLE_FORMAT);
  rv = nr_protocol_parse_preamble(buf, &datalen);
  tlib_pass_if_status_failure("datalen too large", rv);
  tlib_pass_if_int_equal("datalen too large", 0, datalen);
  nr_buffer_destroy(&buf);

  buf = nr_buffer_create(0, 0);
  nr_buffer_write_uint32_t_le(buf, 1);
  nr_buffer_write_uint32_t_le(buf, NR_PREAMBLE_FORMAT);
  rv = nr_protocol_parse_preamble(buf, &datalen);
  tlib_pass_if_status_success("success", rv);
  tlib_pass_if_int_equal("success", 1, datalen);
  nr_buffer_destroy(&buf);
}

#define TEST_NETWORK_TIMEOUT_MS 10

static void test_send_receive_success(void) {
  int socks[2];
  nr_status_t st;
  nrbuf_t* buf;
  nrtime_t deadline;

  setup_pair(socks);

  deadline = nr_get_time() + (TEST_NETWORK_TIMEOUT_MS * NR_TIME_DIVISOR_MS);
  st = nr_write_message(socks[0], "Hello, World!", 13, deadline);
  tlib_pass_if_status_success("send success", st);

  buf = nr_network_receive(socks[1], 0);
  nr_buffer_add(buf, "\0", 1);
  tlib_pass_if_str_equal(__func__, "Hello, World!",
                         (const char*)nr_buffer_cptr(buf));
  nr_buffer_destroy(&buf);

  nr_close(socks[0]);
  nr_close(socks[1]);
}

static void test_send_bad_params(void) {
  int socks[2];
  nr_status_t st;
  int bad_fd;
  nrtime_t deadline;

  setup_pair(socks);

  deadline = nr_get_time() + (TEST_NETWORK_TIMEOUT_MS * NR_TIME_DIVISOR_MS);
  st = nr_write_message(-1, "Hello, World!", 13, deadline);
  tlib_pass_if_status_failure("negative fd", st);

  deadline = nr_get_time() + (TEST_NETWORK_TIMEOUT_MS * NR_TIME_DIVISOR_MS);
  st = nr_write_message(socks[0], NULL, 13, deadline);
  tlib_pass_if_status_failure("null data", st);

  deadline = nr_get_time() + (TEST_NETWORK_TIMEOUT_MS * NR_TIME_DIVISOR_MS);
  st = nr_write_message(socks[0], "hello world",
                        NR_PROTOCOL_CMDLEN_MAX_BYTES + 1, deadline);
  tlib_pass_if_status_failure("excessive len", st);

  bad_fd = socks[0];
  nr_close(socks[0]);
  nr_close(socks[1]);

  deadline = nr_get_time() + (TEST_NETWORK_TIMEOUT_MS * NR_TIME_DIVISOR_MS);
  st = nr_write_message(bad_fd, "Hello, World!", 13, deadline);
  tlib_pass_if_status_failure("bad fd", st);
}

static void test_receive_bad_params(void) {
  int socks[2];
  int bad_fd;
  nrbuf_t* buf;
  nrtime_t deadline;

  setup_pair(socks);
  deadline = nr_get_time() + (TEST_NETWORK_TIMEOUT_MS * NR_TIME_DIVISOR_MS);
  nr_write_message(socks[0], "Hello, World!", 13, deadline);

  buf = nr_network_receive(-1, 0);
  tlib_pass_if_null("negative fd", buf);

  bad_fd = socks[0];
  nr_close(socks[0]);
  nr_close(socks[1]);

  buf = nr_network_receive(bad_fd, 0);
  tlib_pass_if_null("bad fd", buf);
}

static void test_receive_corrupted(void) {
  int socks[2];
  const char* data_json = "\"hello\"";
  int len = nr_strlen(data_json);
  nrbuf_t* reply;
  nrbuf_t* buf;

  setup_pair(socks);

  buf = nr_buffer_create(0, 0);
  nr_buffer_write_uint32_t_le(buf, len);
  reply = nr_network_receive(socks[1], 0);
  tlib_pass_if_null("incomplete preamble", reply);
  nr_buffer_destroy(&buf);

  buf = nr_buffer_create(0, 0);
  nr_buffer_write_uint32_t_le(buf, len);
  nr_buffer_write_uint32_t_le(buf, NR_PREAMBLE_FORMAT + 1);
  nr_buffer_add(buf, data_json, len);
  nr_write_full(socks[0], nr_buffer_cptr(buf), nr_buffer_len(buf), 0);
  reply = nr_network_receive(socks[1], 0);
  tlib_pass_if_null("bad preamble", reply);
  nr_buffer_destroy(&buf);

  buf = nr_buffer_create(0, 0);
  nr_buffer_write_uint32_t_le(buf, len);
  nr_buffer_write_uint32_t_le(buf, NR_PREAMBLE_FORMAT);
  nr_buffer_add(buf, data_json, len - 1);
  nr_write_full(socks[0], nr_buffer_cptr(buf), nr_buffer_len(buf), 0);
  reply = nr_network_receive(socks[1], 0);
  tlib_pass_if_null("incomplete data", reply);
  nr_buffer_destroy(&buf);

  nr_close(socks[0]);
  nr_close(socks[1]);
}

static void test_read_bad_params(void) {
  int bad_fd;
  int socks[2];
  nrbuf_t* reply;
  const char* data = "hello";
  uint32_t datalen = nr_strlen("hello");
  nrtime_t deadline = 0;

  setup_pair(socks);
  nr_write_full(socks[0], data, datalen, 0);

  reply = nrn_read(-1, datalen, deadline);
  tlib_pass_if_null("negative fd", reply);

  reply = nrn_read(socks[1], 0, deadline);
  tlib_pass_if_null("zero nbytes", reply);

  bad_fd = socks[0];
  nr_close(socks[0]);
  nr_close(socks[1]);

  reply = nrn_read(bad_fd, datalen, deadline);
  tlib_pass_if_null("bad fd", reply);
}

static void test_read_times_out(void) {
  int socks[2];
  nrtime_t start;
  nrtime_t stop;
  int duration_msec;
  nrtime_t deadline;
  nrbuf_t* reply;

  setup_pair(socks);

  start = nr_get_time();
  deadline = start + (10 * NR_TIME_DIVISOR_MS);
  reply = nrn_read(socks[1], 10, deadline);
  stop = nr_get_time();
  duration_msec = (int)((stop - start) / NR_TIME_DIVISOR_MS);

  tlib_pass_if_null("times out", reply);
  /* This range is very large to account for Valgrind time dilation */
  tlib_pass_if_true("times out",
  /*
   * This is large on BSD and sometimes on linux.
   */
#if defined(__FreeBSD__) || defined(__linux__)
                    (duration_msec >= 8) && (duration_msec < 250),
#else
                    (duration_msec >= 8) && (duration_msec < 40),
#endif
                    "duration_msec=%d", duration_msec);

  nr_close(socks[0]);
  nr_close(socks[1]);
  nr_buffer_destroy(&reply);
}

static void test_set_nonblocking_bad_param(void) {
  nr_status_t st = nr_network_set_non_blocking(-1);

  tlib_pass_if_status_failure("negative fd", st);
}

/*
 * This doesn't run in parallel
 * due to races in saved_syscalls data structures.
 *
 * We don't expect to be swapping that (effective) vtbl
 * in real life, so don't bother testing it here.
 */
tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  /* sigpipe ignored to allow testing of failed writes */
  tlib_ignore_sigpipe();

  test_write_bad_params();
  test_read_write();
  test_read_after_close();
  test_multi_read_write();
  test_write_after_close();

  test_write_parse_preamble();
  test_parse_preamble_bad_params();
  test_parse_preamble_corrupted();

  test_read_bad_params();
  test_send_receive_success();
  test_send_bad_params();
  test_receive_bad_params();
  test_receive_corrupted();
  test_read_bad_params();
  test_read_times_out();

  test_set_nonblocking_bad_param();
}
