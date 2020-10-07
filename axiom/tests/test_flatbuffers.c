/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_commands.h"
#include "nr_commands_private.h"
#include "util_flatbuffers.h"
#include "util_memory.h"
#include "util_random.h"
#include "util_strings.h"

#include "tlib_main.h"

static void test_bad_input(void) {
  tlib_pass_if_null(__func__, nr_flatbuffers_data(NULL));
  tlib_pass_if_size_t_equal(__func__, 0, nr_flatbuffers_len(NULL));
  tlib_pass_if_status_failure(__func__, nr_flatbuffers_object_begin(NULL, 1));
}

static void test_read_uoffset(void) {
  uint8_t in[] = {0xEF, 0xBE, 0xAD, 0xDE, 0x78, 0x56, 0x34, 0x12};

  tlib_pass_if_uint32_t_equal(__func__, 0x12345678,
                              nr_flatbuffers_read_uoffset(in, 4));
}

#define test_bytes_equal(E, ELEN, A) \
  test_bytes_equal_fn(__func__, (E), (ELEN), (A), __FILE__, __LINE__)

static void test_bytes_equal_fn(const char* testname,
                                const uint8_t* expected,
                                size_t expected_len,
                                const nr_flatbuffer_t* fb,
                                const char* file,
                                int line) {
  tlib_pass_if_bytes_equal_f(testname, expected, expected_len,
                             nr_flatbuffers_data(fb), nr_flatbuffers_len(fb),
                             file, line);
}

