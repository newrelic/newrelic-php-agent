/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <assert.h>
#include <stddef.h>

#include "util_flatbuffers.h"
#include "util_memory.h"
#include "util_strings.h"

#define nr_flatbuffers_assert(X) (assert(X))

struct _nr_voffset_t {
  uint16_t offset;
};

struct _nr_flatbuffer_t {
  /*
   * Flatbuffer contents. This buffer is populated downward
   * (i.e. back-to-front) because flatbuffer objects are constructed
   * starting from the leaves. Fully constructed, `pos` is the position
   * of the root object. During construction, it is the position of the
   * most recently written object.
   *
   * bytes_allocated = back - front
   * bytes_used      = back - pos
   * bytes_remaining = pos - front
   */
  uint8_t* front;
  uint8_t* pos;
  uint8_t* back;

  size_t min_align;
  int inside_object;
  size_t object_end;

  /*
   * Vtable for the current object. Each element in `vtable` represents
   * the position of a field relative to the start of an object. The use
   * of relative rather than absolute positions allows vtables to be
   * deduplicated. The mapping of fields to elements in `vtable` is
   * determined by the Flatbuffers compiler when an IDL is processed.
   */
  uint32_t* vtable;
  int vtable_len; /* vtable actual number of elements */
  int vtable_cap; /* vtable maximum number of elements */

  /*
   * Keep an array of vtable positions for deduplication purposes. Before
   * writing a vtable to `bytes`, this array is searched. If an equivalent
   * vtable has already been written, it is re-used.
   */
  uint32_t* vtables;
  int vtables_len;
  int vtables_cap;
};

/* Number of metadata fields in each vtable. */
#define VTABLE_METADATA_FIELDS 2

nr_flatbuffer_t* nr_flatbuffers_create(size_t initial_size) {
  nr_flatbuffer_t* fb;

  fb = (nr_flatbuffer_t*)nr_zalloc(sizeof(*fb));
  fb->min_align = 1;

  if (initial_size > 0) {
    fb->front = (uint8_t*)nr_zalloc(initial_size);
    fb->back = fb->front + initial_size;
    fb->pos = fb->back;
  }

  fb->vtables_len = 0;
  fb->vtables_cap = 16;
  fb->vtables = (uint32_t*)nr_calloc(fb->vtables_cap, sizeof(uint32_t));
  return fb;
}

const uint8_t* nr_flatbuffers_data(const nr_flatbuffer_t* fb) {
  if (fb) {
    return fb->pos;
  }
  return NULL;
}

size_t nr_flatbuffers_len(const nr_flatbuffer_t* fb) {
  if (fb) {
    return (size_t)(fb->back - fb->pos);
  }
  return 0;
}

void nr_flatbuffers_destroy(nr_flatbuffer_t** fb_ptr) {
  nr_flatbuffer_t* fb;

  if ((NULL == fb_ptr) || (NULL == *fb_ptr)) {
    return;
  }

  fb = *fb_ptr;
  fb->pos = NULL;
  fb->back = NULL;
  nr_free(fb->front);
  nr_free(fb->vtable);
  nr_free(fb->vtables);
  nr_realfree((void**)fb_ptr);
}

int8_t nr_flatbuffers_read_i8(const uint8_t* buf, size_t offset) {
  return (int8_t)buf[offset];
}

int16_t nr_flatbuffers_read_i16(const uint8_t* buf, size_t offset) {
  int16_t x = 0;

  x |= (int16_t)buf[offset];
  x |= (int16_t)buf[offset + 1] << 8;
  return x;
}

int32_t nr_flatbuffers_read_i32(const uint8_t* buf, size_t offset) {
  int32_t x = 0;

  x |= (int32_t)buf[offset];
  x |= (int32_t)buf[offset + 1] << 8;
  x |= (int32_t)buf[offset + 2] << 16;
  x |= (int32_t)buf[offset + 3] << 24;
  return x;
}

int64_t nr_flatbuffers_read_i64(const uint8_t* buf, size_t offset) {
  int64_t x = 0;

  x |= (int64_t)buf[offset];
  x |= (int64_t)buf[offset + 1] << 8;
  x |= (int64_t)buf[offset + 2] << 16;
  x |= (int64_t)buf[offset + 3] << 24;
  x |= (int64_t)buf[offset + 4] << 32;
  x |= (int64_t)buf[offset + 5] << 40;
  x |= (int64_t)buf[offset + 6] << 48;
  x |= (int64_t)buf[offset + 7] << 56;
  return x;
}

