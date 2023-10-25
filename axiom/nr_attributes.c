/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "nr_attributes.h"
#include "nr_attributes_private.h"
#include "util_hash.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

static void nr_attribute_config_modify_destinations_internal(
    nr_attribute_config_t* config,
    const char* match,
    uint32_t include_destinations,
    uint32_t exclude_destinations,
    bool finalize_destinations);

/*
 * Returns : 1 if there is a match and 0 otherwise.
 */
int nr_attribute_destination_modifier_match(
    const nr_attribute_destination_modifier_t* modifier,
    const char* key,
    uint32_t key_hash) {
  if (0 == modifier) {
    return 0;
  }
  if (0 == modifier->has_wildcard_suffix) {
    /* Exact match expected */
    if (modifier->match_hash != key_hash) {
      return 0;
    }
    if (0 == nr_strcmp(modifier->match, key)) {
      return 1;
    }
  } else {
    /* Note: match does NOT include '*' */
    /* match_len is used to avoid matching the terminating '\0' */
    if (0 == nr_strncmp(modifier->match, key, modifier->match_len)) {
      return 1;
    }
  }

  return 0;
}

uint32_t nr_attribute_destination_modifier_apply(
    const nr_attribute_destination_modifier_t* modifier,
    const char* key,
    uint32_t key_hash,
    uint32_t destinations) {
  if (0 == nr_attribute_destination_modifier_match(modifier, key, key_hash)) {
    return destinations;
  }

  /* Include before exclude, since exclude has priority. */
  destinations |= modifier->include_destinations;
  destinations &= ~modifier->exclude_destinations;

  return destinations;
}

void nr_attribute_destination_modifier_destroy(
    nr_attribute_destination_modifier_t** entry_ptr) {
  nr_attribute_destination_modifier_t* entry;

  if (0 == entry_ptr) {
    return;
  }
  entry = *entry_ptr;
  if (0 == entry) {
    return;
  }

  nr_free(entry->match);
  nr_realfree((void**)entry_ptr);
}

nr_attribute_config_t* nr_attribute_config_create(void) {
  nr_attribute_config_t* config;

  config = (nr_attribute_config_t*)nr_zalloc(sizeof(nr_attribute_config_t));
  config->modifier_list = 0;
  config->disabled_destinations = 0;

  return config;
}

void nr_attribute_config_disable_destinations(nr_attribute_config_t* config,
                                              uint32_t disabled_destinations) {
  if (0 == config) {
    return;
  }

  config->disabled_destinations |= disabled_destinations;
}

void nr_attribute_config_enable_destinations(nr_attribute_config_t* config,
                                             uint32_t enabled_destinations) {
  if (0 == config) {
    return;
  }

  config->disabled_destinations &= ~enabled_destinations;
}

static nr_attribute_destination_modifier_t*
nr_attribute_destination_modifier_create_internal(const char* match,
                                                  uint32_t include_destinations,
                                                  uint32_t exclude_destinations,
                                                  int is_finalize_rule) {
  nr_attribute_destination_modifier_t* new_entry;
  int match_len;
  int has_wildcard_suffix;

  if (0 == match) {
    return 0;
  }

  match_len = nr_strlen(match);

  if ('*' == match[match_len - 1]) {
    has_wildcard_suffix = 1;
    match_len -= 1;
  } else {
    has_wildcard_suffix = 0;
  }

  new_entry = (nr_attribute_destination_modifier_t*)nr_zalloc(
      sizeof(nr_attribute_destination_modifier_t));
  new_entry->has_wildcard_suffix = has_wildcard_suffix;
  new_entry->is_finalize_rule = is_finalize_rule;

  /* Use nr_strndup with match_len to avoid copying '*' suffix */
  new_entry->match = nr_strndup(match, match_len);
  new_entry->match_len = match_len;
  new_entry->match_hash = nr_mkhash(new_entry->match, 0);
  new_entry->include_destinations = include_destinations;
  new_entry->exclude_destinations = exclude_destinations;
  new_entry->next = 0;

  return new_entry;
}

nr_attribute_destination_modifier_t* nr_attribute_destination_modifier_create(
    const char* match,
    uint32_t include_destinations,
    uint32_t exclude_destinations) {
  return nr_attribute_destination_modifier_create_internal(
      match, include_destinations, exclude_destinations, false);
}