static void test_byte_layout_numbers(void) {
  nr_flatbuffer_t* fb;
  const uint8_t* expected;

  fb = nr_flatbuffers_create(0);
  tlib_pass_if_size_t_equal(__func__, 0, nr_flatbuffers_len(fb));

  expected = (const uint8_t[]){0x01};
  nr_flatbuffers_prepend_bool(fb, 1);
  test_bytes_equal(expected, 1, fb);

  expected = (const uint8_t[]){0x81, 0x01};
  nr_flatbuffers_prepend_i8(fb, -127);
  test_bytes_equal(expected, 2, fb);

  expected = (const uint8_t[]){0xFF, 0x81, 0x01};
  nr_flatbuffers_prepend_u8(fb, 255);
  test_bytes_equal(expected, 3, fb);

  /* First field that requires padding. */
  expected = (const uint8_t[]){0x22, 0x82, 0x00, 0xFF, 0x81, 0x01};
  nr_flatbuffers_prepend_i16(fb, -32222);
  test_bytes_equal(expected, 6, fb);

  /* No padding required this time. */
  expected = (const uint8_t[]){0xEE, 0xFE, 0x22, 0x82, 0x00, 0xFF, 0x81, 0x01};
  nr_flatbuffers_prepend_u16(fb, 0xFEEE);
  test_bytes_equal(expected, 8, fb);

  expected = (const uint8_t[]){0xCC, 0xCC, 0xCC, 0xFC, 0xEE, 0xFE,
                               0x22, 0x82, 0x00, 0xFF, 0x81, 0x01};
  nr_flatbuffers_prepend_i32(fb, -53687092);
  test_bytes_equal(expected, 12, fb);

  expected = (const uint8_t[]){0x32, 0x54, 0x76, 0x98, 0xCC, 0xCC, 0xCC, 0xFC,
                               0xEE, 0xFE, 0x22, 0x82, 0x00, 0xFF, 0x81, 0x01};
  nr_flatbuffers_prepend_u32(fb, 0x98765432);
  test_bytes_equal(expected, 16, fb);

  nr_flatbuffers_destroy(&fb);

  fb = nr_flatbuffers_create(0);
  expected = (const uint8_t[]){0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
  nr_flatbuffers_prepend_u64(fb, 0x1122334455667788LL);
  test_bytes_equal(expected, 8, fb);
  nr_flatbuffers_destroy(&fb);
}

static void test_byte_layout_vectors(void) {
  nr_flatbuffer_t* fb;
  const uint8_t* expected;

  /*
   * 1xbyte
   */
  fb = nr_flatbuffers_create(0);

  expected = (const uint8_t[]){0, 0, 0};
  nr_flatbuffers_vector_begin(fb, sizeof(uint8_t), 1, 1);
  test_bytes_equal(expected, 3, fb);

  expected = (const uint8_t[]){1, 0, 0, 0};
  nr_flatbuffers_prepend_u8(fb, 1);
  test_bytes_equal(expected, 4, fb);

  expected = (const uint8_t[]){1, 0, 0, 0, 1, 0, 0, 0};
  nr_flatbuffers_vector_end(fb, 1);
  test_bytes_equal(expected, 8, fb);
  nr_flatbuffers_destroy(&fb);

  /*
   * 2xbyte
   */
  fb = nr_flatbuffers_create(0);

  expected = (const uint8_t[]){0, 0};
  nr_flatbuffers_vector_begin(fb, sizeof(uint8_t), 2, 1);
  test_bytes_equal(expected, 2, fb);

  expected = (const uint8_t[]){1, 0, 0};
  nr_flatbuffers_prepend_u8(fb, 1);
  test_bytes_equal(expected, 3, fb);

  expected = (const uint8_t[]){2, 1, 0, 0};
  nr_flatbuffers_prepend_u8(fb, 2);
  test_bytes_equal(expected, 4, fb);

  expected = (const uint8_t[]){2, 0, 0, 0, 2, 1, 0, 0};
  nr_flatbuffers_vector_end(fb, 2);
  test_bytes_equal(expected, 8, fb);
  nr_flatbuffers_destroy(&fb);

  /*
   * 11xbyte vector matches builder size.
   */
  fb = nr_flatbuffers_create(12);
  expected = (const uint8_t[]){0x08, 0x00, 0x00, 0x00, /* length */
                               0x0B, 0x0A, 0x09, 0x08, /* data */
                               0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};

  nr_flatbuffers_vector_begin(fb, sizeof(uint8_t), 8, 1);
  for (uint8_t i = 1; i < 12; i++) {
    nr_flatbuffers_prepend_u8(fb, i);
  }
  nr_flatbuffers_vector_end(fb, 8);
  test_bytes_equal(expected, 15, fb);
  nr_flatbuffers_destroy(&fb);

  /*
   * 1 x uint16
   */
  fb = nr_flatbuffers_create(0);

  expected = (const uint8_t[]){0, 0};
  nr_flatbuffers_vector_begin(fb, sizeof(uint16_t), 1, 1);
  test_bytes_equal(expected, 2, fb);

  expected = (const uint8_t[]){1, 0, 0, 0};
  nr_flatbuffers_prepend_u16(fb, 1);
  test_bytes_equal(expected, 4, fb);

  expected = (const uint8_t[]){1, 0, 0, 0, 1, 0, 0, 0};
  nr_flatbuffers_vector_end(fb, 1);
  test_bytes_equal(expected, 8, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * 2 x uint16
   */
  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_vector_begin(fb, sizeof(uint16_t), 2, 1);

  nr_flatbuffers_prepend_u16(fb, 0xABCD);
  expected = (const uint8_t[]){0xCD, 0xAB};
  test_bytes_equal(expected, 2, fb);

  nr_flatbuffers_prepend_u16(fb, 0xDCBA);
  expected = (const uint8_t[]){0xBA, 0xDC, 0xCD, 0xAB};
  test_bytes_equal(expected, 4, fb);

  nr_flatbuffers_vector_end(fb, 2);
  expected = (const uint8_t[]){0x02, 0x00, 0x00, 0x00, 0xBA, 0xDC, 0xCD, 0xAB};
  test_bytes_equal(expected, 8, fb);

  nr_flatbuffers_destroy(&fb);
}

static void test_byte_layout_strings(void) {
  uint32_t offset;
  nr_flatbuffer_t* fb;
  uint8_t expected[] = {
      0, 0, 0, 0, 0,                     /* final prepend empty string */
      0, 0, 0,                           /* padding */
      4, 0, 0, 0, 'm', 'o', 'o', 'p', 0, /* second string + NUL */
      0, 0, 0,                           /* padding */
      3, 0, 0, 0, 'f', 'o', 'o', 0,      /* first string + NUL */
  };

  fb = nr_flatbuffers_create(0);

  offset = nr_flatbuffers_prepend_string(fb, "foo");
  tlib_pass_if_uint32_t_equal("prepend string", offset, 8);
  test_bytes_equal(&expected[20], 8, fb);

  offset = nr_flatbuffers_prepend_string(fb, "moop");
  tlib_pass_if_uint32_t_equal("prepend string", offset, 20);
  test_bytes_equal(&expected[8], 20, fb);

  offset = nr_flatbuffers_prepend_string(fb, NULL);
  tlib_pass_if_uint32_t_equal("prepend NULL string", offset, 0);

  offset = nr_flatbuffers_prepend_string(fb, "");
  tlib_pass_if_uint32_t_equal("prepend empty string", offset, 28);
  test_bytes_equal(&expected[0], 28, fb);

  nr_flatbuffers_destroy(&fb);
}

static void test_byte_layout_utf8(void) {
  nr_flatbuffer_t* fb;
  uint8_t expected[] = {
      9,   0,   0,   0,                               /* length */
      230, 151, 165, 230, 156, 172, 232, 170, 158, 0, /* data  */
      0,   0                                          /* padding */
  };

  fb = nr_flatbuffers_create(0);

  /*
   * These characters are chinese from blog.golang.org/strings.
   * Original: ""
   */
  nr_flatbuffers_prepend_string(fb, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
  test_bytes_equal(expected, 16, fb);

  nr_flatbuffers_destroy(&fb);
}

static void test_byte_layout_vtables(void) {
  nr_flatbuffer_t* fb;
  const uint8_t* expected;
  uint32_t vector_end;
  uint32_t object_end;

  /*
   * Table with no fields.
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 0);
  nr_flatbuffers_object_end(fb);

  expected = (const uint8_t[]){
      4, 0,      /* vtable size */
      4, 0,      /* object size */
      4, 0, 0, 0 /* vtable offset */
  };
  test_bytes_equal(expected, 8, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Table with one bool field.
   *
   * table T { a: bool; };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_bool(fb, 0, 1, 0);
  nr_flatbuffers_object_end(fb);

  expected = (const uint8_t[]){
      6, 0,       /* vtable size */
      8, 0,       /* object size */
      7, 0,       /* vtable[0]: T.a */
      6, 0, 0, 0, /* vtable offset */
      0, 0, 0,    /* padded */
      1,          /* T.a */
  };
  test_bytes_equal(expected, 14, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Table with one bool field having default value.
   *
   * table T { a: bool; };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_bool(fb, 0, 0, 0);
  nr_flatbuffers_object_end(fb);

  expected = (const uint8_t[]){
      6, 0,       /* vtable size */
      4, 0,       /* object size */
      0, 0,       /* vtable[0]: T.a */
      6, 0, 0, 0, /* vtable offset */
  };
  test_bytes_equal(expected, 10, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Table with one scalar field with same alignment as the vtable.
   *
   * table T { a: short; };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_i16(fb, 0, 0x789A, 0);
  nr_flatbuffers_object_end(fb);

  expected = (const uint8_t[]){
      6,    0,          /* vtable size */
      8,    0,          /* object size */
      6,    0,          /* vtable[0]: T.a */
      6,    0,    0, 0, /* vtable offset */
      0,    0,          /* padding */
      0x9A, 0x78,       /* T.a */
  };
  test_bytes_equal(expected, 14, fb);
  nr_flatbuffers_destroy(&fb);

  /*
   * Table with two fields with uniform size.
   *
   * table T { a: short; b: short; };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 2);
  nr_flatbuffers_object_prepend_i16(fb, 0, 0x3456, 0);
  nr_flatbuffers_object_prepend_i16(fb, 1, 0x789A, 0);
  nr_flatbuffers_object_end(fb);

  expected = (const uint8_t[]){
      8,    0,          /* vtable size */
      8,    0,          /* object size */
      6,    0,          /* vtable[0]: T.a */
      4,    0,          /* vtable[1]: T.b */
      8,    0,    0, 0, /* vtable offset */
      0x9A, 0x78,       /* T.b */
      0x56, 0x34,       /* T.a */
  };
  test_bytes_equal(expected, 16, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Table with int16 and bool fields.
   *
   * table T { a: short; b: bool };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 2);
  nr_flatbuffers_object_prepend_i16(fb, 0, 0x3456, 0);
  nr_flatbuffers_object_prepend_bool(fb, 1, 1, 0);
  nr_flatbuffers_object_end(fb);

  expected = (const uint8_t[]){
      8,    0,          /* vtable size */
      8,    0,          /* object size */
      6,    0,          /* vtable[0]: T.a */
      5,    0,          /* vtable[1]: T.b */
      8,    0,    0, 0, /* vtable offset */
      0,                /* padding */
      1,                /* T.b */
      0x56, 0x34,       /* T.a */
  };
  test_bytes_equal(expected, 16, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Table of empty vector.
   *
   * table T { a: [uint]; };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_vector_begin(fb, sizeof(uint8_t), 0, 1);
  vector_end = nr_flatbuffers_vector_end(fb, 0);
  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_u32(fb, 0, vector_end, 0);
  nr_flatbuffers_object_end(fb);

  expected = (const uint8_t[]){
      6, 0,       /* vtable size */
      8, 0,       /* object size */
      4, 0,       /* vtable[0]: T.a */
      6, 0, 0, 0, /* vtable offset */
      4, 0, 0, 0, /* T.a (offset of T.a[0]) */
      0, 0, 0, 0, /* length of vector */
  };
  test_bytes_equal(expected, 18, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Table with empty vector of byte and a scalar field.
   *
   * table T { a: short; b: [byte] };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_vector_begin(fb, sizeof(uint8_t), 0, 1);
  vector_end = nr_flatbuffers_vector_end(fb, 0);
  nr_flatbuffers_object_begin(fb, 2);
  nr_flatbuffers_object_prepend_i16(fb, 0, 55, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, 1, vector_end, 0);
  nr_flatbuffers_object_end(fb);

  expected = (const uint8_t[]){
      8,  0,        /* vtable size */
      12, 0,        /* object size */
      10, 0,        /* vtable[0]: offset of T.a */
      4,  0,        /* vtable[1]: offset of T.b */
      8,  0, 0,  0, /* vtable offset */
      8,  0, 0,  0, /* T.b (offset to T.b[0]) */
      0,  0, 55, 0, /* T.a */
      0,  0, 0,  0, /* length of vector */
  };
  test_bytes_equal(expected, 24, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Table with scalar and vector having same (16-bit) alignment.
   *
   * table T { a: short; b: [short]; };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_vector_begin(fb, sizeof(uint16_t), 2, 1);
  nr_flatbuffers_prepend_u16(fb, 0x1234); /* T.b[1] */
  nr_flatbuffers_prepend_u16(fb, 0x5678); /* T.b[0] */
  vector_end = nr_flatbuffers_vector_end(fb, 2);
  nr_flatbuffers_object_begin(fb, 2);
  nr_flatbuffers_object_prepend_uoffset(fb, 1, vector_end, 0); /* T.b */
  nr_flatbuffers_object_prepend_i16(fb, 0, 55, 0);             /* T.a */
  nr_flatbuffers_object_end(fb);

  expected = (const uint8_t[]){
      8,    0,          /* vtable size */
      12,   0,          /* object size */
      6,    0,          /* vtable[0]: offset of T.a */
      8,    0,          /* vtable[1]: offset of T.b */
      8,    0,    0, 0, /* offset to vtable */
      0,    0,          /* padding */
      55,   0,          /* T.a */
      4,    0,    0, 0, /* T.b (offset to vector) */
      2,    0,    0, 0, /* vector length */
      0x78, 0x56,       /* vector[0] */
      0x34, 0x12,       /* vector[1] */
  };
  test_bytes_equal(expected, 28, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Table containing a vector of struct.
   *
   * struct S { a: byte, b: byte };
   * table T { v: [S] };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_vector_begin(fb, 2 * sizeof(uint8_t), 2, 1);
  nr_flatbuffers_prepend_i8(fb, 33); /* T.v[1].b */
  nr_flatbuffers_prepend_i8(fb, 44); /* T.v[1].a */
  nr_flatbuffers_prepend_i8(fb, 55); /* T.v[0].b */
  nr_flatbuffers_prepend_i8(fb, 66); /* T.v[0].a */
  vector_end = nr_flatbuffers_vector_end(fb, 2);
  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_uoffset(fb, 0, vector_end, 0); /* T.v */
  nr_flatbuffers_object_end(fb);

  expected = (const uint8_t[]){
      6,  0,       /* vtable size */
      8,  0,       /* object size */
      4,  0,       /* vtable[0] */
      6,  0, 0, 0, /* vtable offset */
      4,  0, 0, 0, /* vector offset */
      2,  0, 0, 0, /* vector length */
      66,          /* T.v[0].a */
      55,          /* T.v[0].b */
      44,          /* T.v[1].a */
      33,          /* T.v[1].b */
  };
  test_bytes_equal(expected, 22, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Object with scalar fields having different alignments.
   *
   * table T {
   *  a: byte;
   *  b: short;
   * };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 2);
  nr_flatbuffers_object_prepend_i8(fb, 0, 33, 0);
  nr_flatbuffers_object_prepend_i16(fb, 1, 66, 0);
  object_end = nr_flatbuffers_object_end(fb);
  nr_flatbuffers_finish(fb, object_end);

  expected = (const uint8_t[]){
      12, 0, 0, 0, /* root object offset */
      8,  0,       /* vtable size */
      8,  0,       /* object size */
      7,  0,       /* vtable[0]: T.a */
      4,  0,       /* vtable[1]: T.b */
      8,  0, 0, 0, /* vtable offset */
      66, 0,       /* T.b */
      0,           /* padding */
      33,          /* T.a */
  };
  test_bytes_equal(expected, 20, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Two consecutive root objects.
   *
   * table T1 { a: byte; b: byte; };
   * table T2 { a: byte; b: byte; c: byte };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 2);
  nr_flatbuffers_object_prepend_i8(fb, 0, 33, 0); /* T1.a */
  nr_flatbuffers_object_prepend_i8(fb, 1, 44, 0); /* T1.b */
  object_end = nr_flatbuffers_object_end(fb);
  nr_flatbuffers_finish(fb, object_end);

  nr_flatbuffers_object_begin(fb, 3);
  nr_flatbuffers_object_prepend_i8(fb, 0, 55, 0); /* T2.a */
  nr_flatbuffers_object_prepend_i8(fb, 1, 66, 0); /* T2.b */
  nr_flatbuffers_object_prepend_i8(fb, 2, 77, 0); /* T2.c */
  object_end = nr_flatbuffers_object_end(fb);
  nr_flatbuffers_finish(fb, object_end);

  expected = (const uint8_t[]){
      16, 0, 0, 0, /* root object offset */
      0,  0,       /* padding */
      10, 0,       /* vtable size */
      8,  0,       /* object size */
      7,  0,       /* vtable[0]: T2.a */
      6,  0,       /* vtable[1]: T2.b */
      5,  0,       /* vtable[2]: T2.c */
      10, 0, 0, 0, /* vtable offset */
      0,           /* padding */
      77,          /* T2.c */
      66,          /* T2.b */
      55,          /* T2.c */

      12, 0, 0, 0, /* root object offset */
      8,  0,       /* vtable size */
      8,  0,       /* object size */
      7,  0,       /* vtable[0]: T1.a */
      6,  0,       /* vtable[1]: T1.b */
      8,  0, 0, 0, /* vtable offset */
      0,  0,       /* padding */
      44,          /* T1.b */
      33,          /* T1.a */
  };
  test_bytes_equal(expected, 44, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Table of bools.
   *
   * table T {
   *   a: bool; b: bool; c: bool; d: bool;
   *   e: bool; f: bool; g: bool; h: bool;
   * };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 8);
  nr_flatbuffers_object_prepend_bool(fb, 0, 1, 0); /* T.a */
  nr_flatbuffers_object_prepend_bool(fb, 1, 1, 0); /* T.b */
  nr_flatbuffers_object_prepend_bool(fb, 2, 1, 0); /* T.c */
  nr_flatbuffers_object_prepend_bool(fb, 3, 1, 0); /* T.d */
  nr_flatbuffers_object_prepend_bool(fb, 4, 1, 0); /* T.e */
  nr_flatbuffers_object_prepend_bool(fb, 5, 1, 0); /* T.f */
  nr_flatbuffers_object_prepend_bool(fb, 6, 1, 0); /* T.g */
  nr_flatbuffers_object_prepend_bool(fb, 7, 1, 0); /* T.h */
  object_end = nr_flatbuffers_object_end(fb);
  nr_flatbuffers_finish(fb, object_end);

  expected = (const uint8_t[]){
      24, 0, 0, 0, /* root object offset */
      20, 0,       /* vtable size */
      12, 0,       /* object size */
      11, 0,       /* vtable[0]: offset of T.a */
      10, 0,       /* vtable[0]: offset of T.b */
      9,  0,       /* vtable[0]: offset of T.c */
      8,  0,       /* vtable[0]: offset of T.d */
      7,  0,       /* vtable[0]: offset of T.e */
      6,  0,       /* vtable[0]: offset of T.f */
      5,  0,       /* vtable[0]: offset of T.g */
      4,  0,       /* vtable[0]: offset of T.h */
      20, 0, 0, 0, /* offset of vtable */
      1,           /* T.h */
      1,           /* T.g */
      1,           /* T.f */
      1,           /* T.e */
      1,           /* T.d */
      1,           /* T.c */
      1,           /* T.b */
      1,           /* T.a */
  };
  test_bytes_equal(expected, 36, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Object with an odd number of bools.
   *
   * table T { a: bool; b: bool; c: bool; };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 3);
  nr_flatbuffers_object_prepend_bool(fb, 0, 1, 0); /* T.a */
  nr_flatbuffers_object_prepend_bool(fb, 1, 1, 0); /* T.b */
  nr_flatbuffers_object_prepend_bool(fb, 2, 1, 0); /* T.c */
  object_end = nr_flatbuffers_object_end(fb);
  nr_flatbuffers_finish(fb, object_end);

  expected = (const uint8_t[]){
      16, 0, 0, 0, /* offset of root object */
      0,  0,       /* padding */
      10, 0,       /* vtable size */
      8,  0,       /* object size */
      7,  0,       /* vtable[0]: offset of T.a */
      6,  0,       /* vtable[1]: offset of T.b */
      5,  0,       /* vtable[2]: offset of T.c */
      10, 0, 0, 0, /* vtable offset */
      0,           /* padding */
      1,           /* T.c */
      1,           /* T.b */
      1,           /* T.a */
  };
  test_bytes_equal(expected, 24, fb);

  nr_flatbuffers_destroy(&fb);

  /*
   * Object with a float field.
   *
   * table T { a: float; };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_f32(fb, 0, 1.0, 0.0);
  nr_flatbuffers_object_end(fb);

  expected = (const uint8_t[]){
      6, 0,          /* vtable size */
      8, 0,          /* object size */
      4, 0,          /* vtable[0]: offset of T.a */
      6, 0, 0,   0,  /* vtable offset */
      0, 0, 128, 63, /* T.a */
  };
  test_bytes_equal(expected, 14, fb);

  nr_flatbuffers_destroy(&fb);
}

static void test_vtable_deduplication(void) {
  nr_flatbuffer_t* fb;
  nr_flatbuffers_table_t tbl;
  uint32_t obj0, obj1, obj2;
  uint8_t expected[] = {
      /* obj2 */
      240,
      255,
      255,
      255, /* == -12. offset to dedupped vtable. */
      99,
      0,
      88,
      77,
      /* obj1 */
      248,
      255,
      255,
      255, /* == -8. offset to dedupped vtable. */
      66,
      0,
      55,
      44,
      /* obj0 */
      12,
      0,
      8,
      0,
      0,
      0,
      7,
      0,
      6,
      0,
      4,
      0,
      12,
      0,
      0,
      0,
      33,
      0,
      22,
      11,
  };

  fb = nr_flatbuffers_create(0);

  nr_flatbuffers_object_begin(fb, 4);
  nr_flatbuffers_object_prepend_u8(fb, 0, 0, 0);
  nr_flatbuffers_object_prepend_u8(fb, 1, 11, 0);
  nr_flatbuffers_object_prepend_u8(fb, 2, 22, 0);
  nr_flatbuffers_object_prepend_i16(fb, 3, 33, 0);
  obj0 = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_object_begin(fb, 4);
  nr_flatbuffers_object_prepend_u8(fb, 0, 0, 0);
  nr_flatbuffers_object_prepend_u8(fb, 1, 44, 0);
  nr_flatbuffers_object_prepend_u8(fb, 2, 55, 0);
  nr_flatbuffers_object_prepend_i16(fb, 3, 66, 0);
  obj1 = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_object_begin(fb, 4);
  nr_flatbuffers_object_prepend_u8(fb, 0, 0, 0);
  nr_flatbuffers_object_prepend_u8(fb, 1, 77, 0);
  nr_flatbuffers_object_prepend_u8(fb, 2, 88, 0);
  nr_flatbuffers_object_prepend_i16(fb, 3, 99, 0);
  obj2 = nr_flatbuffers_object_end(fb);

  test_bytes_equal(expected, sizeof(expected), fb);

  nr_flatbuffers_table_init(&tbl, nr_flatbuffers_data(fb),
                            nr_flatbuffers_len(fb),
                            nr_flatbuffers_len(fb) - obj0);
  tlib_pass_if_size_t_equal(__func__, 12, tbl.vsize);
  tlib_pass_if_uint8_t_equal(__func__, 11,
                             nr_flatbuffers_table_read_u8(&tbl, 1, 0));
  tlib_pass_if_uint8_t_equal(__func__, 22,
                             nr_flatbuffers_table_read_u8(&tbl, 2, 0));
  tlib_pass_if_uint8_t_equal(__func__, 33,
                             nr_flatbuffers_table_read_u8(&tbl, 3, 0));

  nr_flatbuffers_table_init(&tbl, nr_flatbuffers_data(fb),
                            nr_flatbuffers_len(fb),
                            nr_flatbuffers_len(fb) - obj1);
  tlib_pass_if_size_t_equal(__func__, 12, tbl.vsize);
  tlib_pass_if_uint8_t_equal(__func__, 44,
                             nr_flatbuffers_table_read_u8(&tbl, 1, 0));
  tlib_pass_if_uint8_t_equal(__func__, 55,
                             nr_flatbuffers_table_read_u8(&tbl, 2, 0));
  tlib_pass_if_uint8_t_equal(__func__, 66,
                             nr_flatbuffers_table_read_u8(&tbl, 3, 0));

  nr_flatbuffers_table_init(&tbl, nr_flatbuffers_data(fb),
                            nr_flatbuffers_len(fb),
                            nr_flatbuffers_len(fb) - obj2);
  tlib_pass_if_size_t_equal(__func__, 12, tbl.vsize);
  tlib_pass_if_uint8_t_equal(__func__, 77,
                             nr_flatbuffers_table_read_u8(&tbl, 1, 0));
  tlib_pass_if_uint8_t_equal(__func__, 88,
                             nr_flatbuffers_table_read_u8(&tbl, 2, 0));
  tlib_pass_if_uint8_t_equal(__func__, 99,
                             nr_flatbuffers_table_read_u8(&tbl, 3, 0));

  nr_flatbuffers_destroy(&fb);
}

static void test_prepend_bytes(void) {
  int i;
  uint8_t expected[30];
  size_t expected_len = sizeof(expected) / sizeof(expected[0]);
  nr_flatbuffer_t* a;
  nr_flatbuffer_t* b;

  /*
   * The following sequences should be equivalent.
   *
   *   1. nr_flatbuffers_prepend_bytes (fb, array, N);
   *
   *   2. nr_flatbuffers_vector_begin (fb, sizeof (uint8_t), N 1);
   *      nr_flatbuffers_prepend_u8 (fb, array[N-1]);
   *      ...
   *      nr_flatbuffers_prepend_u8 (fb, array[0]);
   *      nr_flatbuffers_vector_end (fb, N);
   */

  for (i = 0; i < (int)expected_len; i++) {
    expected[i] = (uint8_t)i;
  }

  /* 1. */
  a = nr_flatbuffers_create(0);
  nr_flatbuffers_vector_begin(a, sizeof(uint8_t), expected_len, 1);
  for (i = (int)expected_len - 1; i >= 0; i--) {
    nr_flatbuffers_prepend_u8(a, expected[i]);
  }
  nr_flatbuffers_vector_end(a, expected_len);

  /* 2. */
  b = nr_flatbuffers_create(0);
  nr_flatbuffers_prepend_bytes(b, expected, expected_len);

  tlib_pass_if_bytes_equal(__func__, nr_flatbuffers_data(a),
                           nr_flatbuffers_len(a), nr_flatbuffers_data(b),
                           nr_flatbuffers_len(b));

  nr_flatbuffers_destroy(&a);
  nr_flatbuffers_destroy(&b);
}

/*
 * These values were specially chosen for fuzz testing and were
 * taken verbatim from FuzzTest1 in the Flatbuffers C++ test suite.
 */
#define OVERFLOWING_INT32 0x33333383L
#define OVERFLOWING_INT64 0x4444444444444484LL

static void fuzz_encode_decode(uint64_t seed, int ntables, int nfields) {
  nr_flatbuffer_t* fb;
  nr_random_t* rng;
  uint32_t* offsets;

  /*
   * The values to test against. Chosen to ensure no bits get
   * truncated anywhere, but also to be uniquely identifiable.
   */
  int bool_val = 1;
  int8_t i8_val = -127; /* 0x81 */
  uint8_t u8_val = 0xFF;
  int16_t i16_val = -32222; /* 0x8222 */
  uint16_t u16_val = 0xFEEE;
  int32_t i32_val = OVERFLOWING_INT32;
  uint32_t u32_val = 0xFDDDDDDDL;
  int64_t i64_val = OVERFLOWING_INT64;
  uint64_t u64_val = 0xFCCCCCCCCCCCCCCCLL;
  float f32_val = 3.14159;
  double f64_val = 3.14159265359;

  /*
   * First, generate a random sequence of tables and containing our test values.
   */

  rng = nr_random_create_from_seed(seed);
  offsets = (uint32_t*)nr_calloc(ntables, sizeof(uint32_t));
  fb = nr_flatbuffers_create(0);

  for (int i = 0; i < ntables; i++) {
    nr_flatbuffers_object_begin(fb, nfields);

    for (int j = 0; j < nfields; j++) {
      unsigned long data_type = nr_random_range(rng, 11);

      switch (data_type) {
        case 0:
          nr_flatbuffers_object_prepend_bool(fb, j, bool_val, 0);
          break;
        case 1:
          nr_flatbuffers_object_prepend_i8(fb, j, i8_val, 0);
          break;
        case 2:
          nr_flatbuffers_object_prepend_u8(fb, j, u8_val, 0);
          break;
        case 3:
          nr_flatbuffers_object_prepend_i16(fb, j, i16_val, 0);
          break;
        case 4:
          nr_flatbuffers_object_prepend_u16(fb, j, u16_val, 0);
          break;
        case 5:
          nr_flatbuffers_object_prepend_i32(fb, j, i32_val, 0);
          break;
        case 6:
          nr_flatbuffers_object_prepend_u32(fb, j, u32_val, 0);
          break;
        case 7:
          nr_flatbuffers_object_prepend_i64(fb, j, i64_val, 0);
          break;
        case 8:
          nr_flatbuffers_object_prepend_u64(fb, j, u64_val, 0);
          break;
        case 9:
          nr_flatbuffers_object_prepend_f32(fb, j, f32_val, 0);
          break;
        case 10:
          nr_flatbuffers_object_prepend_f64(fb, j, f64_val, 0);
          break;
      }
    }

    offsets[i] = nr_flatbuffers_object_end(fb);
  }

  /*
   * Now read back the buffer and verify we read the same values. Reseed
   * the random number generator so we can replay the same sequence of
   * random choices.
   */

  nr_random_seed(rng, seed);

  for (int i = 0; i < ntables; i++) {
    nr_flatbuffers_table_t tbl;

    nr_flatbuffers_table_init(&tbl, nr_flatbuffers_data(fb),
                              nr_flatbuffers_len(fb),
                              nr_flatbuffers_len(fb) - offsets[i]);

    for (int j = 0; j < nfields; j++) {
      int did_fail = 1;
      unsigned long data_type = nr_random_range(rng, 11);

      switch (data_type) {
        case 0: {
          int actual = nr_flatbuffers_table_read_bool(&tbl, j, 0);
          did_fail = tlib_pass_if_true(__func__, bool_val == actual,
                                       "i=%d j=%d expected=%d actual=%d", i, j,
                                       bool_val, actual);
        } break;
        case 1: {
          int8_t actual = nr_flatbuffers_table_read_i8(&tbl, j, 0);
          did_fail = tlib_pass_if_true(__func__, i8_val == actual,
                                       "i=%d j=%d expected=%" PRId8
                                       " actual=%" PRId8,
                                       i, j, i8_val, actual);
        } break;
        case 2: {
          uint8_t actual = nr_flatbuffers_table_read_u8(&tbl, j, 0);
          did_fail = tlib_pass_if_true(__func__, u8_val == actual,
                                       "i=%d j=%d expected=%" PRIu8
                                       " actual=%" PRIu8,
                                       i, j, u8_val, actual);
        } break;
        case 3: {
          int16_t actual = nr_flatbuffers_table_read_i16(&tbl, j, 0);
          did_fail = tlib_pass_if_true(__func__, i16_val == actual,
                                       "i=%d j=%d expected=%" PRId16
                                       " actual=%" PRId16,
                                       i, j, i16_val, actual);
        } break;
        case 4: {
          uint16_t actual = nr_flatbuffers_table_read_u16(&tbl, j, 0);
          did_fail = tlib_pass_if_true(__func__, u16_val == actual,
                                       "i=%d j=%d expected=%" PRIu16
                                       " actual=%" PRIu16,
                                       i, j, u16_val, actual);
        } break;
        case 5: {
          int32_t actual = nr_flatbuffers_table_read_i32(&tbl, j, 0);
          did_fail = tlib_pass_if_true(__func__, i32_val == actual,
                                       "i=%d j=%d expected=%" PRId32
                                       " actual=%" PRId32,
                                       i, j, i32_val, actual);
        } break;
        case 6: {
          uint32_t actual = nr_flatbuffers_table_read_u32(&tbl, j, 0);
          did_fail = tlib_pass_if_true(__func__, u32_val == actual,
                                       "i=%d j=%d expected=%#" PRIx32
                                       " actual=%#" PRIx32,
                                       i, j, u32_val, actual);
        } break;
        case 7: {
          int64_t actual = nr_flatbuffers_table_read_i64(&tbl, j, 0);
          did_fail = tlib_pass_if_true(__func__, i64_val == actual,
                                       "i=%d j=%d expected=%" PRIx64
                                       " actual=%" PRIx64,
                                       i, j, i64_val, actual);
        } break;
        case 8: {
          uint64_t actual = nr_flatbuffers_table_read_u64(&tbl, j, 0);
          did_fail = tlib_pass_if_true(__func__, u64_val == actual,
                                       "i=%d j=%d expected=%" PRIx64
                                       " actual=%" PRIx64,
                                       i, j, u64_val, actual);
        } break;
        case 9: {
          float actual = nr_flatbuffers_table_read_f32(&tbl, j, 0);
          did_fail = tlib_pass_if_true(__func__, f32_val == actual,
                                       "i=%d j=%d expected=%f actual=%f", i, j,
                                       f32_val, actual);
        } break;
        case 10: {
          double actual = nr_flatbuffers_table_read_f64(&tbl, j, 0);
          did_fail = tlib_pass_if_true(__func__, f64_val == actual,
                                       "i=%d j=%d expected=%f actual=%f", i, j,
                                       f64_val, actual);
        } break;
      }

      if (did_fail) {
        goto done;
      }
    }
  }

done:
  nr_free(offsets);
  nr_random_destroy(&rng);
  nr_flatbuffers_destroy(&fb);
}

static void test_read_indirect(void) {
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;
  nr_aoffset_t pos;
  uint32_t child_offset;
  uint32_t vector_offset;
  uint32_t parent_offset;

  /*
   * Construct a flatbuffer with the following schema and read it back.
   *
   * table Child { field: uint; };
   * table Parent { vector: [Child]; };
   */

  fb = nr_flatbuffers_create(0);

  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_u32(fb, 0, 0x12345678, 0);
  child_offset = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_vector_begin(fb, sizeof(uint32_t), 1, sizeof(uint32_t));
  nr_flatbuffers_prepend_uoffset(fb, child_offset);
  vector_offset = nr_flatbuffers_vector_end(fb, 1);

  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_uoffset(fb, 0, vector_offset, 0);
  parent_offset = nr_flatbuffers_object_end(fb);
  nr_flatbuffers_finish(fb, parent_offset);

  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  pos = nr_flatbuffers_table_read_vector(&tbl, 0);
  tlib_fail_if_uint32_t_equal(__func__, 0, pos.offset);
  tlib_pass_if_uint32_t_equal(__func__, 1,
                              nr_flatbuffers_table_read_vector_len(&tbl, 0));

  nr_flatbuffers_table_init(&tbl, tbl.data, tbl.length,
                            nr_flatbuffers_read_indirect(tbl.data, pos).offset);
  tlib_pass_if_uint32_t_equal(__func__, 0x12345678,
                              nr_flatbuffers_table_read_u32(&tbl, 0, 0));

  nr_flatbuffers_destroy(&fb);
}

static void test_read_struct(void) {
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;
  nr_aoffset_t pos;
  uint32_t offset;

  /*
   * Test reading and writing schemas like the following.
   *
   * struct S {
   *   a: int;
   *   b: int;
   * };
   *
   * table T {
   *   c: int;
   *   d: S;
   * };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 2);
  nr_flatbuffers_prep(fb, 2 * sizeof(int32_t), 0);
  nr_flatbuffers_prepend_i32(fb, 3); /* S.b */
  nr_flatbuffers_prepend_i32(fb, 2); /* S.a */
  nr_flatbuffers_object_prepend_struct(fb, 1, nr_flatbuffers_len(fb),
                                       0);        /* T.d */
  nr_flatbuffers_object_prepend_i32(fb, 0, 1, 0); /* T.c */
  offset = nr_flatbuffers_object_end(fb);
  nr_flatbuffers_finish(fb, offset);

  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  tlib_pass_if_int32_t_equal(
      __func__, 1, nr_flatbuffers_table_read_i32(&tbl, 0, 0)); /* T.c */

  pos = nr_flatbuffers_table_lookup(&tbl, 1);
  tlib_pass_if_int32_t_equal(__func__, 2,
                             nr_flatbuffers_read_i32(tbl.data, pos.offset + 0));
  tlib_pass_if_int32_t_equal(__func__, 3,
                             nr_flatbuffers_read_i32(tbl.data, pos.offset + 4));

  nr_flatbuffers_destroy(&fb);
}

static void test_read_union(void) {
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;
  uint32_t offset;

  /*
   * Test read and writing schemas like the following.
   *
   * table A { a: int; };
   * table B { b: float; };
   * union U { A, B };
   * table R { u: U; };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_i32(fb, 0, 42, 0); /* A.a */
  offset = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_object_begin(fb, 2);
  nr_flatbuffers_object_prepend_uoffset(fb, 1, offset, 0); /* R.u */
  nr_flatbuffers_object_prepend_i8(fb, 0, 1, 0); /* R.u discriminator */
  offset = nr_flatbuffers_object_end(fb);
  nr_flatbuffers_finish(fb, offset);

  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  tlib_pass_if_int8_t_equal(__func__, 1,
                            nr_flatbuffers_table_read_i8(&tbl, 0, 0));
  tlib_pass_if_int_equal(__func__, 1,
                         nr_flatbuffers_table_read_union(&tbl, &tbl, 1));
  tlib_pass_if_int32_t_equal(__func__, 42,
                             nr_flatbuffers_table_read_i32(&tbl, 0, 0));

  nr_flatbuffers_destroy(&fb);
}

static void test_read_missing_union(void) {
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;
  uint32_t offset;

  /*
   * Test read and writing schemas like the following when the union
   * field is not present.
   *
   * table A { a: int; };
   * table B { b: float; };
   * union U { A, B };
   * table R { u: U; };
   */

  fb = nr_flatbuffers_create(0);

  nr_flatbuffers_object_begin(fb, 2);
  nr_flatbuffers_object_prepend_uoffset(fb, 1, 0, 0); /* R.u */
  nr_flatbuffers_object_prepend_i8(fb, 0, 1, 0);      /* R.u discriminator */
  offset = nr_flatbuffers_object_end(fb);
  nr_flatbuffers_finish(fb, offset);

  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  tlib_pass_if_int8_t_equal(__func__, 1,
                            nr_flatbuffers_table_read_i8(&tbl, 0, 0));
  tlib_pass_if_int_equal(__func__, 0,
                         nr_flatbuffers_table_read_union(&tbl, &tbl, 1));

  nr_flatbuffers_destroy(&fb);
}

static void test_read_bytes(void) {
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;
  uint32_t offset;

  /*
   * Test reading and writing schema like the following.
   *
   * table T { v: [ubyte] };
   */

  fb = nr_flatbuffers_create(0);
  offset = nr_flatbuffers_prepend_bytes(fb, "Hello, World!", 13);
  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_uoffset(fb, 0, offset, 0);
  nr_flatbuffers_finish(fb, nr_flatbuffers_object_end(fb));

  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  tlib_pass_if_bytes_equal(__func__, "Hello, World!", 13,
                           nr_flatbuffers_table_read_bytes(&tbl, 0),
                           nr_flatbuffers_table_read_vector_len(&tbl, 0));

  nr_flatbuffers_destroy(&fb);
}

static void test_read_missing_vector(void) {
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;

  /*
   * Test reading and writing schema like the following when the vector
   * is not present.
   *
   * table T { v: [ubyte] };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_uoffset(fb, 0, 0, 0);
  nr_flatbuffers_finish(fb, nr_flatbuffers_object_end(fb));

  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  tlib_pass_if_null(__func__, nr_flatbuffers_table_read_bytes(&tbl, 0));

  nr_flatbuffers_destroy(&fb);
}

static void test_read_empty_vector(void) {
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;
  uint32_t offset;

  /*
   * Test reading and writing schema like the following when the vector
   * is not present.
   *
   * table T { v: [ubyte] };
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_vector_begin(fb, 1, 0, 1);
  offset = nr_flatbuffers_vector_end(fb, 0);

  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_uoffset(fb, 0, offset, 0);
  nr_flatbuffers_finish(fb, nr_flatbuffers_object_end(fb));

  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  tlib_pass_if_null(__func__, nr_flatbuffers_table_read_bytes(&tbl, 0));

  nr_flatbuffers_destroy(&fb);
}

static void test_read_string(void) {
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;
  uint32_t offset;

  /*
   * Test reading and writing schema like the following.
   *
   * table T { s: string };
   */

  fb = nr_flatbuffers_create(0);
  offset = nr_flatbuffers_prepend_string(fb, "Hello, World!");
  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_uoffset(fb, 0, offset, 0);
  nr_flatbuffers_finish(fb, nr_flatbuffers_object_end(fb));

  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  tlib_pass_if_str_equal(__func__, "Hello, World!",
                         nr_flatbuffers_table_read_str(&tbl, 0));

  nr_flatbuffers_destroy(&fb);
}

static void test_lookup_unknown_field(void) {
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;

  /*
   * Test reading a field whose index is past the end of the vtable as
   * would happen when an old client receives a buffer containing a new
   * field.
   */

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, 1);
  nr_flatbuffers_object_prepend_i32(fb, 0, 42, 0);
  nr_flatbuffers_finish(fb, nr_flatbuffers_object_end(fb));

  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  tlib_pass_if_int32_t_equal(__func__, 0,
                             nr_flatbuffers_table_read_i32(&tbl, 1, 0));

  nr_flatbuffers_destroy(&fb);
}

static void test_minimum_flatbuffer_size(void) {
  nr_flatbuffer_t* fb;
  size_t min_len;

  fb = nr_flatbuffers_create(0);
  nr_flatbuffers_object_begin(fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_finish(fb, nr_flatbuffers_object_end(fb));

  min_len = nr_flatbuffers_len(fb);

  tlib_pass_if_size_t_equal("expected minimum flatbuffer size",
                            MIN_FLATBUFFER_SIZE, min_len);

  nr_flatbuffers_destroy(&fb);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  int fuzz_num_tables;
  int fuzz_fields_per_table;
  uint64_t fuzz_seed;

  test_bad_input();
  test_read_uoffset();
  test_byte_layout_numbers();
  test_byte_layout_vectors();
  test_byte_layout_strings();
  test_byte_layout_utf8();
  test_byte_layout_vtables();
  test_vtable_deduplication();
  test_prepend_bytes();
  test_read_indirect();
  test_read_struct();
  test_read_union();
  test_read_missing_union();
  test_read_missing_vector();
  test_read_empty_vector();
  test_read_bytes();
  test_read_string();
  test_lookup_unknown_field();
  test_minimum_flatbuffer_size();

  /*
   * These values control the fuzz test and were taken verbatim from
   * the Flatbuffers project. Increasing the number of tables increases
   * the thoroughness of the test.
   */
  fuzz_seed = 48271;
  fuzz_num_tables = 10000;
  fuzz_fields_per_table = 4;
  fuzz_encode_decode(fuzz_seed, fuzz_num_tables, fuzz_fields_per_table);
}
