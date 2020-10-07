/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <unistd.h>

#include "util_buffer.h"
#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

static void test_buffer_add(nrbuf_t* buf, int dlen, int est, int ext, int* in) {
  int l1;
  int l2;

  l1 = nr_buffer_len(buf);
  nr_buffer_add(buf, in, dlen);
  l2 = nr_buffer_len(buf);
  tlib_pass_if_true("buffer add increases len correctly", (l2 - l1) == dlen,
                    "l1=%d l2=%d dlen=%d est=%d ext=%d", l1, l2, dlen, est,
                    ext);
}

static void test_buffer_use(nrbuf_t* buf, int dlen, int est, int ext, int* in) {
  int length;
  int compare_code;
  int l1;
  int l2;
  int nout;
  int* out;

  out = (int*)nr_zalloc(dlen);

  l1 = nr_buffer_len(buf);
  nout = (dlen > l1) ? l1 : dlen;
  length = nr_buffer_use(buf, out, dlen);
  tlib_pass_if_true("buffer use returns correct num", nout == length,
                    "nout=%d length=%d est=%d ext=%d", nout, length, est, ext);
  compare_code = nr_memcmp(in, out, nout);
  tlib_pass_if_true("buffer use outputs correct bytes", 0 == compare_code,
                    "compare_code=%d est=%d ext=%d", compare_code, est, ext);
  l2 = nr_buffer_len(buf);
  tlib_pass_if_true("buffer use decreases len correctly", (l1 - l2) == nout,
                    "l1=%d l2=%d nout=%d est=%d ext=%d", l1, l2, nout, est,
                    ext);
  nr_free(out);
}

/*
 * Test 1: nr_buffer_create returns a new buffer which is empty.
 * This is in its own function so that we have something to hang
 * suppressions on.
 */
static nrbuf_t* test_buffer_test1(int est, int ext) {
  nrbuf_t* buf;
  const char* rp;
  int length;

  buf = nr_buffer_create(est, ext);
  tlib_pass_if_true("new buffer not null", 0 != buf, "buf=%p est=%d ext=%d",
                    buf, est, ext);

  length = nr_buffer_len(buf);
  tlib_pass_if_true("new buffer has length zero", 0 == length,
                    "length=%d est=%d ext=%d", length, est, ext);

  rp = (const char*)nr_buffer_cptr(buf);
  tlib_pass_if_true("new buffer has null nr_buffer_cptr", 0 == rp,
                    "rp=%p est=%d ext=%d", rp, est, ext);

  return buf;
}

static void test_buffer(int est, int ext) {
  unsigned int i;
  nrbuf_t* buf;
  const void* rp;
  int rv_buffer;
  int in[2048];

  for (i = 0; i < sizeof(in) / sizeof(*in); i++) {
    in[i] = i + 1;
  }

  /*
   * Test 1: nr_buffer_create returns a new buffer which is empty.
   */
  buf = test_buffer_test1(est, ext);

  /*
   * Test 2: test add and use
   */
  test_buffer_add(buf, sizeof(int) * 22, est, ext, in);
  test_buffer_add(buf, sizeof(int) * 33, est, ext, in);
  test_buffer_use(buf, sizeof(int) * 22, est, ext, in);
  test_buffer_add(buf, sizeof(int) * 11, est, ext, in);
  test_buffer_use(buf, sizeof(int) * 33, est, ext, in);
  test_buffer_use(buf, sizeof(int) * 500, est, ext,
                  in); /* more than is in buffer */
  test_buffer_use(buf, sizeof(int) * 500, est, ext, in); /* buffer is empty */

  /*
   * Test 3: nr_buffer_destroy properly disposes, and all functions handle null
   * buffer input correctly.
   */
  nr_buffer_destroy(&buf);
  tlib_pass_if_true("nr_buffer_destroy disposes buffer", 0 == buf,
                    "buf=%p est=%d ext=%d", buf, est, ext);
  rv_buffer = nr_buffer_use(NULL, NULL, 14);
  tlib_pass_if_true("null buffer cannot be used", -1 == rv_buffer,
                    "rv_buffer=%d est=%d ext=%d", rv_buffer, est, ext);
  nr_buffer_add(NULL, in, 14); /* Don't blow up! */
  rv_buffer = nr_buffer_len(NULL);
  tlib_pass_if_true("null buffer length returns error", -1 == rv_buffer,
                    "rv_buffer=%d est=%d ext=%d", rv_buffer, est, ext);
  rp = nr_buffer_cptr(NULL);
  tlib_pass_if_true("null buffer has null ptr", 0 == rp, "rp=%p est=%d ext=%d",
                    rp, est, ext);
}