/*
 * Purpose : Determine the precedence order of two destination modifiers.
 *
 * Returns : 0 if the destination modifiers are identical.
 *           1 if mod1 should be applied after mod2
 *          -1 if mod1 should be applied before mod2
 */
static int nr_attribute_destination_modifier_compare(
    const nr_attribute_destination_modifier_t* mod1,
    const nr_attribute_destination_modifier_t* mod2) {
  int cmp;

  cmp = nr_strcmp(mod1->match, mod2->match);
  if (cmp < 0) {
    return -1;
  }

  if (0 == cmp) {
    if (mod1->has_wildcard_suffix == mod2->has_wildcard_suffix) {
      return 0;
    }
    if (mod1->has_wildcard_suffix) {
      return -1;
    }
    return 1;
  }

  return 1;
}

/*
 * Purpose : Inspects current modifier list and adds finalize rules as needed.
 *
 * Params  : 1. Attribute configuration
 *
 * Notes   : Certain attriubtes like log context attributes expect the "include"
 *           rules to act exclusively.
 *           For example:
 *             include = "A"
 *             exclude = "B"
 *             input = "A" "B" "C"
 *             expected = "A"
 *
 *           Note that "C" was excluded because it was not in the include rules.
 *           Also note an empty include rule means include everything and
 *           and to exclude nothing.
 *
 *           All other attributes besides log context attributes do NOT have
 *           this exclusive behavior for the include rules.  This function only
 *           considers rules of the NR_ATTRIBUTE_DESTINATION_LOG destination.
 *
 *           The algorithm examines all input rules for the
 *           NR_ATTRIBUTE_DESTINATION_LOG destination, and will create a new
 * rule of "exclude=*" with "is_finalize_rule=true" if any input rules exist.
 *           The exception is if there is an input rule of "*" for destination
 *           NR_ATTRIBUTE_DESTINATION_LOG.  In this case no finalize rule is
 * added.
 *
 *           The net resulting effect is the exclusion of any attributes not
 *           contained in the set of input rules for
 * NR_ATTRIBUTE_DESTINATION_LOG. The exception for if an "input=*" rule is
 * necessary since this input rule excludes nothing, making the finalize rule of
 * "exclude=*" unnecessary.
 *
 */
static void nr_attribute_config_finalize_log_destination(
    nr_attribute_config_t* config) {
  nr_attribute_destination_modifier_t* cur = NULL;
  nr_attribute_destination_modifier_t* prev = NULL;
  nr_attribute_destination_modifier_t* next = NULL;
  bool add_finalize_rule = false;

  if (NULL == config || NULL == config->modifier_list) {
    /* Since there is no configuration, no work to do. */
    return;
  }

  /* remove any existing rules with is_finalize_rule = true */
  for (cur = config->modifier_list; cur;) {
    next = cur->next;

    /* currently only finalize rules being created are for the 
     * NR_ATTRIBUTE_DESTINATION_LOG destination but check to 
     * be thorough and in case other finalize rules are created
     * in the future.
     */
    if (cur->is_finalize_rule
        && (cur->include_destinations & NR_ATTRIBUTE_DESTINATION_LOG)) {
      nr_attribute_destination_modifier_destroy(&cur);
      if (NULL != prev) {
        prev->next = next;
      } else {
        config->modifier_list = next;
      }
    } else {
      prev = cur;
    }
    cur = next;
  }

  /* unlikely but if all rules were finalize rules then no more work to do */
  if (NULL == config->modifier_list) {
    return;
  }

  /* now look for any include rules with a destination of
   * NR_ATTRIBUTE_DESTINATION_LOG and evaluate if any
   * finalize rules need to be added.
   */
  for (cur = config->modifier_list; cur; cur = cur->next) {
    if (cur->include_destinations & NR_ATTRIBUTE_DESTINATION_LOG) {
      if (cur->has_wildcard_suffix) {
        if (0 == cur->match_len) {
          /* there is an include rule of "*" so no finalize is needed
           * since all attributes are being explicitely included */
          return;
        }
      }
      add_finalize_rule = true;
    }
  }

  /* a finalize rule is needed
   * add an exclude rule of "*" which will remove any attributes which passed
   * through the include rules and therefore are excluded implicitely
   */
  if (add_finalize_rule) {
    nr_attribute_config_modify_destinations_internal(
        config, "*", 0, NR_ATTRIBUTE_DESTINATION_LOG, true);
  }
}