uint8_t nr_flatbuffers_read_u8(const uint8_t* buf, size_t offset) {
  return buf[offset];
}

uint16_t nr_flatbuffers_read_u16(const uint8_t* buf, size_t offset) {
  uint16_t x = 0;

  x |= (uint16_t)buf[offset];
  x |= (uint16_t)buf[offset + 1] << 8;
  return x;
}

uint32_t nr_flatbuffers_read_u32(const uint8_t* buf, size_t offset) {
  uint32_t x = 0;

  x |= (uint32_t)buf[offset];
  x |= (uint32_t)buf[offset + 1] << 8;
  x |= (uint32_t)buf[offset + 2] << 16;
  x |= (uint32_t)buf[offset + 3] << 24;
  return x;
}

uint64_t nr_flatbuffers_read_u64(const uint8_t* buf, size_t offset) {
  uint64_t x = 0;

  x |= (uint64_t)buf[offset];
  x |= (uint64_t)buf[offset + 1] << 8;
  x |= (uint64_t)buf[offset + 2] << 16;
  x |= (uint64_t)buf[offset + 3] << 24;
  x |= (uint64_t)buf[offset + 4] << 32;
  x |= (uint64_t)buf[offset + 5] << 40;
  x |= (uint64_t)buf[offset + 6] << 48;
  x |= (uint64_t)buf[offset + 7] << 56;
  return x;
}

float nr_flatbuffers_read_f32(const uint8_t* buf, size_t offset) {
  uint32_t x;
  float y;

  x = 0;
  x |= (uint32_t)buf[offset];
  x |= (uint32_t)buf[offset + 1] << 8;
  x |= (uint32_t)buf[offset + 2] << 16;
  x |= (uint32_t)buf[offset + 3] << 24;

  /* avoid strict aliasing violations */
  nr_memcpy(&y, &x, sizeof(float));

  return y;
}

double nr_flatbuffers_read_f64(const uint8_t* buf, size_t offset) {
  uint64_t x;
  double y;

  x = 0;
  x |= (uint64_t)buf[offset];
  x |= (uint64_t)buf[offset + 1] << 8;
  x |= (uint64_t)buf[offset + 2] << 16;
  x |= (uint64_t)buf[offset + 3] << 24;
  x |= (uint64_t)buf[offset + 4] << 32;
  x |= (uint64_t)buf[offset + 5] << 40;
  x |= (uint64_t)buf[offset + 6] << 48;
  x |= (uint64_t)buf[offset + 7] << 56;

  /* avoid strict aliasing violations */
  nr_memcpy(&y, &x, sizeof(double));

  return y;
}

int32_t nr_flatbuffers_read_soffset(const uint8_t* buf, size_t offset) {
  int32_t x = 0;

  x |= (int32_t)buf[offset];
  x |= (int32_t)buf[offset + 1] << 8;
  x |= (int32_t)buf[offset + 2] << 16;
  x |= (int32_t)buf[offset + 3] << 24;

  return x;
}

uint32_t nr_flatbuffers_read_uoffset(const uint8_t* buf, size_t offset) {
  uint32_t x = 0;

  x |= (uint32_t)buf[offset];
  x |= (uint32_t)buf[offset + 1] << 8;
  x |= (uint32_t)buf[offset + 2] << 16;
  x |= (uint32_t)buf[offset + 3] << 24;

  return x;
}

nr_voffset_t nr_flatbuffers_read_voffset(const uint8_t* buf, size_t offset) {
  nr_voffset_t voffset = {0};

  voffset.offset |= (uint16_t)buf[offset];
  voffset.offset |= (uint16_t)buf[offset + 1] << 8;

  return voffset;
}

nr_aoffset_t nr_flatbuffers_read_indirect(const uint8_t* buf,
                                          nr_aoffset_t pos) {
  nr_aoffset_t aoffset = {0};

  aoffset.offset |= (uint32_t)buf[pos.offset];
  aoffset.offset |= (uint32_t)buf[pos.offset + 1] << 8;
  aoffset.offset |= (uint32_t)buf[pos.offset + 2] << 16;
  aoffset.offset |= (uint32_t)buf[pos.offset + 3] << 24;
  aoffset.offset += pos.offset;

  return aoffset;
}

