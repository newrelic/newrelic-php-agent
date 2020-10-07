/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for string pooling.
 */
#ifndef UTIL_STRING_POOL_HDR
#define UTIL_STRING_POOL_HDR

#include <stdint.h>

/*
 * These constants determine the starting size of the string pool, and the
 * increments by which the string pool is enlarged.
 */
#define NR_STRPOOL_STARTING_SIZE 4096
#define NR_STRPOOL_INCREASE_SIZE 4096

/*
 * Strings are not individually malloced. Instead they are stored in tables.
 * This is the minimum table size.
 */
#define NR_STRPOOL_TABLE_SIZE 32768

/*
 * USAGE NOTES: STRING POOLS
 *
 * String pools are an attempt to reduce the memory consumption of code that
 * uses a lot of strings, especially when there are lots of duplicates, and to
 * make searching for strings a lot quicker by allowing code that would
 * normally do a lot of strcmp() calls to instead compare simple integers.  You
 * begin using a string pool by creating it with nr_string_pool_create().
 *
 * Once the pool has been created, you can add strings to the pool, and later
 * retrieve them. You add strings with nr_string_add(), providing it the opaque
 * pool as the first argument, and the string to add as the second.  If you
 * attempt to add the same string, it will always result in the same return
 * value.
 *
 * There are several things you need to be very careful of when using string
 * pools. The first is to understand that an actual string as returned by the
 * nr_string_get() function is *A CONSTANT STRING*. Under no circumstances ever
 * should these be cast to normal char* strings or modified in any way. Once a
 * string has been added to a pool it is there for the entire lifetime of the
 * pool and will never be freed, recycled, or modified in any way. Therefore,
 * be sure that using a string pool is to your best advantage. There are cases
 * where they are a clear win but do not use them indiscriminately.
 *
 * The second key point to string pools is that they are FAST. If you have a
 * large list of strings / names / whatevers that you find yourself comparing a
 * lot, that's an ideal candidate for string pools, as now to determine string
 * equality you simply compare two integers, rather than doing a much more
 * expensive strcmp() on the strings. Even if you don't need a full string
 * pool, you may benefit from the very fast string hashing algorithm, which is
 * exposed as an API call.
 *
 * Last point to remember is that string pools can save a lot of memory, but
 * they will retain *all* strings added to the pool until such time as the pool
 * is destroyed. Depending on the requirements of the usage, string pools may
 * not be appropriate if you have a relatively small number of strings that
 * need to be kept around for longer than you want to keep the entire pool
 * around.
 */

typedef struct _nrstrpool_t nrpool_t;

extern nrpool_t* nr_string_pool_create(void);

/*
 * Purpose : Add a string to the pool.
 *
 * Returns : The position of the string within the pool, or 0 on error.
 *
 * Note    : The string must be NULL-terminated, even if a length is provided.
 *           The string is always stored using strcpy. If a length is provided,
 *           it merely avoids a strlen.
 */
extern int nr_string_add(nrpool_t* pool, const char* string);
extern int nr_string_add_with_hash(nrpool_t* pool,
                                   const char* string,
                                   uint32_t hash);
extern int nr_string_add_with_hash_length(nrpool_t* pool,
                                          const char* string,
                                          uint32_t hash,
                                          int length);

/*
 * Purpose : Look for a string in the pool.
 *
 * Returns : The position of the string within the pool, or 0 if not found.
 */
extern int nr_string_find(const nrpool_t* pool, const char* string);
extern int nr_string_find_with_hash(const nrpool_t* pool,
                                    const char* string,
                                    uint32_t hash);
extern int nr_string_find_with_hash_length(const nrpool_t* pool,
                                           const char* string,
                                           uint32_t hash,
                                           int length);

/*
 * Purpose : Destroy the given string pool, releasing all its memory.
 */
extern void nr_string_pool_destroy(nrpool_t** poolptr);

/*
 * Purpose : Given a pooled string index, get its value, hash, or length
 */
extern const char* nr_string_get(const nrpool_t* pool, int idx);
extern int nr_string_len(const nrpool_t* pool, int idx);
extern uint32_t nr_string_hash(const nrpool_t* pool, int idx);

/*
 * Purpose : Write a string pool to JSON.
 *
 * Returns : A newly allocated JSON string, or NULL on error.
 */
extern char* nr_string_pool_to_json(const nrpool_t* pool);

typedef void (*nr_string_pool_apply_func_t)(const char* str,
                                            int len,
                                            void* user_data);

extern void nr_string_pool_apply(const nrpool_t* pool,
                                 nr_string_pool_apply_func_t apply_func,
                                 void* user_data);

#endif /* UTIL_STRING_POOL_HDR */
