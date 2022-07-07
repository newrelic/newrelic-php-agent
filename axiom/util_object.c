/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "util_buffer.h"
#include "util_memory.h"
#include "util_number_converter.h"
#include "util_object.h"
#include "util_object_private.h"
#include "util_strings.h"

/*
 * Set the chunk size we use to allocate for hashes and arrays.
 */
#define NRO_CHUNK_SIZE 8

/*
 * This file implements the generic object. Unlike its use in the php agent
 * where the internals of this type are visible to all, in this implementation
 * the object is completely opaque. All member access is done through the
 * functions defined here. That is why the actual structure is defined here
 * and not in a header file. This gives us the opportunity to change the
 * internals in the future should the need ever arise, without affecting any
 * external code.
 *
 * In order to shorten the function names and to increase legibility, we use
 * the prefix nro_ for all functions, which stands for "New Relic Object".
 */
typedef struct _nrohash_t {
  int size;
  int allocated;
  char** keys;
  struct _nrintobj_t** data;
} nrohash_t;

typedef struct _nrarray_t {
  int size;
  int allocated;
  struct _nrintobj_t** data;
} nrarray_t;

typedef struct _nrintobj_t {
  nrotype_t type;
  union {
    int ival;       /* int */
    int64_t lval;   /* long */
    uint64_t ulval; /* ulong */
    double dval;    /* double */
    char* sval;     /* string (strdup'ed) */
    nrohash_t hval; /* hash */
    nrarray_t aval; /* array */
  } u;
} nrintobj_t;

static nr_status_t nro_internal_setvalue_array(nrintobj_t* op,
                                               int idx,
                                               nrobj_t* nobj);
static nr_status_t nro_internal_setvalue_hash(nrintobj_t* op,
                                              const char* key,
                                              nrobj_t* nobj);

nrotype_t nro_type(const nrobj_t* obj) {
  if (0 == obj) {
    return NR_OBJECT_INVALID;
  }

  return obj->type;
}

nrobj_t* nro_assert(nrobj_t* obj, nrotype_t type) {
  if (nrunlikely(NULL == obj)) {
    return NULL;
  }
  if (nrlikely(type == obj->type)) {
    return obj;
  }
  return NULL;
}

/*
 * A variant of nro_assert, but for constant objects.
 */
static const nrobj_t* nro_cassert(const nrobj_t* obj, nrotype_t type) {
  if (nrunlikely(NULL == obj)) {
    return NULL;
  }
  if (nrlikely(type == obj->type)) {
    return obj;
  }
  return NULL;
}

static void nro_internal_new(nrintobj_t* op) {
  switch (op->type) {
    case NR_OBJECT_INVALID:
    case NR_OBJECT_NONE:
    case NR_OBJECT_BOOLEAN:
    case NR_OBJECT_INT:
    case NR_OBJECT_LONG:
    case NR_OBJECT_ULONG:
    case NR_OBJECT_DOUBLE:
    case NR_OBJECT_STRING:
    case NR_OBJECT_JSTRING:
      /* Nothing to allocate for these until set */
      break;

    case NR_OBJECT_HASH:
      op->u.hval.allocated = NRO_CHUNK_SIZE;
      op->u.hval.keys = (char**)nr_calloc(NRO_CHUNK_SIZE, sizeof(char*));
      op->u.hval.data
          = (nrintobj_t**)nr_calloc(NRO_CHUNK_SIZE, sizeof(nrintobj_t*));
      break;

    case NR_OBJECT_ARRAY:
      op->u.aval.allocated = NRO_CHUNK_SIZE;
      op->u.aval.data
          = (nrintobj_t**)nr_calloc(NRO_CHUNK_SIZE, sizeof(nrintobj_t*));
      break;
  }
}

nrobj_t* nro_new(nrotype_t type) {
  nrintobj_t* op;

  if (NR_OBJECT_INVALID == type) {
    return 0;
  }
  op = (nrintobj_t*)nr_zalloc(sizeof(nrintobj_t));
  op->type = type;
  nro_internal_new(op);
  return (nrobj_t*)op;
}

static nrintobj_t* nro_internal_new_and_construct(nrotype_t type) {
  nrintobj_t* op;

  op = (nrintobj_t*)nr_zalloc(sizeof(nrintobj_t));
  op->type = type;
  nro_internal_new(op);
  return op;
}

nrobj_t* nro_new_none(void) {
  nrobj_t* obj = nro_internal_new_and_construct(NR_OBJECT_NONE);

  return obj;
}

nrobj_t* nro_new_boolean(int x) {
  nrobj_t* obj = nro_internal_new_and_construct(NR_OBJECT_BOOLEAN);

  if (x) {
    obj->u.ival = 1;
  } else {
    obj->u.ival = 0;
  }

  return obj;
}

nrobj_t* nro_new_int(int x) {
  nrobj_t* obj = nro_internal_new_and_construct(NR_OBJECT_INT);

  obj->u.ival = x;
  return obj;
}

nrobj_t* nro_new_long(int64_t x) {
  nrobj_t* obj = nro_internal_new_and_construct(NR_OBJECT_LONG);

  obj->u.lval = x;
  return obj;
}

nrobj_t* nro_new_ulong(uint64_t x) {
  nrobj_t* obj = nro_internal_new_and_construct(NR_OBJECT_ULONG);

  obj->u.ulval = x;
  return obj;
}

nrobj_t* nro_new_double(double x) {
  nrobj_t* obj = nro_internal_new_and_construct(NR_OBJECT_DOUBLE);

  obj->u.dval = x;
  return obj;
}

nrobj_t* nro_new_string(const char* x) {
  nrobj_t* obj = nro_internal_new_and_construct(NR_OBJECT_STRING);

  obj->u.sval = nr_strdup(x);
  return obj;
}

nrobj_t* nro_new_jstring(const char* x) {
  nrobj_t* obj = nro_internal_new_and_construct(NR_OBJECT_JSTRING);

  obj->u.sval = nr_strdup(x);
  return obj;
}

nrobj_t* nro_new_hash(void) {
  nrobj_t* obj = nro_internal_new_and_construct(NR_OBJECT_HASH);

  return obj;
}

nrobj_t* nro_new_array(void) {
  nrobj_t* obj = nro_internal_new_and_construct(NR_OBJECT_ARRAY);

  return obj;
}