void nr_flatbuffers_pad(nr_flatbuffer_t* fb, size_t n) {
  fb->pos -= n;
  nr_memset(fb->pos, 0, n);
}

static void nr_flatbuffers_grow(nr_flatbuffer_t* fb) {
  size_t used;
  size_t old_size;
  size_t new_size;
  uint8_t* new_front;
  uint8_t* new_pos;

  old_size = (size_t)(fb->back - fb->front);
  /* Cannot grow buffer beyond 2 gigabytes. */
  nr_flatbuffers_assert(0 == (old_size & (size_t)0xC0000000));

  new_size = old_size * 2;
  if (0 == new_size) {
    new_size = 1;
  }

  /*
   * Note: flatbuffers are built back-to-front; i.e. additional space is
   * prepended to the buffer rather than appended.
   */
  used = nr_flatbuffers_len(fb);
  new_front = (uint8_t*)nr_malloc(new_size * sizeof(uint8_t));
  new_pos = new_front + (new_size - used);
  nr_memset(new_front, 0, new_size - used);
  nr_memcpy(new_pos, fb->pos, used);

  nr_free(fb->front);
  fb->front = new_front;
  fb->back = fb->front + new_size;
  fb->pos = fb->back - used;
}

void nr_flatbuffers_prep(nr_flatbuffer_t* fb,
                         size_t size,
                         size_t additional_bytes) {
  size_t pad_size;

  /* Track the biggest thing we've ever aligned to. */
  if (size > fb->min_align) {
    fb->min_align = size;
  }

  /* Note that the padding must be less than 'size'. */
  while ((size_t)(fb->pos - fb->front) <= (2 * size) + additional_bytes) {
    nr_flatbuffers_grow(fb);
  }

  /*
   * Find the amount of alignment needed such that `size` is properly
   * aligned after `additional_bytes`:
   */
  pad_size = (size_t)(fb->pos - additional_bytes - size) % size;

  nr_flatbuffers_pad(fb, pad_size);
}

/* Prepends a uint8 to the buffer, without checking for space. */
static void nr_flatbuffers_put_u8(nr_flatbuffer_t* fb, uint8_t x) {
  fb->pos -= sizeof(uint8_t);
  *fb->pos = x;
}

/* Prepends a uint16 to the buffer, without checking for space. */
static void nr_flatbuffers_put_u16(nr_flatbuffer_t* fb, uint16_t x) {
  fb->pos -= sizeof(uint16_t);
  fb->pos[0] = (uint8_t)x;
  fb->pos[1] = (uint8_t)(x >> 8);
}

/* Prepends a uint32 to the buffer, without checking for space. */
static void nr_flatbuffers_put_u32(nr_flatbuffer_t* fb, uint32_t x) {
  fb->pos -= sizeof(uint32_t);
  fb->pos[0] = (uint8_t)x;
  fb->pos[1] = (uint8_t)(x >> 8);
  fb->pos[2] = (uint8_t)(x >> 16);
  fb->pos[3] = (uint8_t)(x >> 24);
}

/* Prepends a uint64 to the buffer, without checking for space. */
static void nr_flatbuffers_put_u64(nr_flatbuffer_t* fb, uint64_t x) {
  fb->pos -= sizeof(uint64_t);
  fb->pos[0] = (uint8_t)x;
  fb->pos[1] = (uint8_t)(x >> 8);
  fb->pos[2] = (uint8_t)(x >> 16);
  fb->pos[3] = (uint8_t)(x >> 24);
  fb->pos[4] = (uint8_t)(x >> 32);
  fb->pos[5] = (uint8_t)(x >> 40);
  fb->pos[6] = (uint8_t)(x >> 48);
  fb->pos[7] = (uint8_t)(x >> 56);
}

/* Prepends a int8 to the buffer, without checking for space. */
static void nr_flatbuffers_put_i8(nr_flatbuffer_t* fb, int8_t x) {
  fb->pos -= sizeof(int8_t);
  *fb->pos = (uint8_t)x;
}

/* Prepends a int16 to the buffer, without checking for space. */
static void nr_flatbuffers_put_i16(nr_flatbuffer_t* fb, int16_t x) {
  fb->pos -= sizeof(int16_t);
  fb->pos[0] = (uint8_t)x;
  fb->pos[1] = (uint8_t)(x >> 8);
}

