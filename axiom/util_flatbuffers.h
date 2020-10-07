/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file implements support for the flatbuffers serialization format.
 *
 * See: http://google.github.io/flatbuffers/index.html
 */
#ifndef UTIL_FLATBUFFERS_HDR
#define UTIL_FLATBUFFERS_HDR

#include <stddef.h>
#include <stdint.h>

/*
 * State machine for building flatbuffer objects. Use a nr_flatbuffer_t to
 * construct object(s) starting from leaf nodes.
 *
 * Byte buffers are constructed in a last-first manner for simplicity and
 * performance.
 */
typedef struct _nr_flatbuffer_t nr_flatbuffer_t;

/*
 * Offset within a vtable.  These offsets are relative to the object.
 */
typedef struct _nr_voffset_t nr_voffset_t;

/*
 * Absolute offset, relative to entire flatbuffer buffer.
 */
typedef struct {
  uint32_t offset;
} nr_aoffset_t;

/*
 * Purpose : Returns a new buffer with the given initial capacity.
 *
 * Params  : 1. The initial capacity in bytes.
 *
 * Returns : A new, empty buffer.
 */
extern nr_flatbuffer_t* nr_flatbuffers_create(size_t initial_size);

/*
 * Purpose : Returns a pointer to the first byte in the buffer.
 *
 * Params  : 1. The flatbuffer.
 *
 * Returns : A pointer to the first byte in the buffer.
 */
extern const uint8_t* nr_flatbuffers_data(const nr_flatbuffer_t* fb);

/*
 * Purpose : Returns the size in bytes of the buffer.
 *
 * Params  : 1. The flatbuffer.
 *
 * Returns : The size of the buffer in bytes.
 */
extern size_t nr_flatbuffers_len(const nr_flatbuffer_t* fb);

/*
 * Purpose : Prepares to write an element of `size` bytes after
 *           `additional_bytes` have been written. If all you need to do
 *           is align, `additional_bytes` will be 0.
 *
 * Params  : 1. The flatbuffer.
 *           2. The size of the element that will be written after
 *              `additional_bytes`.
 *           3. The number of bytes that will be written first.
 *
 * Notes   : To write a string, prefixed by its length, you need to align
 *           such that the length field is aligned to sizeof(int32_t), and
 *           the string data follows it directly.
 */
extern void nr_flatbuffers_prep(nr_flatbuffer_t* fb,
                                size_t size,
                                size_t additional_bytes);

/*
 * Purpose : Prepend zeros to the flatbuffer.
 *
 * Params  : 1. The flatbuffer.
 *           2. The number of zeros to prepend.
 */
extern void nr_flatbuffers_pad(nr_flatbuffer_t* fb, size_t n);

/*
 * Purpose : Prepends a primitive value to the flatbuffer.
 *           Aligns and checks for space.
 *
 * Params  : 1. The flatbuffer object to update.
 *           2. The value to prepend.
 */
extern void nr_flatbuffers_prepend_i8(nr_flatbuffer_t* fb, int8_t x);
extern void nr_flatbuffers_prepend_i16(nr_flatbuffer_t* fb, int16_t x);
extern void nr_flatbuffers_prepend_i32(nr_flatbuffer_t* fb, int32_t x);
extern void nr_flatbuffers_prepend_i64(nr_flatbuffer_t* fb, int64_t x);
extern void nr_flatbuffers_prepend_u8(nr_flatbuffer_t* fb, uint8_t x);
extern void nr_flatbuffers_prepend_u16(nr_flatbuffer_t* fb, uint16_t x);
extern void nr_flatbuffers_prepend_u32(nr_flatbuffer_t* fb, uint32_t x);
extern void nr_flatbuffers_prepend_u64(nr_flatbuffer_t* fb, uint64_t x);
extern void nr_flatbuffers_prepend_f32(nr_flatbuffer_t* fb, float x);
extern void nr_flatbuffers_prepend_f64(nr_flatbuffer_t* fb, double x);
extern void nr_flatbuffers_prepend_bool(nr_flatbuffer_t* fb, int x);
extern void nr_flatbuffers_prepend_uoffset(nr_flatbuffer_t* fb, uint32_t x);

/*
 * Purpose : Prepends a NUL-terminated string to the buffer.
 *
 * Params  : 1. The flatbuffer.
 *           2. The string to prepend.
 *
 * Returns : A reference to the string representing its offset relative
 *           to the end of the buffer. Use this value to refer to the
 *           string in a parent object.  Returns 0 if the string is NULL.
 */