static void nro_internal_delete(nrintobj_t* op, int freebase) {
  int i;

  if (0 == op) {
    return;
  }

  switch (op->type) {
    case NR_OBJECT_INVALID:
    case NR_OBJECT_NONE:
    case NR_OBJECT_BOOLEAN:
    case NR_OBJECT_INT:
    case NR_OBJECT_LONG:
    case NR_OBJECT_ULONG:
    case NR_OBJECT_DOUBLE:
      /* Nothing to free for these */
      break;

    case NR_OBJECT_STRING:
    case NR_OBJECT_JSTRING:
      nr_free(op->u.sval);
      op->u.sval = 0;
      break;

    case NR_OBJECT_HASH:
      for (i = 0; i < op->u.hval.size; i++) {
        nr_free(op->u.hval.keys[i]);
        nro_internal_delete(op->u.hval.data[i], 1);
        op->u.hval.keys[i] = 0;
        op->u.hval.data[i] = 0;
      }
      nr_free(op->u.hval.keys);
      nr_free(op->u.hval.data);
      op->u.hval.size = 0;
      op->u.hval.allocated = 0;
      op->u.hval.keys = 0;
      op->u.hval.data = 0;
      break;

    case NR_OBJECT_ARRAY:
      for (i = 0; i < op->u.aval.size; i++) {
        nro_internal_delete(op->u.aval.data[i], 1);
        op->u.aval.data[i] = 0;
      }
      nr_free(op->u.aval.data);
      op->u.aval.size = 0;
      op->u.aval.allocated = 0;
      op->u.aval.data = 0;
      break;
  }

  if (freebase) {
    nr_free(op);
  }
}

void nro_real_delete(nrobj_t** obj) {
  nrintobj_t** op = (nrintobj_t**)obj;

  if ((0 == op) || (0 == *op)) {
    return;
  }

  nro_internal_delete(*op, 1);
  *obj = 0;
}

/*
 * Search for an existing key in a hash.
 * Returns : its position if found (positional range from 0 .. size)
 *           -2 if no key was found
 *           -1 on any real error
 */
static int nro_find_key_in_hash(const nrintobj_t* op, const char* key) {
  int i;

  if (NR_OBJECT_HASH != op->type) {
    return -1;
  }

  if ((const char*)0 == key) {
    return -1;
  }
  if (0 == key[0]) {
    return -1;
  }

  if (0 == op->u.hval.size) {
    return -2;
  }

  for (i = 0; i < op->u.hval.size; i++) {
    if (0 == nr_strcmp(op->u.hval.keys[i], key)) {
      return i;
    }
  }

  return -2;
}

static nr_status_t nro_internal_setvalue_array(nrintobj_t* op,
                                               int idx,
                                               nrobj_t* nobj) {
  int i;

  if (NULL == op) {
    return NR_FAILURE;
  }
  if (NR_OBJECT_ARRAY != op->type) {
    return NR_FAILURE;
  }
  if (0 == nobj) {
    return NR_FAILURE;
  }
  if (nobj == op) {
    /* Can't add ourselves to ourselves */
    return NR_FAILURE;
  }

  /*
   * Note that the value is an nrobj_t, which can also be an array. This
   * allows us to have multi-dimensional arrays. Index is 1-based, with 0
   * meaning "add to the end of the array".
   */
  if (idx < 0) {
    /* Array position out of bounds */
    return NR_FAILURE;
  }
  if (idx > 0) {
    idx--;
    if (idx > op->u.aval.size) {
      /* Array position out of bounds */
      return NR_FAILURE;
    }
    if (idx == op->u.aval.size) {
      idx = 0;
    }
  }

  /*
   * If the 0 == idx, we are adding a new element. We increment the
   * size and reallocate the arrays if we have reached the currently
   * allocated array size.
   */
  if (0 == idx) {
    if (op->u.aval.size == op->u.aval.allocated) {
      op->u.aval.allocated += NRO_CHUNK_SIZE;
      op->u.aval.data = (nrintobj_t**)nr_realloc(
          (void*)op->u.aval.data, op->u.aval.allocated * sizeof(nrintobj_t*));

      /* Set the newly allocated memory to 0 */
      for (i = op->u.aval.size; i < op->u.aval.allocated; i++) {
        op->u.aval.data[i] = 0;
      }
    }
    idx = op->u.aval.size;
    op->u.aval.size++;
  } else {
    /*
     * Replacing an existing element. Free the current one.
     */
    nro_delete(op->u.aval.data[idx]);
  }
  op->u.aval.data[idx] = nobj;
  return NR_SUCCESS;
}

static nr_status_t nro_internal_setvalue_hash(nrintobj_t* op,
                                              const char* key,
                                              nrobj_t* nobj) {
  int i;
  int idx;

  if (NULL == op) {
    return NR_FAILURE;
  }
  if (NR_OBJECT_HASH != op->type) {
    return NR_FAILURE;
  }
  if (0 == nobj) {
    return NR_FAILURE;
  }
  if (nobj == op) {
    /* Can not add ourselves to ourselves */
    return NR_FAILURE;
  }

  /*
   * For hashes we always check to see if the key already exists in the
   * hash. If it does we free its object. If not we simply add the new
   * key=value pair. Note that the value does not have to be a string.
   * It can be an array or another hash, thus allowing multi-dimension
   * hashes.
   */
  idx = nro_find_key_in_hash(op, key);
  if (-1 == idx) {
    return NR_FAILURE;
  }

  if (-2 != idx) {
    /*
     * We have existing data for this value. Free it.
     */
    nro_delete(op->u.hval.data[idx]);
  } else {
    /*
     * The key does not exist. Create it and set the value, extending the
     * arrays as needed.
     */
    idx = op->u.hval.size;
    if (idx == op->u.hval.allocated) {
      op->u.hval.allocated += NRO_CHUNK_SIZE;
      op->u.hval.keys = (char**)nr_realloc(
          op->u.hval.keys, op->u.hval.allocated * sizeof(char*));
      op->u.hval.data = (nrintobj_t**)nr_realloc(
          op->u.hval.data, op->u.hval.allocated * sizeof(nrintobj_t*));

      /* Set the newly allocated memory to 0 */
      for (i = op->u.hval.size; i < op->u.hval.allocated; i++) {
        op->u.hval.keys[i] = 0;
        op->u.hval.data[i] = 0;
      }
    }
    op->u.hval.size++;
    op->u.hval.keys[idx] = nr_strdup(key);
  }
  op->u.hval.data[idx] = nobj;
  return NR_SUCCESS;
}

nr_status_t nro_set_hash(nrobj_t* obj, const char* key, const nrobj_t* value) {
  nr_status_t rv;
  nrobj_t* dup;

  dup = nro_copy(value);

  rv = nro_internal_setvalue_hash(obj, key, dup);
  if (NR_SUCCESS != rv) {
    nro_delete(dup);
  }
  return rv;
}