/*
 * Purpose : Inserts a modifier rule into an attribute configuraiton.
 *
 * Params  : 1. Attribute config to add modifier to
 *           2. String containing matching text for modifier
 *           3. Destinations to apply match to as include rule
 *           4. Destinations to apply match to as exclude rule
 *           5. If true this is a finalize rule used to cause
 *              include rules (on log destinations only currently)
 *              to be exclusive to anything not in the set of all
 *              include rules.
 *              For normal modifiers (from user rules) this can be false.
 *
 * Notes   : The is_finalize_rule option is currently only used in
 *           nr_attribute_config_finalize_log_destination() as it
 *           is called while adding an attribute and without this
 *           option there would be an infinite loop of adding and
 *           finalizing.
 */
static void nr_attribute_config_modify_destinations_internal(
    nr_attribute_config_t* config,
    const char* match,
    uint32_t include_destinations,
    uint32_t exclude_destinations,
    bool is_finalize_rule) {
  nr_attribute_destination_modifier_t* entry;
  nr_attribute_destination_modifier_t* new_entry;
  nr_attribute_destination_modifier_t** entry_ptr;

  if (0 == config) {
    return;
  }

  new_entry = nr_attribute_destination_modifier_create_internal(
      match, include_destinations, exclude_destinations, is_finalize_rule);
  if (0 == new_entry) {
    return;
  }

  entry_ptr = &config->modifier_list;
  entry = *entry_ptr;

  while (entry) {
    int cmp = nr_attribute_destination_modifier_compare(new_entry, entry);

    if (cmp < 0) {
      break;
    }

    /* if a finalize rule with the same name as a user rule is added (or vice
     * verse), do not remove the existing one or else the user rule could be
     * lost when finalizing */
    if (0 == cmp && new_entry->is_finalize_rule == entry->is_finalize_rule) {
      entry->include_destinations |= new_entry->include_destinations;
      entry->exclude_destinations |= new_entry->exclude_destinations;
      nr_attribute_destination_modifier_destroy(&new_entry);
      goto finalize_modifier;
    }

    entry_ptr = &entry->next;
    entry = *entry_ptr;
  }

  new_entry->next = entry;
  *entry_ptr = new_entry;

finalize_modifier:
  /* if an include modifier was added need to also add an exclude rule of "*"
   * to have include rule act to exclude anything not included. Exception is
   * if include rule was simply "*" which would allow everything so no
   * exclude=* is required
   */
  if (!is_finalize_rule) {
    nr_attribute_config_finalize_log_destination(config);
  }
}

void nr_attribute_config_modify_destinations(nr_attribute_config_t* config,
                                             const char* match,
                                             uint32_t include_destinations,
                                             uint32_t exclude_destinations) {
  nr_attribute_config_modify_destinations_internal(
      config, match, include_destinations, exclude_destinations, false);
}

static nr_attribute_destination_modifier_t*
nr_attribute_destination_modifier_copy(
    const nr_attribute_destination_modifier_t* entry) {
  nr_attribute_destination_modifier_t* new_entry;

  new_entry
      = (nr_attribute_destination_modifier_t*)nr_zalloc(sizeof(*new_entry));
  new_entry->has_wildcard_suffix = entry->has_wildcard_suffix;
  new_entry->is_finalize_rule = entry->is_finalize_rule;
  new_entry->match = nr_strdup(entry->match);
  new_entry->match_len = entry->match_len;
  new_entry->match_hash = entry->match_hash;
  new_entry->include_destinations = entry->include_destinations;
  new_entry->exclude_destinations = entry->exclude_destinations;
  new_entry->next = 0;

  return new_entry;
}

nr_attribute_config_t* nr_attribute_config_copy(
    const nr_attribute_config_t* config) {
  nr_attribute_config_t* new_config;
  const nr_attribute_destination_modifier_t* entry;
  nr_attribute_destination_modifier_t** new_entry_ptr;

  if (0 == config) {
    return 0;
  }

  new_config = nr_attribute_config_create();

  new_config->disabled_destinations = config->disabled_destinations;

  new_entry_ptr = &new_config->modifier_list;

  for (entry = config->modifier_list; entry; entry = entry->next) {
    nr_attribute_destination_modifier_t* new_entry
        = nr_attribute_destination_modifier_copy(entry);

    *new_entry_ptr = new_entry;
    new_entry_ptr = &new_entry->next;
  }

  return new_config;
}