static void nr_flatbuffers_encode_i32(uint8_t* p, int32_t x) {
  p[0] = (uint8_t)x;
  p[1] = (uint8_t)(x >> 8);
  p[2] = (uint8_t)(x >> 16);
  p[3] = (uint8_t)(x >> 24);
}

/* Prepends a int32 to the buffer, without checking for space. */
static void nr_flatbuffers_put_i32(nr_flatbuffer_t* fb, int32_t x) {
  fb->pos -= sizeof(int32_t);
  fb->pos[0] = (uint8_t)x;
  fb->pos[1] = (uint8_t)(x >> 8);
  fb->pos[2] = (uint8_t)(x >> 16);
  fb->pos[3] = (uint8_t)(x >> 24);
}

/* Prepends a int64 to the buffer, without checking for space. */
static void nr_flatbuffers_put_i64(nr_flatbuffer_t* fb, int64_t x) {
  fb->pos -= sizeof(int64_t);
  fb->pos[0] = (uint8_t)x;
  fb->pos[1] = (uint8_t)(x >> 8);
  fb->pos[2] = (uint8_t)(x >> 16);
  fb->pos[3] = (uint8_t)(x >> 24);
  fb->pos[4] = (uint8_t)(x >> 32);
  fb->pos[5] = (uint8_t)(x >> 40);
  fb->pos[6] = (uint8_t)(x >> 48);
  fb->pos[7] = (uint8_t)(x >> 56);
}

/* Prepends a float to the buffer, without checking for space. */
static void nr_flatbuffers_put_f32(nr_flatbuffer_t* fb, float x) {
  uint32_t n;

  /* avoid strict aliasing violations */
  nr_memcpy(&n, &x, sizeof(uint32_t));

  fb->pos -= sizeof(n);
  fb->pos[0] = (uint8_t)n;
  fb->pos[1] = (uint8_t)(n >> 8);
  fb->pos[2] = (uint8_t)(n >> 16);
  fb->pos[3] = (uint8_t)(n >> 24);
}

/* Prepends a double to the buffer, without checking for space. */
static void nr_flatbuffers_put_f64(nr_flatbuffer_t* fb, double x) {
  uint64_t n;

  /* avoid strict aliasing violations */
  nr_memcpy(&n, &x, sizeof(uint64_t));

  fb->pos -= sizeof(n);
  fb->pos[0] = (uint8_t)n;
  fb->pos[1] = (uint8_t)(n >> 8);
  fb->pos[2] = (uint8_t)(n >> 16);
  fb->pos[3] = (uint8_t)(n >> 24);
  fb->pos[4] = (uint8_t)(n >> 32);
  fb->pos[5] = (uint8_t)(n >> 40);
  fb->pos[6] = (uint8_t)(n >> 48);
  fb->pos[7] = (uint8_t)(n >> 56);
}

#define NR_FLATBUFFERS_PREPEND(TYPE, SUFFIX)                          \
  void nr_flatbuffers_prepend_##SUFFIX(nr_flatbuffer_t* fb, TYPE x) { \
    nr_flatbuffers_prep(fb, sizeof(TYPE), 0);                         \
    nr_flatbuffers_put_##SUFFIX(fb, x);                               \
  }

NR_FLATBUFFERS_PREPEND(int8_t, i8)
NR_FLATBUFFERS_PREPEND(int16_t, i16)
NR_FLATBUFFERS_PREPEND(int32_t, i32)
NR_FLATBUFFERS_PREPEND(int64_t, i64)

NR_FLATBUFFERS_PREPEND(uint8_t, u8)
NR_FLATBUFFERS_PREPEND(uint16_t, u16)
NR_FLATBUFFERS_PREPEND(uint32_t, u32)
NR_FLATBUFFERS_PREPEND(uint64_t, u64)

NR_FLATBUFFERS_PREPEND(float, f32)
NR_FLATBUFFERS_PREPEND(double, f64)

#undef NR_FLATBUFFERS_PREPEND

void nr_flatbuffers_prepend_bool(nr_flatbuffer_t* fb, int x) {
  nr_flatbuffers_prep(fb, sizeof(uint8_t), 0);
  nr_flatbuffers_put_u8(fb, x ? 1 : 0);
}

