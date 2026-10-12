/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "nr_axiom.h"
#include "php_includes.h"
#include "php_agent.h"
#include "php_objtable.h"
#include "util_hashmap.h"
#include "util_memory.h"

#ifdef NR_PHP_OBJTABLE_WEAK
#include "zend_weakrefs.h"
#endif

typedef struct {
  nr_php_objtable_t* table;
  uint64_t key;
  void* value;
} nr_php_objtable_entry_t;

struct _nr_php_objtable_t {
  void (*dtor)(void* value);

  /*
   * Maps the key of an object to its entry. The key is the object's address
   * for the weak backend, so it is never reused while the entry exists, and the
   * object's handle for the handle map backend.
   *
   * For the weak backend the entries are borrowed: the weak table below owns
   * them. For the handle map backend this map owns them.
   */
  nr_hashmap_t* index;

#ifdef NR_PHP_OBJTABLE_WEAK
  /*
   * Owns the entries and is registered with the engine, which removes an entry
   * when its object is freed. Persistent, because it is destroyed in
   * post-deactivate, after the request's memory is gone.
   */
  HashTable weak;
#endif
};

static void nr_php_objtable_entry_free(nr_php_objtable_entry_t* entry) {
  if (NULL == entry) {
    return;
  }
  if (entry->table->dtor) {
    entry->table->dtor(entry->value);
  }
  nr_free(entry);
}

#ifdef NR_PHP_OBJTABLE_WEAK

/*
 * Called by the engine when the object is freed or the entry is deleted, and by
 * zend_hash_destroy(). Only frees memory: no PHP calls.
 */
static void nr_php_objtable_weak_dtor(zval* zv) {
  nr_php_objtable_entry_t* entry = Z_PTR_P(zv);

  nr_hashmap_index_delete(entry->table->index, entry->key);
  nr_php_objtable_entry_free(entry);
}

static uint64_t nr_php_objtable_key(const zval* obj) {
  return (uint64_t)(uintptr_t)Z_OBJ_P((zval*)obj);
}

#else

static uint64_t nr_php_objtable_key(const zval* obj) {
  return (uint64_t)Z_OBJ_HANDLE_P((zval*)obj);
}

#endif

nr_php_objtable_t* nr_php_objtable_create(void (*dtor)(void* value)) {
  nr_php_objtable_t* table = nr_zalloc(sizeof(nr_php_objtable_t));

  table->dtor = dtor;
#ifdef NR_PHP_OBJTABLE_WEAK
  table->index = nr_hashmap_create(NULL);
  zend_hash_init(&table->weak, 8, NULL, nr_php_objtable_weak_dtor, 1);
#else
  table->index
      = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_php_objtable_entry_free);
#endif

  return table;
}

void nr_php_objtable_destroy(nr_php_objtable_t** table_ptr) {
  nr_php_objtable_t* table;

  if (NULL == table_ptr || NULL == *table_ptr) {
    return;
  }
  table = *table_ptr;

#ifdef NR_PHP_OBJTABLE_WEAK
  /*
   * The entries remove themselves from the index, so the index goes last. This
   * does not call the weak reference API: the engine has already dropped its
   * registrations by the time this runs.
   */
  zend_hash_destroy(&table->weak);
#endif
  nr_hashmap_destroy(&table->index);
  nr_realfree((void**)table_ptr);
}

void nr_php_objtable_del(nr_php_objtable_t* table, const zval* obj) {
  if (NULL == table || NULL == obj || IS_OBJECT != Z_TYPE_P(obj)) {
    return;
  }

#ifdef NR_PHP_OBJTABLE_WEAK
  /* Runs the dtor above, which removes the index entry. */
  zend_weakrefs_hash_del(&table->weak, Z_OBJ_P((zval*)obj));
#else
  nr_hashmap_index_delete(table->index, nr_php_objtable_key(obj));
#endif
}

void nr_php_objtable_set(nr_php_objtable_t* table,
                         const zval* obj,
                         void* value) {
  nr_php_objtable_entry_t* entry;

  if (NULL == table || NULL == obj) {
    return;
  }

  if (IS_OBJECT != Z_TYPE_P(obj)) {
    if (table->dtor) {
      table->dtor(value);
    }
    return;
  }

  entry = nr_zalloc(sizeof(nr_php_objtable_entry_t));
  entry->table = table;
  entry->key = nr_php_objtable_key(obj);
  entry->value = value;

#ifdef NR_PHP_OBJTABLE_WEAK
  {
    zval zv;

    /* zend_weakrefs_hash_add() fails if the object already has an entry. */
    nr_php_objtable_del(table, obj);

    ZVAL_PTR(&zv, entry);
    if (NULL
        == zend_weakrefs_hash_add(&table->weak, Z_OBJ_P((zval*)obj), &zv)) {
      nr_php_objtable_entry_free(entry);
      return;
    }
    nr_hashmap_index_update(table->index, entry->key, entry);
  }
#else
  /* The update frees the entry it replaces. */
  nr_hashmap_index_update(table->index, entry->key, entry);
#endif
}

void* nr_php_objtable_get(nr_php_objtable_t* table, const zval* obj) {
  nr_php_objtable_entry_t* entry;

  if (NULL == table || NULL == obj || IS_OBJECT != Z_TYPE_P(obj)) {
    return NULL;
  }

  entry = nr_hashmap_index_get(table->index, nr_php_objtable_key(obj));
  if (NULL == entry) {
    return NULL;
  }

  return entry->value;
}

size_t nr_php_objtable_count(const nr_php_objtable_t* table) {
  if (NULL == table) {
    return 0;
  }

  return nr_hashmap_count(table->index);
}
