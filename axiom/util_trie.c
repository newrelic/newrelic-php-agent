/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "util_trie.h"
#include "util_memory.h"
#include "util_strings.h"

typedef struct _nr_trie_node_t nr_trie_node_t;
struct _nr_trie_node_t {
  nr_trie_node_t* children[256];
  nr_trie_node_t* parent;
  void* value;
};

static nr_trie_node_t* nr_trie_node_create(void) {
  nr_trie_node_t* node = nr_zalloc(sizeof(nr_trie_node_t));
  return node;
}

void nr_trie_suffix_add(nr_trie_t* trie,
                        const char* suffix,
                        size_t suffix_len,
                        bool is_case_sensitive,
                        void* value) {
  nr_trie_node_t* root = trie;
  int c = suffix[suffix_len - 1];

  if (NULL == trie) {
    return;
  }

  if (NULL == root->children[c]) {
    root->children[c] = nr_trie_node_create();
    root->children[c]->parent = root;
    // make trie case-insensitive
    if (!is_case_sensitive) {
      int c_other_case;
      if (nr_islower(c)) {
        c_other_case = nr_toupper(c);
      } else {
        c_other_case = nr_tolower(c);
      }
      root->children[c_other_case] = root->children[c];
      root->children[c_other_case]->parent = root;
    }
  }
  if (1 == suffix_len) {
    root->value = value;
    return;
  } else {
    nr_trie_suffix_add(root->children[c], suffix, suffix_len - 1,
                       is_case_sensitive, value);
  }
}

void* nr_trie_suffix_lookup(nr_trie_t* trie,
                            const char* string,
                            size_t string_len,
                            size_t skip_len) {
  nr_trie_node_t* node = trie;
  nr_trie_node_t* parent = NULL;
  int cidx = string_len - (1 + skip_len);

  if (NULL == trie || NULL == string || 0 == string_len) {
    return NULL;
  }

  while (node && cidx >= 0) {  // don't go past start of the string
    int c = string[cidx--];
    if (!node->children[c]) {
      break;
    }
    parent = node;
    node = node->children[c];
  }
  return parent ? parent->value : NULL;
}

void nr_trie_prefix_add(nr_trie_t* trie,
                        const char* prefix,
                        size_t prefix_len,
                        bool is_case_sensitive,
                        void* value) {
  nr_trie_node_t* root = trie;
  int c = prefix[0];

  if (NULL == trie) {
    return;
  }

  if (NULL == root->children[c]) {
    root->children[c] = nr_trie_node_create();
    root->children[c]->parent = root;
    // make trie case-insensitive
    if (!is_case_sensitive) {
      int c_other_case;
      if (nr_islower(c)) {
        c_other_case = nr_toupper(c);
      } else {
        c_other_case = nr_tolower(c);
      }
      root->children[c_other_case] = root->children[c];
      root->children[c_other_case]->parent = root;
    }
  }
  if (1 == prefix_len) {
    root->value = value;
    return;
  } else {
    nr_trie_suffix_add(root->children[c], prefix + 1, prefix_len - 1,
                       is_case_sensitive, value);
  }
}

void* nr_trie_prefix_lookup(nr_trie_t* trie,
                            const char* string,
                            size_t string_len,
                            size_t skip_len) {
  nr_trie_node_t* node = trie;
  nr_trie_node_t* parent = NULL;
  size_t cidx = skip_len;

  if (NULL == trie || NULL == string || 0 == string_len) {
    return NULL;
  }

  while (node && cidx <= string_len) {  // don't go past end of the string
    int c = string[cidx++];
    if (!node->children[c]) {
      break;
    }
    parent = node;
    node = node->children[c];
  }
  return parent ? parent->value : NULL;
}

nr_trie_t* nr_trie_create(void) {
  nr_trie_t* root = nr_trie_node_create();
  return root;
}

void nr_trie_destroy(nr_trie_t** trie_ptr) {
  nr_trie_node_t* root;

  if (NULL == trie_ptr || NULL == *trie_ptr) {
    return;
  }

  root = *trie_ptr;
  for (int i = 0; i < 256; i++) {
    if (root->children[i] && !nr_isupper(i)) {
      nr_trie_destroy(&root->children[i]);
    }
  }
  nr_realfree((void**)trie_ptr);
}