uint32_t nr_attribute_config_apply(const nr_attribute_config_t* config,
                                   const char* key,
                                   uint32_t key_hash,
                                   uint32_t destinations) {
  nr_attribute_destination_modifier_t* modifier;

  if (0 == key) {
    /* A NULL key should not go to any destination. */
    return 0;
  }
  if (0 == config) {
    /* Since there is no configuration, the destinations are unchanged. */
    return destinations;
  }

  /* Important: The linked list must be iterated in a forward direction */
  for (modifier = config->modifier_list; modifier; modifier = modifier->next) {
    destinations = nr_attribute_destination_modifier_apply(
        modifier, key, key_hash, destinations);
  }

  /*
   * Apply the disabled destinations filter last, since it has priority over
   * all include/exclude settings.
   */
  destinations &= ~config->disabled_destinations;

  return destinations;
}

void nr_attribute_config_destroy(nr_attribute_config_t** config_ptr) {
  nr_attribute_destination_modifier_t* modifier;
  nr_attribute_config_t* config;

  if (0 == config_ptr) {
    return;
  }
  config = *config_ptr;
  if (0 == config) {
    return;
  }

  modifier = config->modifier_list;
  while (modifier) {
    nr_attribute_destination_modifier_t* next = modifier->next;

    nr_attribute_destination_modifier_destroy(&modifier);
    modifier = next;
  }

  nr_realfree((void**)config_ptr);
}

nr_attributes_t* nr_attributes_create(const nr_attribute_config_t* config) {
  nr_attributes_t* attributes;

  attributes = (nr_attributes_t*)nr_zalloc(sizeof(nr_attributes_t));
  attributes->config = nr_attribute_config_copy(config);
  attributes->agent_attribute_list = 0;
  attributes->user_attribute_list = 0;
  attributes->num_user_attributes = 0;

  return attributes;
}

void nr_attribute_destroy(nr_attribute_t** attribute_ptr) {
  nr_attribute_t* attribute;

  if (0 == attribute_ptr) {
    return;
  }
  attribute = *attribute_ptr;
  if (0 == attribute) {
    return;
  }
  nro_delete(attribute->value);
  nr_free(attribute->key);
  nr_realfree((void**)attribute_ptr);
}

static void nr_attribute_list_destroy(nr_attribute_t* attribute) {
  while (attribute) {
    nr_attribute_t* next = attribute->next;

    nr_attribute_destroy(&attribute);
    attribute = next;
  }
}

void nr_attributes_destroy(nr_attributes_t** attributes_ptr) {
  nr_attributes_t* attributes;

  if (0 == attributes_ptr) {
    return;
  }

  attributes = *attributes_ptr;
  if (0 == attributes) {
    return;
  }

  nr_attribute_config_destroy(&attributes->config);
  nr_attribute_list_destroy(attributes->user_attribute_list);
  nr_attribute_list_destroy(attributes->agent_attribute_list);

  nr_realfree((void**)attributes_ptr);
}

void nr_attributes_remove_attribute(nr_attributes_t* attributes,
                                    const char* key,
                                    int is_user) {
  uint32_t key_hash;

  if ((NULL == attributes) || (NULL == key)) {
    return;
  }
  key_hash = nr_mkhash(key, 0);
  nr_attributes_remove_duplicate(attributes, key, key_hash, is_user);
}

void nr_attributes_remove_duplicate(nr_attributes_t* ats,
                                    const char* key,
                                    uint32_t key_hash,
                                    int is_user) {
  nr_attribute_t* attribute;
  nr_attribute_t** attribute_ptr;

  if (0 == ats) {
    return;
  }
  if (0 == key) {
    return;
  }

  if (is_user) {
    attribute_ptr = &ats->user_attribute_list;
  } else {
    attribute_ptr = &ats->agent_attribute_list;
  }

  attribute = *attribute_ptr;

  while (attribute) {
    if ((key_hash == attribute->key_hash)
        && (0 == nr_strcmp(key, attribute->key))) {
      *attribute_ptr = attribute->next;
      nr_attribute_destroy(&attribute);

      if (is_user) {
        ats->num_user_attributes -= 1;
      }

      return;
    }

    attribute_ptr = &attribute->next;
    attribute = *attribute_ptr;
  }
}