void nr_flatbuffers_prepend_uoffset(nr_flatbuffer_t* fb, uint32_t offset) {
  uint32_t relative_offset;

  nr_flatbuffers_prep(fb, sizeof(uint32_t), 0); /* ensure proper alignment */
  nr_flatbuffers_assert(
      offset <= nr_flatbuffers_len(fb)); /* ensure offset is reachable */

  /*
   * Convert offset from a value relative to the end of the buffer,
   * to a value relative to the current position. The new offset
   * includes the four bytes used to write the value.
   */
  relative_offset = nr_flatbuffers_len(fb) - offset + sizeof(uint32_t);
  nr_flatbuffers_put_u32(fb, relative_offset);
}

uint32_t nr_flatbuffers_prepend_string(nr_flatbuffer_t* fb, const char* s) {
  size_t len;

  if (NULL == s) {
    return 0;
  }

  /*
   * Strings are written to the buffer as a vector of bytes including the
   * NUL terminator. However, the terminator is not included in the
   * length. This allows clients that expect NUL terminated strings to
   * use the string directly without first making a copy.
   */

  len = (size_t)nr_strlen(s);
  nr_flatbuffers_prep(fb, sizeof(uint32_t), len + 1);
  fb->pos -= len + 1;
  nr_memcpy(fb->pos, s, len + 1);
  return nr_flatbuffers_vector_end(fb, len);
}

uint32_t nr_flatbuffers_prepend_bytes(nr_flatbuffer_t* fb,
                                      const void* src,
                                      uint32_t len) {
  nr_flatbuffers_prep(fb, sizeof(uint32_t), len);
  fb->pos -= len;
  nr_memcpy(fb->pos, src, len);
  return nr_flatbuffers_vector_end(fb, len);
}

nr_status_t nr_flatbuffers_object_begin(nr_flatbuffer_t* fb, int num_fields) {
  if (NULL == fb) {
    return NR_FAILURE;
  }
  if (fb->inside_object) {
    return NR_FAILURE;
  }

  fb->inside_object = 1;
  fb->object_end = nr_flatbuffers_len(fb);
  fb->min_align = 1;

  /* reset vtable */
  if (fb->vtable_cap < num_fields) {
    fb->vtable
        = (uint32_t*)nr_realloc(fb->vtable, num_fields * sizeof(uint32_t));
    fb->vtable_cap = num_fields;
  }
  fb->vtable_len = num_fields;
  nr_memset(fb->vtable, 0, fb->vtable_cap * sizeof(uint32_t));

  return NR_SUCCESS;
}

#define NR_FLATBUFFERS_OBJECT_PREPEND(TYPE, SUFFIX)                          \
  void nr_flatbuffers_object_prepend_##SUFFIX(nr_flatbuffer_t* fb, size_t i, \
                                              TYPE x, TYPE d) {              \
    if (x != d) {                                                            \
      nr_flatbuffers_prepend_##SUFFIX(fb, x);                                \
      fb->vtable[i] = (uint32_t)nr_flatbuffers_len(fb);                      \
    }                                                                        \
  }

NR_FLATBUFFERS_OBJECT_PREPEND(int8_t, i8)
NR_FLATBUFFERS_OBJECT_PREPEND(int16_t, i16)
NR_FLATBUFFERS_OBJECT_PREPEND(int32_t, i32)
NR_FLATBUFFERS_OBJECT_PREPEND(int64_t, i64)
NR_FLATBUFFERS_OBJECT_PREPEND(uint8_t, u8)
NR_FLATBUFFERS_OBJECT_PREPEND(uint16_t, u16)
NR_FLATBUFFERS_OBJECT_PREPEND(uint32_t, u32)
NR_FLATBUFFERS_OBJECT_PREPEND(uint64_t, u64)
NR_FLATBUFFERS_OBJECT_PREPEND(float, f32)
NR_FLATBUFFERS_OBJECT_PREPEND(double, f64)
NR_FLATBUFFERS_OBJECT_PREPEND(uint32_t, uoffset)

#undef NR_FLATBUFFERS_OBJECT_PREPEND

void nr_flatbuffers_object_prepend_bool(nr_flatbuffer_t* fb,
                                        size_t i,
                                        int x,
                                        int d) {
  /* Normalize to 0 or 1. */
  x = x ? 1 : 0;
  d = d ? 1 : 0;

  if (x != d) {
    nr_flatbuffers_prepend_u8(fb, (uint8_t)x);
    fb->vtable[i] = (uint32_t)nr_flatbuffers_len(fb);
  }
}