extern uint32_t nr_flatbuffers_prepend_string(nr_flatbuffer_t* fb,
                                              const char* s);

/*
 * Purpose : Prepends an array of bytes to the buffer.
 *
 * Params  : 1. The flatbuffer.
 *           2. The byte array to prepend.
 *
 * Returns : A reference to the array representing its offset relative to
 *           the end of the buffer. Use this value to refer to the array
 *           from another object. (See nr_flatbuffers_object_prepend_uoffset)
 */
extern uint32_t nr_flatbuffers_prepend_bytes(nr_flatbuffer_t* fb,
                                             const void* src,
                                             uint32_t len);

/*
 * Purpose : Begins a new vector whose contents will be prepended to the buffer.
 *
 * Params  : 1. The flatbuffer.
 *           2. Size of each element in bytes.
 *           3. The number of elements.
 *           4. Required alignment for the first element.
 *
 * Notes   : To preserve the order of the elements, prepend them in reverse
 *           order. Flatbuffers are constructed from back-to-front.
 */
extern void nr_flatbuffers_vector_begin(nr_flatbuffer_t* fb,
                                        size_t elem_size,
                                        size_t num_elems,
                                        size_t alignment);

/*
 * Purpose : Finalizes a vector.
 *
 * Params  : 1. The flatbuffer.
 *           2. The number of elements in the vector.
 *
 * Returns : A reference to the vector representing its offset relative to
 *           the end of the buffer. Use this value to refer to the vector
 *           from another object. (See nr_flatbuffers_object_prepend_uoffset)
 */
extern uint32_t nr_flatbuffers_vector_end(nr_flatbuffer_t* fb,
                                          size_t num_elems);

/*
 * Purpose : Begins a new object in the buffer.
 *
 * Params  : 1. The flatbuffer.
 *           2. The number of fields in the object. i.e. The number of slots
 *              in the object's vtable.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_flatbuffers_object_begin(nr_flatbuffer_t* fb,
                                               int num_fields);

/*
 * Purpose : Prepends a field to the current object.
 *
 * Params  : 1. The flatbuffer.
 *           2. The index of the field in the object's vtable. Field indices
 *              are derived from the IDL by the flatbuffers compiler.
 *           3. The field's value.
 *           4. The field's default value.
 */
extern void nr_flatbuffers_object_prepend_i8(nr_flatbuffer_t* fb,
                                             size_t i,
                                             int8_t x,
                                             int8_t d);
extern void nr_flatbuffers_object_prepend_i16(nr_flatbuffer_t* fb,
                                              size_t i,
                                              int16_t x,
                                              int16_t d);
extern void nr_flatbuffers_object_prepend_i32(nr_flatbuffer_t* fb,
                                              size_t i,
                                              int32_t x,
                                              int32_t d);
extern void nr_flatbuffers_object_prepend_i64(nr_flatbuffer_t* fb,
                                              size_t i,
                                              int64_t x,
                                              int64_t d);
extern void nr_flatbuffers_object_prepend_u8(nr_flatbuffer_t* fb,
                                             size_t i,
                                             uint8_t x,
                                             uint8_t d);
extern void nr_flatbuffers_object_prepend_u16(nr_flatbuffer_t* fb,
                                              size_t i,
                                              uint16_t x,
                                              uint16_t d);
extern void nr_flatbuffers_object_prepend_u32(nr_flatbuffer_t* fb,
                                              size_t i,
                                              uint32_t x,
                                              uint32_t d);
extern void nr_flatbuffers_object_prepend_u64(nr_flatbuffer_t* fb,
                                              size_t i,
                                              uint64_t x,
                                              uint64_t d);
extern void nr_flatbuffers_object_prepend_f32(nr_flatbuffer_t* fb,
                                              size_t i,
                                              float x,
                                              float d);
extern void nr_flatbuffers_object_prepend_f64(nr_flatbuffer_t* fb,
                                              size_t i,
                                              double x,
                                              double d);
extern void nr_flatbuffers_object_prepend_bool(nr_flatbuffer_t* fb,
                                               size_t i,
                                               int x,
                                               int d);
extern void nr_flatbuffers_object_prepend_uoffset(nr_flatbuffer_t* fb,
                                                  size_t i,
                                                  uint32_t x,
                                                  uint32_t d);
extern void nr_flatbuffers_object_prepend_struct(nr_flatbuffer_t* fb,
                                                 size_t i,
                                                 uint32_t x,
                                                 uint32_t d);