nr_status_t nro_set_array(nrobj_t* obj, int idx, const nrobj_t* value) {
  nr_status_t rv;
  nrobj_t* dup;

  dup = nro_copy(value);

  rv = nro_internal_setvalue_array(obj, idx, dup);
  if (NR_SUCCESS != rv) {
    nro_delete(dup);
  }
  return rv;
}

nr_status_t nro_set_hash_none(nrobj_t* obj, const char* key) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_none();

  rv = nro_internal_setvalue_hash(obj, key, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_hash_boolean(nrobj_t* obj, const char* key, int value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_boolean(value);

  rv = nro_internal_setvalue_hash(obj, key, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_hash_int(nrobj_t* obj, const char* key, int value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_int(value);

  rv = nro_internal_setvalue_hash(obj, key, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_hash_long(nrobj_t* obj, const char* key, int64_t value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_long(value);

  rv = nro_internal_setvalue_hash(obj, key, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_hash_ulong(nrobj_t* obj, const char* key, uint64_t value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_ulong(value);

  rv = nro_internal_setvalue_hash(obj, key, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_hash_double(nrobj_t* obj, const char* key, double value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_double(value);

  rv = nro_internal_setvalue_hash(obj, key, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_hash_string(nrobj_t* obj,
                                const char* key,
                                const char* value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_string(value);

  rv = nro_internal_setvalue_hash(obj, key, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_hash_jstring(nrobj_t* obj,
                                 const char* key,
                                 const char* value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_jstring(value);

  rv = nro_internal_setvalue_hash(obj, key, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_array_none(nrobj_t* obj, int idx) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_none();

  rv = nro_internal_setvalue_array(obj, idx, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_array_boolean(nrobj_t* obj, int idx, int value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_boolean(value);

  rv = nro_internal_setvalue_array(obj, idx, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_array_int(nrobj_t* obj, int idx, int value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_int(value);

  rv = nro_internal_setvalue_array(obj, idx, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_array_long(nrobj_t* obj, int idx, int64_t value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_long(value);

  rv = nro_internal_setvalue_array(obj, idx, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_array_ulong(nrobj_t* obj, int idx, uint64_t value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_ulong(value);

  rv = nro_internal_setvalue_array(obj, idx, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_array_double(nrobj_t* obj, int idx, double value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_double(value);

  rv = nro_internal_setvalue_array(obj, idx, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_array_string(nrobj_t* obj, int idx, const char* value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_string(value);

  rv = nro_internal_setvalue_array(obj, idx, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

nr_status_t nro_set_array_jstring(nrobj_t* obj, int idx, const char* value) {
  nr_status_t rv;
  nrobj_t* ob = nro_new_jstring(value);

  rv = nro_internal_setvalue_array(obj, idx, ob);
  if (NR_SUCCESS != rv) {
    nro_delete(ob);
  }
  return rv;
}

/*
 * Getters
 */
int nro_get_hash_boolean(const nrobj_t* obj,
                         const char* key,
                         nr_status_t* errp) {
  return nro_get_boolean(nro_get_hash_value(obj, key, 0), errp);
}

int nro_get_hash_int(const nrobj_t* obj, const char* key, nr_status_t* errp) {
  return nro_get_int(nro_get_hash_value(obj, key, 0), errp);
}

int64_t nro_get_hash_long(const nrobj_t* obj,
                          const char* key,
                          nr_status_t* errp) {
  return nro_get_long(nro_get_hash_value(obj, key, 0), errp);
}

uint64_t nro_get_hash_ulong(const nrobj_t* obj,
                            const char* key,
                            nr_status_t* errp) {
  return nro_get_ulong(nro_get_hash_value(obj, key, 0), errp);
}

const char* nro_get_hash_string(const nrobj_t* obj,
                                const char* key,
                                nr_status_t* errp) {
  return nro_get_string(nro_get_hash_value(obj, key, 0), errp);
}

const char* nro_get_hash_jstring(const nrobj_t* obj,
                                 const char* key,
                                 nr_status_t* errp) {
  return nro_get_jstring(nro_get_hash_value(obj, key, 0), errp);
}

double nro_get_hash_double(const nrobj_t* obj,
                           const char* key,
                           nr_status_t* errp) {
  return nro_get_double(nro_get_hash_value(obj, key, 0), errp);
}

const nrobj_t* nro_get_hash_hash(const nrobj_t* obj,
                                 const char* key,
                                 nr_status_t* errp) {
  return nro_cassert(nro_get_hash_value(obj, key, errp), NR_OBJECT_HASH);
}

const nrobj_t* nro_get_hash_array(const nrobj_t* obj,
                                  const char* key,
                                  nr_status_t* errp) {
  return nro_cassert(nro_get_hash_value(obj, key, errp), NR_OBJECT_ARRAY);
}

int nro_get_array_boolean(const nrobj_t* obj, int key, nr_status_t* errp) {
  return nro_get_boolean(nro_get_array_value(obj, key, 0), errp);
}

int nro_get_array_int(const nrobj_t* obj, int key, nr_status_t* errp) {
  return nro_get_int(nro_get_array_value(obj, key, 0), errp);
}

int64_t nro_get_array_long(const nrobj_t* obj, int key, nr_status_t* errp) {
  return nro_get_long(nro_get_array_value(obj, key, 0), errp);
}

uint64_t nro_get_array_ulong(const nrobj_t* obj, int key, nr_status_t* errp) {
  return nro_get_ulong(nro_get_array_value(obj, key, 0), errp);
}

int nro_get_array_ival(const nrobj_t* obj, int key, nr_status_t* errp) {
  return nro_get_ival(nro_get_array_value(obj, key, 0), errp);
}

const char* nro_get_array_string(const nrobj_t* obj,
                                 int key,
                                 nr_status_t* errp) {
  return nro_get_string(nro_get_array_value(obj, key, 0), errp);
}

const char* nro_get_array_jstring(const nrobj_t* obj,
                                  int key,
                                  nr_status_t* errp) {
  return nro_get_jstring(nro_get_array_value(obj, key, 0), errp);
}

double nro_get_array_double(const nrobj_t* obj, int key, nr_status_t* errp) {
  return nro_get_double(nro_get_array_value(obj, key, 0), errp);
}

const nrobj_t* nro_get_array_hash(const nrobj_t* obj,
                                  int key,
                                  nr_status_t* errp) {
  return nro_cassert(nro_get_array_value(obj, key, errp), NR_OBJECT_HASH);
}

const nrobj_t* nro_get_array_array(const nrobj_t* obj,
                                   int key,
                                   nr_status_t* errp) {
  return nro_cassert(nro_get_array_value(obj, key, errp), NR_OBJECT_ARRAY);
}

void nro_iteratehash(const nrobj_t* obj, nrhashiter_t func, void* ptr) {
  const nrintobj_t* op = (const nrintobj_t*)obj;
  int i;

  if (0 == op) {
    return;
  }

  if (0 == func) {
    return;
  }

  if (NR_OBJECT_HASH != op->type) {
    return;
  }

  for (i = 0; i < op->u.hval.size; i++) {
    nr_status_t rv = func(op->u.hval.keys[i], op->u.hval.data[i], ptr);
    if (NR_FAILURE == rv) {
      return;
    }
  }
}

int nro_getsize(const nrobj_t* obj) {
  const nrintobj_t* op = (const nrintobj_t*)obj;

  if (0 == op) {
    return -1;
  }

  if (NR_OBJECT_ARRAY == op->type) {
    return (op->u.aval.size);
  } else if (NR_OBJECT_HASH == op->type) {
    return (op->u.hval.size);
  } else {
    return -1;
  }
}

nrobj_t* nro_copy(const nrobj_t* obj) {
  const nrintobj_t* op = (const nrintobj_t*)obj;
  nrintobj_t* np;
  int i;

  if (0 == op) {
    return 0;
  }

  np = (nrintobj_t*)nr_zalloc(sizeof(nrintobj_t));
  np->type = op->type;
  switch (op->type) {
    case NR_OBJECT_INVALID:
    case NR_OBJECT_NONE:
      /* Nothing to do */
      break;

    case NR_OBJECT_BOOLEAN:
    case NR_OBJECT_INT:
      np->u.ival = op->u.ival;
      break;

    case NR_OBJECT_LONG:
      np->u.lval = op->u.lval;
      break;

    case NR_OBJECT_ULONG:
      np->u.ulval = op->u.ulval;
      break;

    case NR_OBJECT_DOUBLE:
      np->u.dval = op->u.dval;
      break;

    case NR_OBJECT_STRING:
    case NR_OBJECT_JSTRING:
      np->u.sval = nr_strdup(op->u.sval);
      break;

    case NR_OBJECT_HASH:
      np->u.hval.size = op->u.hval.size;
      np->u.hval.allocated = np->u.hval.size;
      np->u.hval.keys = (char**)nr_calloc(np->u.hval.size, sizeof(char*));
      np->u.hval.data
          = (nrintobj_t**)nr_calloc(np->u.hval.size, sizeof(nrintobj_t*));
      for (i = 0; i < np->u.hval.size; i++) {
        np->u.hval.keys[i] = nr_strdup(op->u.hval.keys[i]);
        np->u.hval.data[i] = nro_copy(op->u.hval.data[i]);
      }
      break;

    case NR_OBJECT_ARRAY:
      np->u.aval.size = op->u.aval.size;
      np->u.aval.allocated = np->u.aval.size;
      np->u.aval.data
          = (nrintobj_t**)nr_calloc(np->u.aval.size, sizeof(nrintobj_t*));
      for (i = 0; i < np->u.aval.size; i++) {
        np->u.aval.data[i] = nro_copy(op->u.aval.data[i]);
      }
      break;
  }

  return ((nrobj_t*)np);
}

int nro_get_boolean(const nrobj_t* obj, nr_status_t* errp) {
  const nrintobj_t* op = (const nrintobj_t*)obj;

  if (0 == op) {
    goto error;
  }

  if (NR_OBJECT_BOOLEAN != op->type) {
    goto error;
  }

  if (0 != errp) {
    *errp = NR_SUCCESS;
  }

  return (op->u.ival);

error:
  if (0 != errp) {
    *errp = NR_FAILURE;
  }
  return -1;
}

int nro_get_int(const nrobj_t* obj, nr_status_t* errp) {
  const nrintobj_t* op = (const nrintobj_t*)obj;

  if (0 == op) {
    goto error;
  }

  if (NR_OBJECT_INT != op->type) {
    goto error;
  }

  if (0 != errp) {
    *errp = NR_SUCCESS;
  }

  return (op->u.ival);

error:
  if (0 != errp) {
    *errp = NR_FAILURE;
  }
  return -1;
}

int nro_get_ival(const nrobj_t* obj, nr_status_t* errp) {
  const nrintobj_t* op = (const nrintobj_t*)obj;
  int iret = 0;

  if (0 == op) {
    goto error;
  }

  switch (op->type) {
    case NR_OBJECT_INT:
    case NR_OBJECT_BOOLEAN:
      iret = op->u.ival;
      break;

    case NR_OBJECT_LONG:
      iret = (int)op->u.lval;
      break;

    case NR_OBJECT_ULONG:
      // This is a horrible narrowing cast that should never be used.
      iret = (int)op->u.ulval;
      break;

    case NR_OBJECT_DOUBLE:
      iret = (int)op->u.dval;
      break;

    case NR_OBJECT_INVALID:
    case NR_OBJECT_NONE:
    case NR_OBJECT_STRING:
    case NR_OBJECT_JSTRING:
    case NR_OBJECT_HASH:
    case NR_OBJECT_ARRAY:
    default:
      goto error;
  }

  if (0 != errp) {
    *errp = NR_SUCCESS;
  }

  return iret;

error:
  if (0 != errp) {
    *errp = NR_FAILURE;
  }
  return -1;
}

int64_t nro_get_long(const nrobj_t* obj, nr_status_t* errp) {
  const nrintobj_t* op = (const nrintobj_t*)obj;

  if (0 == op) {
    goto error;
  }

  if (NR_OBJECT_LONG != op->type && NR_OBJECT_INT != obj->type) {
    goto error;
  }

  if (0 != errp) {
    *errp = NR_SUCCESS;
  }

  return (op->u.lval);

error:
  if (0 != errp) {
    *errp = NR_FAILURE;
  }
  return ((int64_t)-1);
}

uint64_t nro_get_ulong(const nrobj_t* obj, nr_status_t* errp) {
  const nrintobj_t* op = (const nrintobj_t*)obj;

  if (0 == op) {
    goto error;
  }

  // The signed types cannot be losslessly converted, so we won't try.
  if (NR_OBJECT_ULONG != op->type) {
    goto error;
  }

  if (0 != errp) {
    *errp = NR_SUCCESS;
  }

  return (op->u.ulval);

error:
  if (0 != errp) {
    *errp = NR_FAILURE;
  }
  return ((uint64_t)0);
}

double nro_get_double(const nrobj_t* obj, nr_status_t* errp) {
  const nrintobj_t* op = (const nrintobj_t*)obj;

  if (0 == op) {
    goto error;
  }

  if (NR_OBJECT_DOUBLE != op->type) {
    goto error;
  }

  if (0 != errp) {
    *errp = NR_SUCCESS;
  }

  return (op->u.dval);

error:
  if (0 != errp) {
    *errp = NR_FAILURE;
  }
  return (-1.0);
}

const char* nro_get_string(const nrobj_t* obj, nr_status_t* errp) {
  const nrintobj_t* op = (const nrintobj_t*)obj;

  if (0 == op) {
    goto error;
  }

  if (NR_OBJECT_STRING != op->type) {
    goto error;
  }

  if (0 != errp) {
    *errp = NR_SUCCESS;
  }

  return (op->u.sval);

error:
  if (0 != errp) {
    *errp = NR_FAILURE;
  }
  return ((const char*)0);
}

const char* nro_get_jstring(const nrobj_t* obj, nr_status_t* errp) {
  const nrintobj_t* op = (const nrintobj_t*)obj;

  if (0 == op) {
    goto error;
  }

  if (NR_OBJECT_JSTRING != op->type) {
    goto error;
  }

  if (0 != errp) {
    *errp = NR_SUCCESS;
  }

  return (op->u.sval);

error:
  if (0 != errp) {
    *errp = NR_FAILURE;
  }
  return ((const char*)0);
}

const nrobj_t* nro_get_hash_value(const nrobj_t* obj,
                                  const char* key,
                                  nr_status_t* errp) {
  const nrintobj_t* op = (const nrintobj_t*)obj;
  int idx;

  if (0 == op) {
    goto error;
  }

  if (NR_OBJECT_HASH != op->type) {
    goto error;
  }

  idx = nro_find_key_in_hash(op, key);
  if (-1 == idx) {
    goto error;
  }

  if (0 != errp) {
    *errp = NR_SUCCESS;
  }

  if (-2 == idx) {
    return (nrobj_t*)0;
  }

  return (op->u.hval.data[idx]);

error:
  if (0 != errp) {
    *errp = NR_FAILURE;
  }
  return 0;
}

const nrobj_t* nro_get_hash_value_by_index(const nrobj_t* obj,
                                           int idx,
                                           nr_status_t* errp,
                                           const char** keyp) {
  const nrintobj_t* op = (const nrintobj_t*)obj;

  if (0 == op) {
    goto error;
  }

  if (NR_OBJECT_HASH != op->type) {
    goto error;
  }

  if ((idx < 1) || (idx > op->u.hval.size)) {
    goto error;
  }

  /* Get back to 0-based */
  idx--;

  if (0 != errp) {
    *errp = NR_SUCCESS;
  }

  if (keyp) {
    *keyp = op->u.hval.keys[idx];
  }

  return (op->u.hval.data[idx]);

error:
  if (0 != errp) {
    *errp = NR_FAILURE;
  }
  return 0;
}

const nrobj_t* nro_get_array_value(const nrobj_t* array,
                                   int idx,
                                   nr_status_t* errp) {
  const nrintobj_t* op = (const nrintobj_t*)array;

  if (0 == op) {
    goto error;
  }

  if (NR_OBJECT_ARRAY != op->type) {
    goto error;
  }

  if ((idx < 1) || ((idx - 1) >= op->u.aval.size)) {
    goto error;
  }

  if (0 != errp) {
    *errp = NR_SUCCESS;
  }

  return (op->u.aval.data[idx - 1]);

error:
  if (0 != errp) {
    *errp = NR_FAILURE;
  }
  return 0;
}

static void add_obj_jfmt(nrbuf_t* buf, const char* fmt, ...) {
  char tbuf[1024];
  char* tmp = tbuf;
  va_list ap;
  int reqlen;

  va_start(ap, fmt);
  reqlen = vsnprintf(tbuf, sizeof(tbuf), fmt, ap);
  va_end(ap);

  if (nrunlikely(reqlen >= (int)sizeof(tbuf))) {
    tmp = (char*)nr_alloca(reqlen + 2);
    va_start(ap, fmt);
    reqlen = vsnprintf(tmp, reqlen + 1, fmt, ap);
    va_end(ap);
  }

  nr_buffer_add(buf, tmp, reqlen);
}

static void add_obj_double(nrbuf_t* buf, const double d) {
  char tbuf[1024];
  int reqlen;

  reqlen = nr_double_to_str(tbuf, sizeof(tbuf), d);

  nr_buffer_add(buf, tbuf, reqlen);
}

static void recursive_obj_to_json(const nrintobj_t* op, nrbuf_t* buf) {
  int l;
  int i;

  if (0 == op) {
    return;
  }

  switch (op->type) {
    case NR_OBJECT_INVALID:
    case NR_OBJECT_NONE:
      nr_buffer_add(buf, "null", 4);
      break;

    case NR_OBJECT_BOOLEAN:
      if (op->u.ival) {
        nr_buffer_add(buf, "true", 4);
      } else {
        nr_buffer_add(buf, "false", 5);
      }
      break;

    case NR_OBJECT_INT:
      add_obj_jfmt(buf, "%d", op->u.ival);
      break;

    case NR_OBJECT_LONG:
      add_obj_jfmt(buf, "%lld", (long long int)op->u.lval);
      break;

    case NR_OBJECT_ULONG:
      add_obj_jfmt(buf, NR_UINT64_FMT, op->u.ulval);
      break;

    case NR_OBJECT_DOUBLE:
      add_obj_double(buf, op->u.dval);
      break;

    case NR_OBJECT_STRING:
      nr_buffer_add_escape_json(buf, op->u.sval);
      break;

    case NR_OBJECT_JSTRING:
      l = nr_strlen(op->u.sval);
      nr_buffer_add(buf, op->u.sval, l);
      break;

    case NR_OBJECT_HASH:
      nr_buffer_add(buf, "{", 1);

      for (i = 0; i < op->u.hval.size; i++) {
        nr_buffer_add_escape_json(buf, op->u.hval.keys[i]);
        nr_buffer_add(buf, ":", 1);
        recursive_obj_to_json(op->u.hval.data[i], buf);

        if (i != op->u.hval.size - 1) {
          nr_buffer_add(buf, ",", 1);
        }
      }
      nr_buffer_add(buf, "}", 1);
      break;

    case NR_OBJECT_ARRAY:
      nr_buffer_add(buf, "[", 1);

      for (i = 0; i < op->u.aval.size; i++) {
        recursive_obj_to_json(op->u.aval.data[i], buf);

        if (i != op->u.aval.size - 1) {
          nr_buffer_add(buf, ",", 1);
        }
      }
      nr_buffer_add(buf, "]", 1);
      break;
  }
}

char* nro_to_json(const nrobj_t* obj) {
  const nrintobj_t* op = (const nrintobj_t*)obj;
  nrbuf_t* buf;
  char* ret;

  buf = nr_buffer_create(4096, 4096);

  if (0 == op) {
    nr_buffer_add(buf, "null", 4);
  } else {
    recursive_obj_to_json(op, buf);
  }

  nr_buffer_add(buf, "\0", 1);

  ret = nr_strdup((const char*)nr_buffer_cptr(buf));
  nr_buffer_destroy(&buf);

  return ret;
}

nr_status_t nro_to_json_buffer(const nrobj_t* obj, nrbuf_t* buf) {
  const nrintobj_t* op = (const nrintobj_t*)obj;

  if (NULL == buf) {
    return NR_FAILURE;
  }

  if (0 == op) {
    nr_buffer_add(buf, "null", 4);
  } else {
    recursive_obj_to_json(op, buf);
  }

  return NR_SUCCESS;
}

/*
 * This parser is from cJSON.
 *
 * Parse the input text to generate a number, and populate the result into item.
 * The decimal radix separator is always '.', regardless of the current locale.
 *
 * ulong support is intentionally not added here: JSON has no concept of
 * unsigned numbers, so we're not going to parse any JSON with that assumption.
 */
static const char* parse_number(nrintobj_t* item, const char* num) {
  double d;
  char* ret = 0;
  long i;

  i = strtol(num, &ret, 0);
  if (ret && (('.' == *ret) || ('e' == *ret) || ('E' == *ret))) {
    ret = 0;
    d = nr_strtod(num, &ret);
    if ((HUGE_VAL == d) || (-HUGE_VAL == d)) {
      int64_t l = (int64_t)strtoll(num, &ret, 0);
      item->type = NR_OBJECT_LONG;
      item->u.lval = l;
    } else {
      item->type = NR_OBJECT_DOUBLE;
      item->u.dval = d;
    }
  } else {
    if ((i <= INT_MIN) || (i >= INT_MAX)) {
      int64_t l = (int64_t)strtoll(num, &ret, 0);
      item->type = NR_OBJECT_LONG;
      item->u.lval = l;
    } else {
      item->type = NR_OBJECT_INT;
      item->u.ival = (int)i;
    }
  }

  if (0 != ret) {
    num = ret;
  }

  return num;
}

static const unsigned char firstByteMark[7]
    = {0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};

static const char* parse_string(nrintobj_t* item, const char* str) {
  const char* ptr = str + 1;
  char* ptr2;
  char* out;
  int len = 0;
  unsigned uc;

  if (0 == str) {
    return 0;
  }

  if (*str != '\"') {
    return 0; /* not a string! */
  }

  for (len = 0; *ptr != '\"'; len++, ptr++) {
    if ('\\' == *ptr) {
      ptr++; /* Skip escaped quotes. */
    } else if ((unsigned char)*ptr < 32) {
      return 0; /* Not a valid string! */
    }
  }

  out = (char*)nr_malloc(
      len + 1); /* This is how long we need for the string, roughly. */

  ptr = str + 1;
  ptr2 = out;
  while (*ptr != '\"' && (unsigned char)*ptr > 31) {
    if (*ptr != '\\') {
      *ptr2++ = *ptr++;
    } else {
      ptr++;
      switch (*ptr) {
        case 'b':
          *ptr2++ = '\b';
          break;

        case 'f':
          *ptr2++ = '\f';
          break;

        case 'n':
          *ptr2++ = '\n';
          break;

        case 'r':
          *ptr2++ = '\r';
          break;

        case 't':
          *ptr2++ = '\t';
          break;

        case '/':
          *ptr2++ = '/';
          break;

        case 'u': /* transcode utf16 to utf8. DOES NOT SUPPORT SURROGATE PAIRS
                     CORRECTLY. */
          sscanf(ptr + 1, "%4x", &uc); /* get the unicode char. */
          len = 3;
          if (uc < 0x80) {
            len = 1;
          } else if (uc < 0x800) {
            len = 2;
          }
          ptr2 += len;

          switch (len) {
            case 3:
              *--ptr2 = ((uc | 0x80) & 0xBF);
              uc >>= 6;
              /*FALLTHROUGH*/

            case 2:
              *--ptr2 = ((uc | 0x80) & 0xBF);
              uc >>= 6;
              /*FALLTHROUGH*/

            case 1:
              *--ptr2 = (uc | firstByteMark[len]);
              break;
          }
          ptr2 += len;
          ptr += 4;
          break;

        default:
          *ptr2++ = *ptr;
          break;
      }
      ptr++;
    }
  }

  *ptr2 = 0;
  if (*ptr == '\"') {
    ptr++;
  }

  item->type = NR_OBJECT_STRING;
  item->u.sval = nr_strdup(out);
  nr_free(out);
  return ptr;
}

/* Predeclare these prototypes. */
static const char* parse_value(nrintobj_t* item, const char* value);
static const char* parse_array(nrintobj_t* item, const char* value);
static const char* parse_object(nrintobj_t* item, const char* value);

/* Utility to jump whitespace and cr/lf */
static const char* json_skip(const char* in) {
  while (in && *in && ((unsigned char)*in <= 32)) {
    in++;
  }
  return in;
}

nrobj_t* nro_create_from_json(const char* json) {
  const char* next;
  nrintobj_t* rv;

  if ((0 == json) || (0 == json[0])) {
    return 0;
  }

  rv = (nrintobj_t*)nr_zalloc(sizeof(nrintobj_t));

  next = parse_value(rv, json_skip(json));
  if (0 == next) {
    nro_internal_delete(rv, 1);
    return 0;
  } else if (!('\0' == *next || '\0' == *json_skip(next))) {
    nro_internal_delete(rv, 1);
    return 0;
  }

  return (nrobj_t*)rv;
}

nrobj_t* nro_create_from_json_unterminated(const char* json, int len) {
  nrobj_t* obj;
  char* terminated;

  if (len <= 0) {
    return NULL;
  }
  if (NULL == json) {
    return NULL;
  }

  terminated = (char*)nr_malloc(len + 1);
  terminated[0] = 0;
  nr_strxcpy(terminated, json, len);
  obj = nro_create_from_json(terminated);
  nr_free(terminated);

  return obj;
}

/* Parser core - when encountering text, process appropriately. */
static const char* parse_value(nrintobj_t* item, const char* value) {
  if (!value) {
    return 0; /* Fail on null. */
  }

  if (!nr_strncmp(value, "null", 4)) {
    item->type = NR_OBJECT_NONE;
    return value + 4;
  }

  if (!nr_strncmp(value, "false", 5)) {
    item->type = NR_OBJECT_BOOLEAN;
    item->u.ival = 0;
    return value + 5;
  }

  if (!nr_strncmp(value, "true", 4)) {
    item->type = NR_OBJECT_BOOLEAN;
    item->u.ival = 1;
    return value + 4;
  }

  if (*value == '\"') {
    return parse_string(item, value);
  }

  if ((*value == '-') || ((*value >= '0') && (*value <= '9'))) {
    return parse_number(item, value);
  }

  if (*value == '[') {
    return parse_array(item, value);
  }

  if (*value == '{') {
    return parse_object(item, value);
  }

  return 0; /* failure. */
}

/* Build an array from input text. */
static const char* parse_array(nrintobj_t* item, const char* value) {
  nrintobj_t* child;

  if (!value) {
    return 0;
  }

  if (*value != '[') {
    return 0; /* not an array! */
  }

  value = json_skip(value + 1);

  item->type = NR_OBJECT_ARRAY;
  item->u.aval.size = 0;
  if (*value == ']') {
    item->u.aval.allocated = 1;
  } else {
    item->u.aval.allocated = NRO_CHUNK_SIZE;
  }
  item->u.aval.data
      = (nrintobj_t**)nr_calloc(item->u.aval.allocated, sizeof(nrintobj_t*));

  if (*value == ']') {
    return value + 1; /* empty array. */
  }

  child = (nrintobj_t*)nr_zalloc(sizeof(nrintobj_t));
  value = json_skip(parse_value(
      child, json_skip(value))); /* json_skip any spacing, get the value. */
  if (!value) {
    nro_internal_delete(item, 0);
    nro_internal_delete(child, 1);
    return 0;
  }
  nro_internal_setvalue_array(item, 0, child);

  while (*value == ',') {
    child = (nrintobj_t*)nr_zalloc(sizeof(nrintobj_t));

    value = json_skip(parse_value(child, json_skip(value + 1)));
    if (!value) {
      nro_internal_delete(item, 0);
      nro_internal_delete(child, 1);
      return 0; /* memory fail */
    }
    nro_internal_setvalue_array(item, 0, child);
  }

  if (*value == ']') {
    return value + 1; /* end of array */
  }

  nro_internal_delete(item, 0);
  return 0; /* malformed. */
}

static const char* parse_object(nrintobj_t* item, const char* value) {
  nrintobj_t* child;
  char* key;

  if (!value) {
    return 0;
  }

  if (*value != '{') {
    return 0; /* not an object! */
  }

  value = json_skip(value + 1);

  item->type = NR_OBJECT_HASH;
  item->u.hval.size = 0;
  if (*value == '}') {
    item->u.hval.allocated = 1;
  } else {
    item->u.hval.allocated = NRO_CHUNK_SIZE;
  }
  item->u.hval.data
      = (nrintobj_t**)nr_calloc(item->u.aval.allocated, sizeof(nrintobj_t*));
  item->u.hval.keys = (char**)nr_calloc(item->u.aval.allocated, sizeof(char*));

  if (*value == '}') {
    return value + 1; /* empty array. */
  }

  child = (nrintobj_t*)nr_zalloc(sizeof(nrintobj_t));
  value = json_skip(parse_string(child, json_skip(value)));
  if (!value) {
    nro_internal_delete(item, 0);
    nro_internal_delete(child, 1);
    return 0;
  }
  child->type = NR_OBJECT_NONE;
  key = child->u.sval;
  child->u.sval = 0;

  if (*value != ':') {
    nr_free(key);
    nr_free(child);
    nro_internal_delete(item, 0);
    return 0; /* fail! */
  }

  value = json_skip(parse_value(
      child, json_skip(value + 1))); /* Skip any spacing, get the value. */
  if (!value) {
    nr_free(key);
    nro_internal_delete(item, 0);
    nro_internal_delete(child, 1);
    return 0;
  }
  nro_internal_setvalue_hash(item, key, child);
  nr_free(key);

  while (*value == ',') {
    child = (nrintobj_t*)nr_zalloc(sizeof(nrintobj_t));

    value = json_skip(parse_string(child, json_skip(value + 1)));
    if (!value) {
      nro_internal_delete(item, 0);
      nro_internal_delete(child, 1);
      return 0;
    }
    key = child->u.sval;
    child->type = NR_OBJECT_NONE;

    if (*value != ':') {
      nr_free(key);
      nr_free(child);
      nro_internal_delete(item, 0);
      return 0; /* fail! */
    }

    value = json_skip(parse_value(
        child,
        json_skip(value + 1))); /* json_skip any spacing, get the value. */
    if (!value) {
      nr_free(key);
      nro_internal_delete(item, 0);
      nro_internal_delete(child, 1);
      return 0;
    }
    nro_internal_setvalue_hash(item, key, child);
    nr_free(key);
  }

  if (*value == '}') {
    return value + 1; /* end of array */
  }

  return 0; /* malformed. */
}

int nro_find_array_int(const nrobj_t* array, int x) {
  int i;
  const nrintobj_t* obj = (const nrintobj_t*)array;

  if ((0 == obj) || (NR_OBJECT_ARRAY != obj->type)) {
    return -1;
  }

  for (i = 0; i < obj->u.aval.size; i++) {
    const nrintobj_t* op = obj->u.aval.data[i];

    if ((0 != op) && (NR_OBJECT_INT == op->type) && (x == op->u.ival)) {
      return i + 1;
    }
  }

  return -1;
}

static const char* spaces
    = "                                                "
      "                                                                        "
      "    "
      "                                                                        "
      "    "
      "                                                                        "
      "    "
      "                                                                        "
      "   ";

static void nro_dump_internal(const nrobj_t* obj, int level, char* retstr) {
  const nrintobj_t* op = (const nrintobj_t*)obj;
  int i;
  char tmpstr[1024]; /* we don't expect recursion to be deep */

  if (0 == op) {
    return;
  }

  if (0 == level) {
    snprintf(tmpstr, sizeof(tmpstr), "Object Dump (%d):\n", op->type);
    nr_strcat(retstr, tmpstr);
  }

  snprintf(tmpstr, sizeof(tmpstr), "  ");
  nr_strcat(retstr, tmpstr);

  if (0 != level) {
    snprintf(tmpstr, sizeof(tmpstr), "%.*s", level * 2, spaces);
    nr_strcat(retstr, tmpstr);
  }

  switch (op->type) {
    case NR_OBJECT_INVALID:
      snprintf(tmpstr, sizeof(tmpstr), "INVALID\n");
      nr_strcat(retstr, tmpstr);
      break;

    case NR_OBJECT_NONE:
      snprintf(tmpstr, sizeof(tmpstr), "NONE\n");
      nr_strcat(retstr, tmpstr);
      break;

    case NR_OBJECT_BOOLEAN:
      snprintf(tmpstr, sizeof(tmpstr), "BOOLEAN: %d\n", op->u.ival);
      nr_strcat(retstr, tmpstr);
      break;

    case NR_OBJECT_INT:
      snprintf(tmpstr, sizeof(tmpstr), "INT: %d\n", op->u.ival);
      nr_strcat(retstr, tmpstr);
      break;

    case NR_OBJECT_LONG:
      snprintf(tmpstr, sizeof(tmpstr), "LONG: %lld\n", (long long)op->u.lval);
      nr_strcat(retstr, tmpstr);
      break;

    case NR_OBJECT_ULONG:
      snprintf(tmpstr, sizeof(tmpstr), "ULONG: " NR_UINT64_FMT "\n",
               op->u.ulval);
      nr_strcat(retstr, tmpstr);
      break;

    case NR_OBJECT_DOUBLE:
      snprintf(tmpstr, sizeof(tmpstr), "DOUBLE: %lf\n", op->u.dval);
      nr_strcat(retstr, tmpstr);
      break;

    case NR_OBJECT_STRING:
      snprintf(tmpstr, sizeof(tmpstr), "STRING: >>>%.900s<<<\n",
               op->u.sval ? op->u.sval : "(NULL)");
      nr_strcat(retstr, tmpstr);
      break;

    case NR_OBJECT_JSTRING:
      snprintf(tmpstr, sizeof(tmpstr), "JSTRING: >>>%.900s<<<\n",
               op->u.sval ? op->u.sval : "(NULL)");
      nr_strcat(retstr, tmpstr);
      break;

    case NR_OBJECT_ARRAY:
      snprintf(tmpstr, sizeof(tmpstr), "ARRAY: size=%d allocated=%d\n",
               op->u.aval.size, op->u.aval.allocated);
      nr_strcat(retstr, tmpstr);
      for (i = 0; i < op->u.aval.size; i++) {
        snprintf(tmpstr, sizeof(tmpstr), "  ");
        nr_strcat(retstr, tmpstr);
        if (0 != level) {
          snprintf(tmpstr, sizeof(tmpstr), "%.*s", level * 2, spaces);
          nr_strcat(retstr, tmpstr);
        }
        snprintf(tmpstr, sizeof(tmpstr), "[%d] = {\n", i + 1);
        nr_strcat(retstr, tmpstr);
        nro_dump_internal(op->u.aval.data[i], level + 1, retstr);
        snprintf(tmpstr, sizeof(tmpstr), "  ");
        nr_strcat(retstr, tmpstr);
        if (0 != level) {
          snprintf(tmpstr, sizeof(tmpstr), "%.*s", level * 2, spaces);
          nr_strcat(retstr, tmpstr);
        }
        snprintf(tmpstr, sizeof(tmpstr), "}\n");
        nr_strcat(retstr, tmpstr);
      }
      break;

    case NR_OBJECT_HASH:
      snprintf(tmpstr, sizeof(tmpstr), "HASH: size=%d allocated=%d\n",
               op->u.hval.size, op->u.hval.allocated);
      nr_strcat(retstr, tmpstr);
      for (i = 0; i < op->u.hval.size; i++) {
        snprintf(tmpstr, sizeof(tmpstr), "  ");
        nr_strcat(retstr, tmpstr);
        if (0 != level) {
          snprintf(tmpstr, sizeof(tmpstr), "%.*s", level * 2, spaces);
          nr_strcat(retstr, tmpstr);
        }
        snprintf(tmpstr, sizeof(tmpstr), "['%.900s'] = {\n",
                 op->u.hval.keys[i]);
        nr_strcat(retstr, tmpstr);
        nro_dump_internal(op->u.hval.data[i], level + 1, retstr);
        snprintf(tmpstr, sizeof(tmpstr), "  ");
        nr_strcat(retstr, tmpstr);
        if (0 != level) {
          snprintf(tmpstr, sizeof(tmpstr), "%.*s", level * 2, spaces);
          nr_strcat(retstr, tmpstr);
        }
        snprintf(tmpstr, sizeof(tmpstr), "}\n");
        nr_strcat(retstr, tmpstr);
      }
      break;
  }
}

char* nro_dump(const nrobj_t* obj) {
  char* str;

  if (0 == obj) {
    return nr_strdup("(NULL)");
  }

  str = (char*)nr_calloc(1, 8192);
  nro_dump_internal(obj, 0, str);
  str = (char*)nr_realloc(str, nr_strlen(str) + 1);

  return str;
}

static char* stringify_boolean(const nrobj_t* obj) {
  nr_status_t err;
  nrbuf_t* buf;
  char* ret;
  int ival = nro_get_ival(obj, &err);

  buf = nr_buffer_create(1024, 1024);

  add_obj_jfmt(buf, "%d", ival);
  nr_buffer_add(buf, "\0", 1);
  ret = nr_strdup((const char*)nr_buffer_cptr(buf));
  nr_buffer_destroy(&buf);

  return ret;
}

char* nro_stringify(const nrobj_t* found) {
  nrotype_t found_type = nro_type(found);
  nrbuf_t* buf;
  char* tmp;
  char* ret;

  buf = nr_buffer_create(1024, 1024);

  if (NR_OBJECT_BOOLEAN == found_type) {
    tmp = stringify_boolean(found);
  } else {
    tmp = nro_to_json(found);
  }

  nr_buffer_add_escape_json(buf, tmp);
  nr_buffer_add(buf, "\0", 1);

  ret = nr_strdup((const char*)nr_buffer_cptr(buf));

  nr_buffer_destroy(&buf);
  nr_free(tmp);

  return ret;
}