static void test_read_write_bad_params(void) {
  nr_status_t rv;
  uint32_t uint32_val = 0;
  nrbuf_t* buf;

  /*
   * NULL Buffer: Don't blow up!
   */
  nr_buffer_write_uint32_t_le(0, 1);

  uint32_val = 0;
  rv = nr_buffer_read_uint32_t_le(0, &uint32_val);
  tlib_pass_if_status_failure("null buf", rv);
  tlib_pass_if_uint32_t_equal("null buf", uint32_val, 0);

  buf = nr_buffer_create(0, 0);

  uint32_val = 0;
  rv = nr_buffer_read_uint32_t_le(buf, &uint32_val);
  tlib_pass_if_status_failure("empty buf", rv);
  tlib_pass_if_uint32_t_equal("empty buf", uint32_val, 0);

  nr_buffer_write_uint32_t_le(buf, 1234567890);

  rv = nr_buffer_read_uint32_t_le(buf, NULL);
  tlib_pass_if_status_failure("null return pointer", rv);

  uint32_val = 0;
  rv = nr_buffer_read_uint32_t_le(buf, &uint32_val);
  tlib_pass_if_status_success("empty buf", rv);
  tlib_pass_if_uint32_t_equal("empty buf", uint32_val, 1234567890);

  nr_buffer_destroy(&buf);
}

#define TEST_VAL_UINT32 0xdb975311

static void test_read_write(void) {
  nr_status_t rv;
  uint32_t uint32_val = 0;
  nrbuf_t* buf = nr_buffer_create(0, 0);

  nr_buffer_write_uint32_t_le(buf, TEST_VAL_UINT32);

  rv = nr_buffer_read_uint32_t_le(buf, &uint32_val);
  tlib_pass_if_status_success("buffer read", rv);
  tlib_pass_if_uint32_t_equal("buffer read", TEST_VAL_UINT32, uint32_val);

  nr_buffer_write_uint32_t_le(buf, TEST_VAL_UINT32 + 2);
  nr_buffer_write_uint32_t_le(buf, TEST_VAL_UINT32 + 5);

  rv = nr_buffer_read_uint32_t_le(buf, &uint32_val);
  tlib_pass_if_status_success("buffer read", rv);
  tlib_pass_if_uint32_t_equal("buffer read", TEST_VAL_UINT32 + 2, uint32_val);
  rv = nr_buffer_read_uint32_t_le(buf, &uint32_val);
  tlib_pass_if_status_success("buffer read", rv);
  tlib_pass_if_uint32_t_equal("buffer read", TEST_VAL_UINT32 + 5, uint32_val);

  nr_buffer_destroy(&buf);
}

static void test_peek_end(void) {
  nrbuf_t* buf;
  char actual = 0;

  // Test : Should not blow up if given NULL
  actual = nr_buffer_peek_end(NULL);
  tlib_pass_if_true("NULL buffer peek did not fail", 0 == actual, "actual=%c",
                    actual);

  buf = nr_buffer_create(0, 0);

  // Test : Should not blow up if nothing is in the buf
  actual = nr_buffer_peek_end(buf);
  tlib_pass_if_true("empty buffer peek did not fail", 0 == actual, "actual=%c",
                    actual);

  // Test : happy path
  nr_buffer_add(buf, "[asdf", 5);

  actual = nr_buffer_peek_end(buf);

  tlib_pass_if_true("success", 'f' == actual, "bufptr=%c", actual);

  // Test : The previous test should not have changed the buf
  actual = nr_buffer_peek_end(buf);

  tlib_pass_if_true("success", 'f' == actual, "bufptr=%c", actual);

  // Test : Null char should not blow up
  nr_buffer_add(buf, "something\0", 10);
  actual = nr_buffer_peek_end(buf);

  tlib_pass_if_true("NULL term string peek did not fail", 0 == actual,
                    "actual=%c", actual);

  nr_buffer_destroy(&buf);
}

static void test_write_uint64_t_as_text(void) {
  nrbuf_t* buf;
  const char* bufptr;

  /* NULL Buffer:  Don't blow up! */
  nr_buffer_write_uint64_t_as_text(0, 12345);

  buf = nr_buffer_create(0, 0);
  nr_buffer_write_uint64_t_as_text(buf, 12345678901234567890ULL);
  nr_buffer_add(buf, "\0", 1); /* Add Nul terminator */
  bufptr = (const char*)nr_buffer_cptr(buf);
  tlib_pass_if_true("success", 0 == nr_strcmp("12345678901234567890", bufptr),
                    "bufptr=%s", NRSAFESTR(bufptr));
  nr_buffer_destroy(&buf);

  buf = nr_buffer_create(0, 0);
  nr_buffer_write_uint64_t_as_text(buf, 0ULL);
  nr_buffer_add(buf, "\0", 1); /* Add Nul terminator */
  bufptr = (const char*)nr_buffer_cptr(buf);
  tlib_pass_if_true("success", 0 == nr_strcmp("0", bufptr), "bufptr=%s",
                    NRSAFESTR(bufptr));
  nr_buffer_destroy(&buf);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_read_write_bad_params();
  test_read_write();

  test_buffer(18, 0);
  test_buffer(0, 0);
  test_buffer(0, 18);
  test_buffer(1, 1);
  test_buffer(-1, -1);

  test_write_uint64_t_as_text();
  test_peek_end();
}