static void nr_attributes_log_destination_change(const char* key,
                                                 uint32_t default_destinations,
                                                 uint32_t final_destinations) {
  nrl_verbosedebug(
      NRL_TXN,
      "attribute '%.128s' destinations modified by configuration: "
      "%.64s%.64s%.64s%.64s ==> %.64s%.64s%.64s%.64s",
      key ? key : "",
      (NR_ATTRIBUTE_DESTINATION_TXN_EVENT & default_destinations) ? "event "
                                                                  : "",
      (NR_ATTRIBUTE_DESTINATION_TXN_TRACE & default_destinations) ? "trace "
                                                                  : "",
      (NR_ATTRIBUTE_DESTINATION_ERROR & default_destinations) ? "error " : "",
      (NR_ATTRIBUTE_DESTINATION_BROWSER & default_destinations) ? "browser "
                                                                : "",

      (NR_ATTRIBUTE_DESTINATION_TXN_EVENT & final_destinations) ? "event " : "",
      (NR_ATTRIBUTE_DESTINATION_TXN_TRACE & final_destinations) ? "trace " : "",
      (NR_ATTRIBUTE_DESTINATION_ERROR & final_destinations) ? "error " : "",
      (NR_ATTRIBUTE_DESTINATION_BROWSER & final_destinations) ? "browser "
                                                              : "");
}

static int nr_attributes_is_valid_value(const nrobj_t* value) {
  double dbl;

  switch (nro_type(value)) {
    case NR_OBJECT_INVALID:
      return 0;

    case NR_OBJECT_DOUBLE:
      dbl = nro_get_double(value, NULL);
      if (isnan(dbl) || isinf(dbl)) {
        const char* kind = isnan(dbl) ? "NaN" : "Infinity";

        nrl_warning(NRL_API, "invalid double attribute argument: %s", kind);
        return 0;
      }
      return 1;

    case NR_OBJECT_NONE:    /* Fall through to */
    case NR_OBJECT_BOOLEAN: /* Fall through to */
    case NR_OBJECT_INT:     /* Fall through to */
    case NR_OBJECT_LONG:    /* Fall through to */
    case NR_OBJECT_ULONG:   /* Fall through to */
    case NR_OBJECT_STRING:  /* Fall through to */
      return 1;

    case NR_OBJECT_JSTRING: /* Fall through to */
    case NR_OBJECT_HASH:    /* Fall through to */
    case NR_OBJECT_ARRAY:   /* Fall through to */
    default:
      nrl_warning(NRL_TXN, "improper attribute type");
      return 0;
  }
}

static nr_status_t nr_attributes_add_internal(nr_attributes_t* ats,
                                              uint32_t default_destinations,
                                              int is_user,
                                              const char* key,
                                              const nrobj_t* value) {
  uint32_t key_hash;
  uint32_t final_destinations;
  nr_attribute_t* attribute;

  if (0 == ats) {
    return NR_FAILURE;
  }
  if ((0 == key) || (0 == key[0])) {
    return NR_FAILURE;
  }
  if (0 == nr_attributes_is_valid_value(value)) {
    return NR_FAILURE;
  }

  /*
   * Dropping attributes whose keys are excessively long rather than
   * truncating the keys was chosen by product management to avoid
   * worrying about the application of configuration to truncated values, or
   * performing the truncation after configuration.
   */
  if (nr_strlen(key) > NR_ATTRIBUTE_KEY_LENGTH_LIMIT) {
    if (is_user) {
      nrl_warning(
          NRL_TXN,
          "potential attribute discarded: key '%.128s' exceeds size limit %d",
          key, NR_ATTRIBUTE_KEY_LENGTH_LIMIT);
    } else {
      /*
       * This log message should not be visible by default.  We do not want
       * long request parameters to generate log errors/warnings.
       */
      nrl_debug(
          NRL_TXN,
          "potential attribute discarded: key '%.128s' exceeds size limit %d",
          key, NR_ATTRIBUTE_KEY_LENGTH_LIMIT);
    }
    return NR_FAILURE;
  }

  key_hash = nr_mkhash(key, 0);
  final_destinations = nr_attribute_config_apply(ats->config, key, key_hash,
                                                 default_destinations);

  if (0 == final_destinations) {
    /* There is no purpose in saving attributes which will not be used. */
    nrl_verbosedebug(NRL_TXN, "attribute '%.128s' disabled by configuration",
                     key);
    return NR_FAILURE;
  }

  if (final_destinations != default_destinations) {
    nr_attributes_log_destination_change(key, default_destinations,
                                         final_destinations);
  }

  /*
   * If the attribute being added has a key which is the same as the key
   * of an attribute which already exists, the existing attribute will be
   * removed:  The last attribute in wins.
   */
  nr_attributes_remove_duplicate(ats, key, key_hash, is_user);

  if (is_user && (NR_ATTRIBUTE_USER_LIMIT == ats->num_user_attributes)) {
    /* Note that we check this after removing a duplicate. */

    nrl_warning(NRL_TXN,
                "attribute '%.128s' discarded: user limit of %d reached.", key,
                NR_ATTRIBUTE_USER_LIMIT);
    return NR_FAILURE;
  }

  attribute = (nr_attribute_t*)nr_zalloc(sizeof(*attribute));
  attribute->destinations = final_destinations;
  attribute->key_hash = key_hash;
  attribute->key = nr_strdup(key);
  attribute->value = nro_copy(value);

  /* Prepend the new attribute to the front of the unordered list. */
  if (is_user) {
    ats->num_user_attributes += 1;
    attribute->next = ats->user_attribute_list;
    ats->user_attribute_list = attribute;
  } else {
    attribute->next = ats->agent_attribute_list;
    ats->agent_attribute_list = attribute;
  }

  return NR_SUCCESS;
}

