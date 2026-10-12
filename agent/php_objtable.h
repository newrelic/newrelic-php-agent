/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * A table that associates a value with a PHP object and drops the value when
 * the object is freed.
 *
 * Why this exists: the agent keys some data on an object handle or address.
 * PHP reuses both once an object is freed, so a later object can find the data
 * of an earlier one. Entries in this table are tied to the object's life
 * instead.
 *
 * Two backends:
 *
 * - Weak (PHP 8.0 and later): entries are registered with the engine's weak
 *   reference machinery, so an entry is removed, and its value destroyed, at
 *   the moment the object is freed.
 *
 * - Handle map (PHP 7.x): there is no weak API, so entries are keyed by object
 *   handle and are replaced only by the next set() on the same handle. A stale
 *   entry cannot be told from a live one. Consumers that only read must not
 *   use this table on these versions.
 *
 * Rules:
 *
 * - Create a table in RINIT and destroy it only in post-deactivate, after the
 *   engine has freed its objects. Never destroy a table earlier: an object
 *   freed later would notify a freed table.
 *
 * - The value destructor can run inside an object's free. It must only free
 *   memory and must not call into PHP.
 */
#ifndef PHP_OBJTABLE_HDR
#define PHP_OBJTABLE_HDR

#include "php_includes.h"

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
#define NR_PHP_OBJTABLE_WEAK 1
#endif

typedef struct _nr_php_objtable_t nr_php_objtable_t;

/*
 * Purpose : Create a table.
 *
 * Params  : 1. The destructor for a stored value. Can be NULL.
 *
 * Returns : A new table, to be destroyed with nr_php_objtable_destroy().
 */
extern nr_php_objtable_t* nr_php_objtable_create(void (*dtor)(void* value));

/*
 * Purpose : Destroy a table and every value in it. Only call this in
 *           post-deactivate (see above). No weak reference API is called.
 */
extern void nr_php_objtable_destroy(nr_php_objtable_t** table_ptr);

/*
 * Purpose : Store a value for an object, replacing any value already stored
 *           for it.
 *
 * Params  : 1. The table.
 *           2. The object. Anything that is not an object is rejected.
 *           3. The value. The table takes ownership: it is destroyed with the
 *              table's destructor when the entry is replaced, deleted or
 *              dropped, and also when it is rejected here.
 *
 * Notes   : Does nothing, and does not take ownership, for a NULL table or a
 *           NULL object.
 */
extern void nr_php_objtable_set(nr_php_objtable_t* table,
                                const zval* obj,
                                void* value);

/*
 * Purpose : Look up the value stored for an object.
 *
 * Returns : The value, which stays owned by the table, or NULL.
 */
extern void* nr_php_objtable_get(nr_php_objtable_t* table, const zval* obj);

/*
 * Purpose : Remove the entry for an object and destroy its value.
 */
extern void nr_php_objtable_del(nr_php_objtable_t* table, const zval* obj);

/*
 * Purpose : Mark the entry of an object as the last one, for callers that use
 *           the most recent object when none is given (a default link).
 *
 * Notes   : Does nothing if the object has no entry. The mark is not owned:
 *           it is cleared when the entry is replaced, deleted or dropped.
 */
extern void nr_php_objtable_mark_last(nr_php_objtable_t* table,
                                      const zval* obj);

/*
 * Purpose : Return the value of the entry marked as the last one.
 *
 * Returns : The value, which stays owned by the table, or NULL if no entry is
 *           marked.
 */
extern void* nr_php_objtable_get_last(nr_php_objtable_t* table);

/*
 * Purpose : Remove the entry marked as the last one and destroy its value.
 *           Does nothing if no entry is marked.
 */
extern void nr_php_objtable_del_last(nr_php_objtable_t* table);

/*
 * Purpose : Return the number of entries in the table.
 */
extern size_t nr_php_objtable_count(const nr_php_objtable_t* table);

#endif /* PHP_OBJTABLE_HDR */