void nr_flatbuffers_object_prepend_struct(nr_flatbuffer_t* fb,
                                          size_t i,
                                          uint32_t x,
                                          uint32_t d) {
  if (x != d) {
    fb->vtable[i] = (uint32_t)nr_flatbuffers_len(fb);
  }
}

static void nr_flatbuffers_save_vtable(nr_flatbuffer_t* fb,
                                       uint32_t vtable_offset) {
  if (fb->vtables_len == fb->vtables_cap) {
    int new_capacity = fb->vtables_cap * 2;

    fb->vtables
        = (uint32_t*)nr_realloc(fb->vtables, new_capacity * sizeof(uint32_t));
    fb->vtables_cap = new_capacity;
  }

  fb->vtables[fb->vtables_len] = vtable_offset;
  fb->vtables_len++;
}

/*
 * Returns the number of fields contained in the i-th vtable.
 */
static int nr_flatbuffers_vtable_num_fields(const nr_flatbuffer_t* fb, int i) {
  if (i < fb->vtables_len) {
    size_t offset;
    size_t size;

    /*
     * saved vtable offsets are relative to the end of the buffer. To
     * read the contents of the vtable, we first need to convert it
     * to a new offset relative to the front.
     */

    offset = nr_flatbuffers_len(fb) - (size_t)fb->vtables[i];
    size = (size_t)nr_flatbuffers_read_u16(fb->pos, offset);
    size -= VTABLE_METADATA_FIELDS * sizeof(uint16_t);
    return (int)(size / sizeof(uint16_t));
  }
  return 0;
}

static uint16_t nr_flatbuffers_vtable_field(const nr_flatbuffer_t* fb,
                                            int i,
                                            size_t j) {
  size_t offset;

  /*
   * saved vtable offsets are relative to the end of the buffer. To
   * read the contents of the vtable, we first need to convert it
   * to a new offset relative to the front.
   */

  offset = nr_flatbuffers_len(fb) - (size_t)fb->vtables[i];
  offset += VTABLE_METADATA_FIELDS * sizeof(uint16_t);
  offset += j * sizeof(uint16_t);
  return nr_flatbuffers_read_u16(fb->pos, offset);
}

static int nr_flatbuffers_match_vtable(const nr_flatbuffer_t* fb,
                                       int vtable_idx) {
  int i;
  int num_fields;
  uint16_t existing_field;

  num_fields = nr_flatbuffers_vtable_num_fields(fb, vtable_idx);
  if (fb->vtable_len != num_fields) {
    return 0;
  }

  for (i = 0; i < num_fields; i++) {
    existing_field = nr_flatbuffers_vtable_field(fb, vtable_idx, i);

    if ((uint32_t)existing_field != fb->vtable[i]) {
      return 0;
    }
  }

  return 1;
}

/*
 * Search backwards through existing vtables, because similar vtables
 * are likely to have been recently appended. In benchmarks performed
 * by the Flatbuffers project, this heuristic saves ~30% of the time
 * spent writing objects with duplicate tables.
 *
 * See: BenchmarkVtableDeduplication at https://github.com/google/flatbuffers
 */
static uint32_t nr_flatbuffers_find_existing_vtable(const nr_flatbuffer_t* fb) {
  int i;

  for (i = fb->vtables_len - 1; i >= 0; i--) {
    if (nr_flatbuffers_match_vtable(fb, i)) {
      return fb->vtables[i];
    }
  }

  return 0;
}

/*
 * See comment above WriteVtable in
 * src/vendor/github.com/google/flatbuffers/go/builder.go.
 */
