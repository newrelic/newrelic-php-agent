/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_buffer.h"
#include "util_hash.h"
#include "util_memory.h"
#include "util_string_pool.h"
#include "util_strings.h"

/*
 * These constants are used for sanity checking when reading a string
 * pool from a buffer.
 */
#define NR_STRPOOL_SANITY_MAX_USED (1000 * 1000)
#define NR_STRPOOL_SANITY_MAX_LENGTH (1000 * 1000)
#define NR_STRPOOL_SANITY_MAX_STRING_BYTES (1000 * 1000 * 100)

typedef struct _nrstable_t {
  struct _nrstable_t* next; /* Linked list pointer to next table */
  int num_bytes_used;       /* Number of string bytes contained in this table */
  int num_bytes_allocated;  /* Total number of bytes allocated in this table */
  char bytes[1];            /* Beginning of string storage bytes */
} nrstable_t;

typedef struct _nrstring_t {
  uint32_t hash; /* String hash */
  int length;    /* String length */
  int left;      /* Binary tree left child */
  int right;     /* Binary tree right child */
} nrstring_t;

typedef struct _nrstrpool_t {
  int num_entries;     /* Number of strings in the pool */
  int size;            /* Current max allocated space in pool */
  nrstring_t* entries; /* One entry for each string in the pool */
  char** strings;      /* Pointers to stored strings. Separated from entries to
                          minimize buffer use */
  nrstable_t* tables;  /* Linked list of tables containing the strings */
} nrstrpool_t;

int nr_string_len(const nrstrpool_t* pool, int idx) {
  if (nrunlikely((0 == pool) || (idx <= 0) || (idx > pool->num_entries))) {
    return -1;
  }
  return pool->entries[idx - 1].length;
}

uint32_t nr_string_hash(const nrstrpool_t* pool, int idx) {
  if (nrunlikely((0 == pool) || (idx <= 0) || (idx > pool->num_entries))) {
    return 0;
  }
  return pool->entries[idx - 1].hash;
}

const char* nr_string_get(const nrstrpool_t* pool, int idx) {
  if (nrunlikely((0 == pool) || (idx <= 0) || (idx > pool->num_entries))) {
    return 0;
  }
  return pool->strings[idx - 1];
}

nrpool_t* nr_string_pool_create(void) {
  nrstrpool_t* pool = (nrstrpool_t*)nr_zalloc(sizeof(nrstrpool_t));

  pool->num_entries = 0;
  pool->size = NR_STRPOOL_STARTING_SIZE;
  pool->entries = (nrstring_t*)nr_zalloc(sizeof(nrstring_t) * pool->size);
  pool->strings = (char**)nr_zalloc(sizeof(char*) * pool->size);
  pool->tables = 0;

  return pool;
}

void nr_string_pool_destroy(nrpool_t** poolptr) {
  nrstrpool_t* pool;
  nrstable_t* table;

  if ((0 == poolptr) || (0 == *poolptr)) {
    return;
  }

  pool = *poolptr;
  table = pool->tables;

  while (table) {
    nrstable_t* next = table->next;

    nr_free(table);
    table = next;
  }

  nr_free(pool->entries);
  nr_free(pool->strings);
  nr_memset(pool, 0, sizeof(nrstrpool_t));
  nr_realfree((void**)poolptr);
}

static int nr_string_find_internal(const nrstrpool_t* pool,
                                   const char* string,
                                   uint32_t hash,
                                   int length) {
  int idx = 1;

  if (nrunlikely((0 == pool) || (0 == string) || (length < 0))) {
    return 0;
  }

  while (idx > 0) {
    nrstring_t* entry = &pool->entries[idx - 1];

    if ((hash == entry->hash) && (length == entry->length)
        && (0 == nr_strcmp(string, pool->strings[idx - 1]))) {
      return idx;
    }

    if (entry->hash < hash) {
      idx = entry->left;
    } else {
      idx = entry->right;
    }
  }
  return 0;
}

int nr_string_find(const nrstrpool_t* pool, const char* string) {
  int length = 0;
  uint32_t hash = nr_mkhash(string, &length);

  return nr_string_find_internal(pool, string, hash, length);
}

int nr_string_find_with_hash(const nrstrpool_t* pool,
                             const char* string,
                             uint32_t hash) {
  int length = nr_strlen(string);

  return nr_string_find_internal(pool, string, hash, length);
}

int nr_string_find_with_hash_length(const nrstrpool_t* pool,
                                    const char* string,
                                    uint32_t hash,
                                    int length) {
  return nr_string_find_internal(pool, string, hash, length);
}