static nr_status_t nr_attributes_add(nr_attributes_t* ats,
                                     uint32_t default_destinations,
                                     int is_user,
                                     const char* key,
                                     const nrobj_t* value) {
  nr_status_t rv;
  const char* str;
  char bounded_str[NR_ATTRIBUTE_VALUE_LENGTH_LIMIT + 1];
  nrobj_t* bounded_value;

  if (NR_OBJECT_STRING != nro_type(value)) {
    return nr_attributes_add_internal(ats, default_destinations, is_user, key,
                                      value);
  }

  /*
   * We do not log the details of this truncation, since the value might be
   * sensitive request parameter (and this is before we even know if the
   * attribute will be captured).
   */
  bounded_str[0] = '\0';
  str = nro_get_string(value, NULL);
  snprintf(bounded_str, sizeof(bounded_str), "%s", str ? str : "");
  bounded_value = nro_new_string(bounded_str);
  rv = nr_attributes_add_internal(ats, default_destinations, is_user, key,
                                  bounded_value);
  nro_delete(bounded_value);
  return rv;
}

nr_status_t nr_attributes_user_add(nr_attributes_t* ats,
                                   uint32_t default_destinations,
                                   const char* key,
                                   const nrobj_t* value) {
  return nr_attributes_add(ats, default_destinations, 1, key, value);
}

nr_status_t nr_attributes_user_add_string(nr_attributes_t* ats,
                                          uint32_t default_destinations,
                                          const char* key,
                                          const char* value) {
  nr_status_t rv;
  nrobj_t* obj = nro_new_string(value);

  rv = nr_attributes_user_add(ats, default_destinations, key, obj);
  nro_delete(obj);
  return rv;
}

nr_status_t nr_attributes_user_add_long(nr_attributes_t* ats,
                                        uint32_t default_destinations,
                                        const char* key,
                                        int64_t lng) {
  nr_status_t rv;
  nrobj_t* obj = nro_new_long(lng);

  rv = nr_attributes_user_add(ats, default_destinations, key, obj);
  nro_delete(obj);

  return rv;
}

nr_status_t nr_attributes_agent_add_long(nr_attributes_t* ats,
                                         uint32_t default_destinations,
                                         const char* key,
                                         int64_t lng) {
  nr_status_t rv;
  nrobj_t* value = nro_new_long(lng);

  rv = nr_attributes_add(ats, default_destinations, 0, key, value);
  nro_delete(value);

  return rv;
}

nr_status_t nr_attributes_agent_add_string(nr_attributes_t* ats,
                                           uint32_t default_destinations,
                                           const char* key,
                                           const char* str) {
  nr_status_t rv;
  nrobj_t* value = nro_new_string(str);

  rv = nr_attributes_add(ats, default_destinations, 0, key, value);
  nro_delete(value);

  return rv;
}

/*
 * Purpose : Internal function to convert list of attributes to nro
 *
 * Params  : 1. List of attributes
 *           2. Prefix to prepend to all attribute names
 *              NULL indicates to use no prefix and is more efficient than ""
 *           3. Attribute destinations
 */