static uint32_t nr_flatbuffers_prepend_vtable(nr_flatbuffer_t* fb) {
  int i;
  uint32_t object_offset;
  uint32_t existing_vtable;

  /*
   * Prepend a zero scalar to the object. Later in this function we'll
   * write an offset here that points to the object's vtable:
   */
  nr_flatbuffers_prepend_i32(fb, 0);
  object_offset = nr_flatbuffers_len(fb);

  /*
   * At this point, vtable contains offsets relative to the end of the buffer.
   * Now that we have the location of the object, we can calculate relative
   * offsets.  This changes the number of necessary bits from 32 to 16, but we
   * reuse the same 32 bit array.
   */
  for (i = 0; i < fb->vtable_len; i++) {
    if (fb->vtable[i]) {
      nr_flatbuffers_assert(object_offset > fb->vtable[i]);
      nr_flatbuffers_assert(object_offset - fb->vtable[i] < 0xFFFF);
      fb->vtable[i] = object_offset - fb->vtable[i];
    }
  }

  existing_vtable = nr_flatbuffers_find_existing_vtable(fb);

  if (0 == existing_vtable) {
    uint32_t object_start;
    uint32_t object_size;
    uint32_t vtable_size;

    /*
     * Did not find a vtable, so write this one to the buffer.
     *
     * Write out the current vtable in reverse, because
     * serialization occurs in last-first order:
     */
    for (i = fb->vtable_len - 1; i >= 0; i--) {
      nr_flatbuffers_prepend_u16(fb, (uint16_t)fb->vtable[i]);
    }

    object_size = object_offset - fb->object_end;
    nr_flatbuffers_prepend_u16(fb, (uint16_t)object_size);

    vtable_size = (fb->vtable_len + VTABLE_METADATA_FIELDS) * sizeof(uint16_t);
    nr_flatbuffers_prepend_u16(fb, (uint16_t)vtable_size);

    object_start = (int32_t)nr_flatbuffers_len(fb) - (int32_t)object_offset;
    nr_flatbuffers_encode_i32(fb->back - object_offset, object_start);

    /* Finally, store this vtable in memory for future deduplication: */
    nr_flatbuffers_save_vtable(fb, (uint32_t)nr_flatbuffers_len(fb));
  } else {
    /*
     * Found a duplicate vtable. Write the offset to the found vtable in
     * the already-allocated slot at the beginning of this object:
     */
    nr_flatbuffers_encode_i32(
        fb->back - object_offset,
        (int32_t)existing_vtable - (int32_t)object_offset);
  }

  fb->vtable_len = 0;
  return object_offset;
}

uint32_t nr_flatbuffers_object_end(nr_flatbuffer_t* fb) {
  uint32_t offset = 0;

  if (fb && fb->inside_object) {
    offset = nr_flatbuffers_prepend_vtable(fb);
    fb->inside_object = 0;
  }
  return offset;
}

void nr_flatbuffers_vector_begin(nr_flatbuffer_t* fb,
                                 size_t elem_size,
                                 size_t num_elems,
                                 size_t alignment) {
  size_t size;

  size = elem_size * num_elems;
  nr_flatbuffers_prep(fb, sizeof(uint32_t), size);
  nr_flatbuffers_prep(fb, alignment, size);
}

uint32_t nr_flatbuffers_vector_end(nr_flatbuffer_t* fb, size_t num_elems) {
  /* We already made space for this, so write it directly. */
  nr_flatbuffers_put_u32(fb, (uint32_t)num_elems);
  return nr_flatbuffers_len(fb);
}

void nr_flatbuffers_finish(nr_flatbuffer_t* fb, uint32_t root_table) {
  nr_flatbuffers_prep(fb, fb->min_align, sizeof(uint32_t));
  nr_flatbuffers_prepend_uoffset(fb, root_table);
}

void nr_flatbuffers_table_init_root(nr_flatbuffers_table_t* tbl,
                                    const uint8_t* data,
                                    size_t len) {
  size_t offset;

  offset = nr_flatbuffers_read_uoffset(data, 0);
  nr_flatbuffers_table_init(tbl, data, len, offset);
}

void nr_flatbuffers_table_init(nr_flatbuffers_table_t* tbl,
                               const uint8_t* data,
                               size_t len,
                               size_t offset) {
  tbl->data = data;
  tbl->length = len;
  tbl->offset = offset;

  /*
   * Vtable offsets are stored relative to the start of the object,
   * convert it to an absolute position within the buffer.
   */
  tbl->vtable = (size_t)(
      (ptrdiff_t)offset - (ptrdiff_t)nr_flatbuffers_read_soffset(data, offset));
  tbl->vsize = nr_flatbuffers_read_u16(data, tbl->vtable);
}

/*
 * Returns the absolute offset of the i-th field in the table. If the
 * field is not present, returns zero.
 */