/*
 * IMPORTANT : The string pool indices start at 1.  The transaction trace
 * JSON formatter assumes this, and therefore the indices should not be
 * changed without checking all consumers.
 */
static int nr_string_add_internal(nrstrpool_t* pool,
                                  const char* string,
                                  uint32_t hash,
                                  int length) {
  int idx;
  int new_string;
  nrstable_t* table;

  if (nrunlikely((0 == pool) || (0 == string) || (length < 0))) {
    return 0;
  }

  if (pool->size == pool->num_entries) {
    pool->size += NR_STRPOOL_INCREASE_SIZE;
    pool->entries = (nrstring_t*)nr_realloc(pool->entries,
                                            pool->size * sizeof(nrstring_t));
    pool->strings
        = (char**)nr_realloc(pool->strings, pool->size * sizeof(char*));
  }

  if (pool->num_entries) {
    idx = 1;
    for (;;) {
      /*
       * Though this loop appears to be a copy of the loop within find_internal
       * it is not: This loop must stop at a match if one exists, or otherwise
       * the closest match (to be the parent of the new node).
       */
      int next;
      nrstring_t* entry = &pool->entries[idx - 1];

      if ((hash == entry->hash) && (length == entry->length)
          && (0 == nr_strcmp(string, pool->strings[idx - 1]))) {
        return idx;
      }

      if (entry->hash < hash) {
        next = entry->left;
      } else {
        next = entry->right;
      }

      if (0 == next) {
        break;
      }
      idx = next;
    }
  } else {
    idx = 0;
  }

  new_string = pool->num_entries;
  pool->num_entries++;
  pool->entries[new_string].hash = hash;
  pool->entries[new_string].length = length;
  pool->entries[new_string].left = 0;
  pool->entries[new_string].right = 0;

  table = pool->tables;
  if ((0 == table)
      || ((table->num_bytes_allocated - table->num_bytes_used)
          < (length + 1))) {
    nrstable_t* t;
    int required = length + 1;
    int size
        = (required > NR_STRPOOL_TABLE_SIZE) ? required : NR_STRPOOL_TABLE_SIZE;

    t = (nrstable_t*)nr_zalloc(sizeof(nrstable_t) + size);
    t->num_bytes_allocated = size;
    t->num_bytes_used = 0;
    t->next = pool->tables;
    pool->tables = t;
  }

  table = pool->tables;
  pool->strings[new_string] = table->bytes + table->num_bytes_used;
  nr_strcpy(table->bytes + table->num_bytes_used, string);
  table->num_bytes_used += length + 1;

  if (idx) {
    if (pool->entries[idx - 1].hash < hash) {
      pool->entries[idx - 1].left = new_string + 1;
    } else {
      pool->entries[idx - 1].right = new_string + 1;
    }
  }

  return new_string + 1;
}

int nr_string_add(nrstrpool_t* pool, const char* string) {
  int length = 0;
  uint32_t hash = nr_mkhash(string, &length);

  return nr_string_add_internal(pool, string, hash, length);
}

int nr_string_add_with_hash(nrpool_t* pool, const char* string, uint32_t hash) {
  int length = nr_strlen(string);

  return nr_string_add_internal(pool, string, hash, length);
}

int nr_string_add_with_hash_length(nrpool_t* pool,
                                   const char* string,
                                   uint32_t hash,
                                   int length) {
  return nr_string_add_internal(pool, string, hash, length);
}

char* nr_string_pool_to_json(const nrpool_t* pool) {
  int i;
  char* js;
  nrbuf_t* buf;

  if (0 == pool) {
    return 0;
  }

  buf = nr_buffer_create(20 * 1000, 0);

  nr_buffer_add(buf, "[", 1);

  for (i = 0; i < pool->num_entries; i++) {
    if (i > 0) {
      nr_buffer_add(buf, ",", 1);
    }

    nr_buffer_add_escape_json(buf, nr_string_get(pool, i + 1));
  }

  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, "\0", 1);
  js = nr_strdup((const char*)nr_buffer_cptr(buf));
  nr_buffer_destroy(&buf);

  return js;
}

void nr_string_pool_apply(const nrpool_t* pool,
                          nr_string_pool_apply_func_t apply_func,
                          void* user_data) {
  int i;

  if (NULL == pool) {
    return;
  }

  for (i = 0; i < pool->num_entries; i++) {
    (apply_func)(pool->strings[i], pool->entries[i].length, user_data);
  }
}
