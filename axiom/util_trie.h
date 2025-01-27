/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_TRIE_HDR
#define UTIL_TRIE_HDR

#include <stddef.h>
#include <stdbool.h>

/*
 * The opaque trie type.
 */
typedef struct _nr_trie_node_t nr_trie_t;

/*
 * Purpose : Add a suffix to the trie.
 *
 * Params  : 1. The trie.
 *           2. The suffix.
 *           3. The length of the suffix.
 *           4. Is the suffix case sensitive.
 *           5. The value to associate with the suffix.
 */
extern void nr_trie_suffix_add(nr_trie_t* root,
                               const char* suffix,
                               size_t suffix_len,
                               bool is_case_sensitive,
                               void* value);

extern void* nr_trie_suffix_lookup(nr_trie_t* root,
                                   const char* string,
                                   size_t string_len,
                                   size_t skip_len);

/*
 * Purpose : Add a prefix to the trie.
 *
 * Params  : 1. The trie.
 *           2. The prefix.
 *           3. The length of the prefix.
 *           4. Is the prefix case sensitive.
 *           5. The value to associate with the prefix.
 */
extern void nr_trie_prefix_add(nr_trie_t* root,
                               const char* prefix,
                               size_t prefix_len,
                               bool is_case_sensitive,
                               void* value);

extern void* nr_trie_prefix_lookup(nr_trie_t* root,
                                   const char* string,
                                   size_t string_len,
                                   size_t skip_len);

/*
 * Purpose : Create a trie.
 *
 * Returns : A newly allocated trie.
 */
extern nr_trie_t* nr_trie_create(void);

/*
 * Purpose : Destroy a trie.
 *
 * Params  : 1. The address of the trie to destroy.
 */
extern void nr_trie_destroy(nr_trie_t** root_ptr);

#endif