/*
 * Purpose : Finalize the current object being written to the buffer.
 *
 * Params  : The flatbuffer.
 *
 * Returns : A reference to this object represented as its offset relative
 *           to the end of the buffer. Use this value to refer to this object
 *           from an (as yet) unwritten parent object by calling
 *           nr_flatbuffers_object_prepend_uoffset.
 */
extern uint32_t nr_flatbuffers_object_end(nr_flatbuffer_t* fb);

/*
 * Purpose : Read a value from a flatbuffer.
 *
 * Params  : 1. Pointer to the front of a flatbuffer.
 *           2. The offset in bytes from the front of `buf` to the
 *              desired value.
 *
 * Returns : The value read from `buf`.
 *
 * Notes   : The bounds of the read is unchecked. Before decoding a
 *           flatbuffer, it should be verified by nr_flatbuffers_verify,
 *           which will ensure the well-formedness of the data.
 */
extern int8_t nr_flatbuffers_read_i8(const uint8_t* buf, size_t offset);
extern int16_t nr_flatbuffers_read_i16(const uint8_t* buf, size_t offset);
extern int32_t nr_flatbuffers_read_i32(const uint8_t* buf, size_t offset);
extern int64_t nr_flatbuffers_read_i64(const uint8_t* buf, size_t offset);
extern uint8_t nr_flatbuffers_read_u8(const uint8_t* buf, size_t offset);
extern uint16_t nr_flatbuffers_read_u16(const uint8_t* buf, size_t offset);
extern uint32_t nr_flatbuffers_read_u32(const uint8_t* buf, size_t offset);
extern uint64_t nr_flatbuffers_read_u64(const uint8_t* buf, size_t offset);
extern float nr_flatbuffers_read_f32(const uint8_t* buf, size_t offset);
extern double nr_flatbuffers_read_f64(const uint8_t* buf, size_t offset);
extern int32_t nr_flatbuffers_read_soffset(const uint8_t* buf, size_t offset);
extern uint32_t nr_flatbuffers_read_uoffset(const uint8_t* buf, size_t offset);
extern nr_voffset_t nr_flatbuffers_read_voffset(const uint8_t* buf,
                                                size_t offset);

/*
 * Purpose : Reads a value from a flatbuffer representing the offset of
 *           another value. Think of this as derefencing a pointer.
 *
 * Params  : 1. The buffer to read.
 *           2. The offset of the value to read.
 *
 * Returns : An offset.
 */
extern nr_aoffset_t nr_flatbuffers_read_indirect(const uint8_t* buf,
                                                 nr_aoffset_t pos);

/*
 * Purpose : Finalizes a buffer by prepending the offset of the root object.
 *
 * Params  : 1. The flatbuffer to finalize.
 *           2. The offset of the root object.
 */
extern void nr_flatbuffers_finish(nr_flatbuffer_t* fb, uint32_t root_table);

/*
 * Purpose : Frees any resources associated with a builder including
 *           the builder itself.
 *
 * Params  : The flatbuffers builder to destroy.
 */
extern void nr_flatbuffers_destroy(nr_flatbuffer_t** fb_ptr);

/*
 * Provides a read-only view of an object within a buffer.
 */
typedef struct _nr_flatbuffers_table_t {
  const uint8_t* data; /* flatbuffer */
  size_t length;       /* flatbuffer size in bytes */
  size_t offset;       /* table position */
  size_t vtable;       /* cached vtable position */
  size_t vsize;        /* cached vtable size in bytes */
} nr_flatbuffers_table_t;

/*
 * Purpose : Initialize a read-only view of a table within a flatbuffer.
 *           The view can be used to read the table's fields.
 *
 * Params  : 1. The table to initialize.
 *           2. A pointer to the front of the flatbuffer.
 *           3. The length of the flatbuffer in bytes.
 *           4. The position of the table within the flatbuffer, in bytes.
 *
 * Notes   : This function does not validate the table. The flatbuffer
 *           should be verified before decoding.
 */
extern void nr_flatbuffers_table_init(nr_flatbuffers_table_t* tbl,
                                      const uint8_t* data,
                                      size_t len,
                                      size_t offset);

/*
 * Purpose : Initialize a read-only view of the root table in a flatbuffer.
 *
 * Params  : 1. The table to initialize.
 *           2. A pointer to the start of the flatbuffer.
 *           3. The length of the flatbuffer in bytes.
 * Notes   : This function does not validate the table. The flatbuffer
 *           should be verified before decoding.
 */
extern void nr_flatbuffers_table_init_root(nr_flatbuffers_table_t* tbl,
                                           const uint8_t* data,
                                           size_t len);