static nrobj_t* nr_attributes_to_obj_internal(
    const nr_attribute_t* attribute_list,
    const char* attribute_prefix,
    uint32_t destination) {
  nrobj_t* obj;
  const nr_attribute_t* attribute;

  if (NULL == attribute_list) {
    return NULL;
  }

  obj = nro_new_hash();

  for (attribute = attribute_list; attribute; attribute = attribute->next) {
    if (0 == (attribute->destinations & destination)) {
      continue;
    }

    if (nrlikely(NULL == attribute_prefix)) {
      nro_set_hash(obj, attribute->key, attribute->value);
    } else {
      char* key = nr_formatf("%s%s", attribute_prefix, attribute->key);
      nro_set_hash(obj, key, attribute->value);
      nr_free(key);
    }
  }

  return obj;
}

nrobj_t* nr_attributes_user_to_obj(const nr_attributes_t* attributes,
                                   uint32_t destination) {
  if (0 == attributes) {
    return 0;
  }
  return nr_attributes_to_obj_internal(attributes->user_attribute_list, NULL,
                                       destination);
}

nrobj_t* nr_attributes_agent_to_obj(const nr_attributes_t* attributes,
                                    uint32_t destination) {
  if (0 == attributes) {
    return 0;
  }
  return nr_attributes_to_obj_internal(attributes->agent_attribute_list, NULL,
                                       destination);
}

nrobj_t* nr_attributes_logcontext_to_obj(const nr_attributes_t* attributes,
                                         uint32_t destination) {
  if (0 == attributes) {
    return 0;
  }
  return nr_attributes_to_obj_internal(attributes->user_attribute_list,
                                       NR_LOG_CONTEXT_DATA_ATTRIBUTE_PREFIX,
                                       destination);
}

static char* nr_attribute_debug_json(const nr_attribute_t* attribute) {
  nrobj_t* dests;
  nrobj_t* obj;
  char* json;

  if (0 == attribute) {
    return 0;
  }

  obj = nro_new_hash();
  dests = nro_new_array();

  if (NR_ATTRIBUTE_DESTINATION_TXN_EVENT & attribute->destinations) {
    nro_set_array_string(dests, 0, "event");
  }
  if (NR_ATTRIBUTE_DESTINATION_TXN_TRACE & attribute->destinations) {
    nro_set_array_string(dests, 0, "trace");
  }
  if (NR_ATTRIBUTE_DESTINATION_ERROR & attribute->destinations) {
    nro_set_array_string(dests, 0, "error");
  }
  if (NR_ATTRIBUTE_DESTINATION_BROWSER & attribute->destinations) {
    nro_set_array_string(dests, 0, "browser");
  }
  if (NR_ATTRIBUTE_DESTINATION_LOG & attribute->destinations) {
    nro_set_array_string(dests, 0, "log");
  }

  nro_set_hash(obj, "dests", dests);
  nro_delete(dests);

  nro_set_hash_string(obj, "key", attribute->key);
  nro_set_hash(obj, "value", attribute->value);

  json = nro_to_json(obj);
  nro_delete(obj);
  return json;
}

/*
 * For testing purposes only.
 */
char* nr_attributes_debug_json(const nr_attributes_t* attributes) {
  nrobj_t* obj;
  nrobj_t* agent;
  nrobj_t* user;
  char* json;
  const nr_attribute_t* attribute;

  if (0 == attributes) {
    return 0;
  }

  obj = nro_new_hash();
  agent = nro_new_array();
  user = nro_new_array();

  for (attribute = attributes->user_attribute_list; attribute;
       attribute = attribute->next) {
    json = nr_attribute_debug_json(attribute);

    nro_set_array_jstring(user, 0, json);
    nr_free(json);
  }

  for (attribute = attributes->agent_attribute_list; attribute;
       attribute = attribute->next) {
    json = nr_attribute_debug_json(attribute);

    nro_set_array_jstring(agent, 0, json);
    nr_free(json);
  }

  nro_set_hash(obj, "user", user);
  nro_set_hash(obj, "agent", agent);
  json = nro_to_json(obj);
  nro_delete(obj);
  nro_delete(agent);
  nro_delete(user);

  return json;
}

bool nr_attributes_user_exists(const nr_attributes_t* attributes,
                               const char* key) {
  const nr_attribute_t* attribute;

  if (NULL == attributes) {
    return false;
  }

  for (attribute = attributes->user_attribute_list; attribute;
       attribute = attribute->next) {
    if (0 == nr_strcmp(attribute->key, key)) {
      return true;
    }
  }

  return false;
}
