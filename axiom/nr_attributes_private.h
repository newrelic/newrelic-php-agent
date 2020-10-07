/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_ATTRIBUTES_PRIVATE_HDR
#define NR_ATTRIBUTES_PRIVATE_HDR

#include <stdint.h>

#include "nr_attributes.h"
#include "util_object.h"

typedef struct _nr_attribute_destination_modifier_t {
  int has_wildcard_suffix; /* Whether 'match' is exact or a prefix. */
  char* match; /* The string to match against.  This will not contain a trailing
                  '*'. */
  int match_len;                 /* The length of 'match'. */
  uint32_t match_hash;           /* The hash of 'match'. */
  uint32_t include_destinations; /* Bit set of destinations to be added by this
                                    modifier. */
  uint32_t exclude_destinations; /* Bit set of destinations to be dropped by
                                    this modifier. */
  struct _nr_attribute_destination_modifier_t*
      next; /* Next linked list entry */
} nr_attribute_destination_modifier_t;

struct _nr_attribute_config_t {
  uint32_t
      disabled_destinations; /* Destinations that no attributes should go to. */
  /*
   * Linked list of destination modifiers.
   * The order of this list is important.  This ordering is based primarily on
   * 'match', and secondarily on 'has_wildcard_suffix'.  Modifiers appearing
   * later have precedence over modifiers appearing earlier.
   *
   * See: nr_attribute_destination_modifier_compare
   */
  nr_attribute_destination_modifier_t* modifier_list;
};

typedef struct _nr_attribute_t {
  char* key;
  uint32_t key_hash;
  nrobj_t* value;
  uint32_t
      destinations; /* Set of destinations after config has been applied. */
  struct _nr_attribute_t* next; /* Next linked list entry. */
} nr_attribute_t;

struct _nr_attributes_t {
  /*
   * Configuration copied during initialization.
   * The configuration is not modified thereafter.
   */
  struct _nr_attribute_config_t* config;
  /*
   * The number of attributes from the user.  This is maintained so that we
   * can cap the number to NR_ATTRIBUTE_USER_LIMIT.
   */
  int num_user_attributes;
  struct _nr_attribute_t*
      agent_attribute_list; /* Unordered linked list of agent attributes. */
  struct _nr_attribute_t*
      user_attribute_list; /* Unordered linked list of user attributes. */
};

extern int nr_attribute_destination_modifier_match(
    const nr_attribute_destination_modifier_t* modifier,
    const char* key,
    uint32_t key_hash);
extern uint32_t nr_attribute_destination_modifier_apply(
    const nr_attribute_destination_modifier_t* modifier,
    const char* key,
    uint32_t key_hash,
    uint32_t destinations);
extern void nr_attribute_destination_modifier_destroy(
    nr_attribute_destination_modifier_t** entry_ptr);
extern nr_attribute_destination_modifier_t*
nr_attribute_destination_modifier_create(const char* match,
                                         uint32_t include_destinations,
                                         uint32_t exclude_destinations);
extern uint32_t nr_attribute_config_apply(const nr_attribute_config_t* config,
                                          const char* key,
                                          uint32_t key_hash,
                                          uint32_t destinations);
extern void nr_attribute_destroy(nr_attribute_t** attribute_ptr);
extern void nr_attributes_remove_duplicate(nr_attributes_t* ats,
                                           const char* key,
                                           uint32_t key_hash,
                                           int is_user);
extern char* nr_attributes_debug_json(const nr_attributes_t* attributes);

#endif /* NR_ATTRIBUTES_PRIVATE_HDR */