nr_aoffset_t nr_flatbuffers_table_lookup(const nr_flatbuffers_table_t* tbl,
                                         size_t i) {
  size_t offset;
  nr_voffset_t field;
  nr_aoffset_t absolute = {0};

  offset = sizeof(uint16_t) * (VTABLE_METADATA_FIELDS + i);
  if (offset >= tbl->vsize) {
    return absolute; /* Return 0 */
  }

  field = nr_flatbuffers_read_voffset(tbl->data, tbl->vtable + offset);
  if (0 == field.offset) {
    return absolute; /* Return 0 */
  }

  absolute.offset = field.offset + tbl->offset;
  return absolute;
}

nr_aoffset_t nr_flatbuffers_table_read_vector(const nr_flatbuffers_table_t* tbl,
                                              size_t i) {
  nr_aoffset_t absolute;

  absolute = nr_flatbuffers_table_lookup(tbl, i);
  if (0 != absolute.offset) {
    absolute.offset += nr_flatbuffers_read_uoffset(tbl->data, absolute.offset);
    absolute.offset += sizeof(uint32_t);
  }
  return absolute;
}

uint32_t nr_flatbuffers_table_read_vector_len(const nr_flatbuffers_table_t* tbl,
                                              size_t i) {
  nr_aoffset_t absolute;

  absolute = nr_flatbuffers_table_lookup(tbl, i);
  if (0 != absolute.offset) {
    absolute.offset += nr_flatbuffers_read_uoffset(tbl->data, absolute.offset);
    return nr_flatbuffers_read_uoffset(tbl->data, absolute.offset);
  }
  return 0;
}

const void* nr_flatbuffers_table_read_bytes(const nr_flatbuffers_table_t* tbl,
                                            size_t i) {
  nr_aoffset_t absolute;

  absolute = nr_flatbuffers_table_lookup(tbl, i);
  if (0 != absolute.offset) {
    absolute.offset += nr_flatbuffers_read_uoffset(tbl->data, absolute.offset);

    /* First four bytes are the length. */
    if (0 == nr_flatbuffers_read_uoffset(tbl->data, absolute.offset)) {
      return NULL;
    }

    return tbl->data + absolute.offset + sizeof(uint32_t);
  }
  return NULL;
}

const char* nr_flatbuffers_table_read_str(const nr_flatbuffers_table_t* tbl,
                                          size_t i) {
  return (const char*)nr_flatbuffers_table_read_bytes(tbl, i);
}

int nr_flatbuffers_table_read_union(nr_flatbuffers_table_t* child,
                                    const nr_flatbuffers_table_t* parent,
                                    size_t i) {
  nr_aoffset_t absolute;

  absolute = nr_flatbuffers_table_lookup(parent, i);
  if (0 != absolute.offset) {
    absolute.offset
        += nr_flatbuffers_read_uoffset(parent->data, absolute.offset);
    nr_flatbuffers_table_init(child, parent->data, parent->length,
                              absolute.offset);
    return 1;
  }
  return 0;
}

/*
 * To read a value from a table:
 *
 * First read the relative offset of the field from the vtable.
 * Then, if present, read the value.
 */
#define FLATBUFFERS_TABLE_READ(TYPE, SUFFIX)                                 \
  TYPE nr_flatbuffers_table_read_##SUFFIX(const nr_flatbuffers_table_t* tbl, \
                                          size_t i, TYPE d) {                \
    nr_aoffset_t o = nr_flatbuffers_table_lookup(tbl, i);                    \
    if (0 != o.offset) {                                                     \
      return nr_flatbuffers_read_##SUFFIX(tbl->data, o.offset);              \
    }                                                                        \
    return d;                                                                \
  }

FLATBUFFERS_TABLE_READ(int8_t, i8)
FLATBUFFERS_TABLE_READ(int16_t, i16)
FLATBUFFERS_TABLE_READ(int32_t, i32)
FLATBUFFERS_TABLE_READ(int64_t, i64)
FLATBUFFERS_TABLE_READ(uint8_t, u8)
FLATBUFFERS_TABLE_READ(uint16_t, u16)
FLATBUFFERS_TABLE_READ(uint32_t, u32)
FLATBUFFERS_TABLE_READ(uint64_t, u64)
FLATBUFFERS_TABLE_READ(float, f32)
FLATBUFFERS_TABLE_READ(double, f64)

#undef FLATBUFFERS_TABLE_READ

int nr_flatbuffers_table_read_bool(const nr_flatbuffers_table_t* tbl,
                                   size_t i,
                                   int d) {
  return nr_flatbuffers_table_read_u8(tbl, i, d ? 1 : 0) != 0;
}