/*
 * Purpose : Returns the absolute offset of the i-th field in the table.
 *
 * Params  : 1. The table.
 *           2. The zero-based field index.
 *
 * Returns : The absolute offset of the field, if present; otherwise, zero.
 */
extern nr_aoffset_t nr_flatbuffers_table_lookup(
    const nr_flatbuffers_table_t* tbl,
    size_t i);

/*
 * Purpose : Read the value of a table field in a flatbuffer.
 *
 * Params  : 1. The table.
 *           2. The zero-based field index.
 *              (Determined by the flatbuffers compiler.)
 *           3. The default value.
 *
 * Returns : The value of the field, if present; otherwise, the default value.
 */
extern int8_t nr_flatbuffers_table_read_i8(const nr_flatbuffers_table_t* tbl,
                                           size_t i,
                                           int8_t d);
extern int16_t nr_flatbuffers_table_read_i16(const nr_flatbuffers_table_t* tbl,
                                             size_t i,
                                             int16_t d);
extern int32_t nr_flatbuffers_table_read_i32(const nr_flatbuffers_table_t* tbl,
                                             size_t i,
                                             int32_t d);
extern int64_t nr_flatbuffers_table_read_i64(const nr_flatbuffers_table_t* tbl,
                                             size_t i,
                                             int64_t d);
extern uint8_t nr_flatbuffers_table_read_u8(const nr_flatbuffers_table_t* tbl,
                                            size_t i,
                                            uint8_t d);
extern uint16_t nr_flatbuffers_table_read_u16(const nr_flatbuffers_table_t* tbl,
                                              size_t i,
                                              uint16_t d);
extern uint32_t nr_flatbuffers_table_read_u32(const nr_flatbuffers_table_t* tbl,
                                              size_t i,
                                              uint32_t d);
extern uint64_t nr_flatbuffers_table_read_u64(const nr_flatbuffers_table_t* tbl,
                                              size_t i,
                                              uint64_t d);
extern float nr_flatbuffers_table_read_f32(const nr_flatbuffers_table_t* tbl,
                                           size_t i,
                                           float d);
extern double nr_flatbuffers_table_read_f64(const nr_flatbuffers_table_t* tbl,
                                            size_t i,
                                            double d);
extern int nr_flatbuffers_table_read_bool(const nr_flatbuffers_table_t* tbl,
                                          size_t i,
                                          int d);

/*
 * Purpose : Read the value of a table field whose type is a vector of bytes.
 *
 * Params  : 1. The table.
 *           2. The zero-based field index.
 *              (Determined by the flatbuffers compiler.)
 *
 * Returns : A pointer to the first element in the vector.
 */
extern const void* nr_flatbuffers_table_read_bytes(
    const nr_flatbuffers_table_t* tbl,
    size_t i);

/*
 * Purpose : Read the value of a table field whose type is a zero-terminated
 *           vector of bytes.
 *
 * Params  : 1. The table.
 *           2. The zero-based field index.
 *              (Determined by the flatbuffers compiler.)
 *
 * Returns : A pointer to the string.
 */
extern const char* nr_flatbuffers_table_read_str(
    const nr_flatbuffers_table_t* tbl,
    size_t i);

/*
 * Purpose : Read the position of the first element of vector in a table.
 *
 * Params  : 1. The table.
 *           2. The zero-based field index.
 *              (Determined by the flatbuffers compiler.)
 *
 * Returns : The position of the first element of the vector relative to
 *           the start of the flatbuffer.
 */
extern nr_aoffset_t nr_flatbuffers_table_read_vector(
    const nr_flatbuffers_table_t* tbl,
    size_t i);

/*
 * Purpose : Read the length of a vector stored in a table field.
 *
 * Params : 1. The table.
 *          2. The zero-based field index.
 *             (Determined by the flatbuffers compiler.)
 *
 * Returns : The number of elements in the vector.
 */
extern uint32_t nr_flatbuffers_table_read_vector_len(
    const nr_flatbuffers_table_t* tbl,
    size_t i);

/*
 * Purpose : Read the value of a union field in a table.
 *
 * Params  : 1. Pointer to the table to initialize from the field.
 *           2. Pointer to the parent table that contains the field.
 *           3. The zero-based field index.
 *              (Determined by the flatbuffers compiler.)
 *
 * Returns : Non-zero if the field is present; zero if the field is missing.
 */
extern int nr_flatbuffers_table_read_union(nr_flatbuffers_table_t* child,
                                           const nr_flatbuffers_table_t* parent,
                                           size_t i);

#endif /* UTIL_FLATBUFFERS_HDR */
