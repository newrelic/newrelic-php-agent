/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "nr_axiom.h"
#include "nr_limits.h"
#include "nr_php_packages.h"
#include "util_memory.h"
#include "util_minmax_heap.h"
#include "util_vector.h"
#include "util_hashmap.h"
#include "util_hashmap_private.h"
#include "util_logging.h"
#include "util_strings.h"

typedef struct {
  nrbuf_t* buf;
  bool package_added;
} nr_php_package_json_builder_t;

static inline const char* nr_php_package_source_priority_to_string(const nr_php_package_source_priority_t source_priority) {
  switch (source_priority) {
    case NR_PHP_PACKAGE_SOURCE_LEGACY:
      return "legacy";
    case NR_PHP_PACKAGE_SOURCE_COMPOSER:
      return "composer";
    default:
      return "unknown";
  }
}

nr_php_package_t* nr_php_package_create_with_source(char* name, char* version, const nr_php_package_source_priority_t source_priority) {
  nr_php_package_t* p = NULL;

  if (NULL == name) {
    return NULL;
  }

  p = (nr_php_package_t*)nr_malloc(sizeof(nr_php_package_t));
  if (NULL == p) {
    return NULL;
  }

  p->package_name = nr_strdup(name);
  if (NULL != version) {
    p->package_version = nr_strdup(version);
  } else {
    p->package_version = nr_strdup(
        PHP_PACKAGE_VERSION_UNKNOWN);  // if null, version is set to an empty
                                       // string with a space according to spec
  }
  p->source_priority = source_priority;
  p->options = 0;

  nrl_verbosedebug(NRL_INSTRUMENT, "Creating PHP Package '%s', version '%s', source %s",
                   p->package_name, p->package_version, nr_php_package_source_priority_to_string(source_priority));
  return p;
}

nr_php_package_t* nr_php_package_create(char* name, char* version) {
  return nr_php_package_create_with_source(name, version, NR_PHP_PACKAGE_SOURCE_LEGACY);
}

void nr_php_package_destroy(nr_php_package_t* p) {
  if (NULL != p) {
    nr_free(p->package_name);
    nr_free(p->package_version);
    nr_free(p);
  }
}

void nr_php_package_set_options(nr_php_package_t* p,
                                nr_php_package_options_t options) {
  if (NULL != p) {
    p->options = options;
  }
}

nr_php_package_options_t nr_php_package_get_options(nr_php_package_t* p) {
  if (NULL == p) {
    return 0;
  }
  return p->options;
}

nr_php_packages_t* nr_php_packages_create() {
  nr_php_packages_t* h = NULL;
  h = (nr_php_packages_t*)nr_malloc(sizeof(nr_php_packages_t));
  if (NULL == h) {
    return NULL;
  }

  h->data = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_php_package_destroy);
  if (NULL == h->data) {
    nr_free(h);
    return NULL;
  }
  return h;
}

nr_php_package_t* nr_php_packages_add_package(nr_php_packages_t* h,
                                              nr_php_package_t* p) {
  nr_php_package_t* package;
  if (NULL == h) {
    return NULL;
  }

  if (NULL == p || NULL == p->package_name || NULL == p->package_version) {
    return NULL;
  }

  // If package with the same key already exists, we will check if the value is
  // different. If it is different, then we will update the value of the package
  package = (nr_php_package_t*)nr_hashmap_get(h->data, p->package_name,
                                              nr_strlen(p->package_name));
  if (NULL != package) {
    if (package->source_priority <= p->source_priority && 0 != nr_strcmp(package->package_version, p->package_version)) {
      nr_free(package->package_version);
      package->package_version = nr_strdup(p->package_version);
    }
    nr_php_package_destroy(p);
    return package;
  }

  nr_hashmap_set(h->data, p->package_name, nr_strlen(p->package_name), p);
  return p;
}

void nr_php_packages_set_package_options(nr_php_packages_t* h,
                                         const char* package_name,
                                         nr_php_package_options_t options) {
  nr_php_package_t* package;

  if (NULL == h) {
    return;
  }

  if (nr_strempty(package_name)) {
    return;
  }

  package = nr_php_packages_get_package(h, package_name);
  if (NULL == package) {
    return;
  }

  nr_php_package_set_options(package, options);
}

nr_php_package_options_t nr_php_packages_get_package_options(
    nr_php_packages_t* h,
    const char* package_name) {
  nr_php_package_t* package;

  if (NULL == h) {
    return 0;
  }

  if (nr_strempty(package_name)) {
    return 0;
  }

  package = nr_php_packages_get_package(h, package_name);
  if (NULL == package) {
    return 0;
  }

  return nr_php_package_get_options(package);
}

void nr_php_packages_iterate(nr_php_packages_t* packages,
                             nr_php_packages_iter_t callback,
                             void* userdata) {
  if (NULL == packages || NULL == callback) {
    return;
  }

  nr_hashmap_apply(packages->data, (nr_hashmap_apply_func_t)callback, userdata);
}

char* nr_php_package_to_json(nr_php_package_t* package) {
  char* json;
  nrbuf_t* buf = NULL;

  if (NULL == package || NULL == package->package_name
      || NULL == package->package_version) {
    return NULL;
  }
  buf = nr_buffer_create(0, 0);
  nr_buffer_add(buf, NR_PSTR("[\""));
  nr_buffer_add(buf, package->package_name, nr_strlen(package->package_name));
  nr_buffer_add(buf, NR_PSTR("\",\""));
  nr_buffer_add(buf, package->package_version,
                nr_strlen(package->package_version));
  nr_buffer_add(buf, NR_PSTR("\",{}]"));
  nr_buffer_add(buf, NR_PSTR("\0"));

  json = nr_strdup(nr_buffer_cptr(buf));
  nr_buffer_destroy(&buf);

  return json;
}

static void apply_package_to_json_conversion(void* value,
                                             const char* key,
                                             size_t key_len,
                                             void* user_data) {
  nr_php_package_json_builder_t* json_builder
      = (nr_php_package_json_builder_t*)user_data;
  char* package_json;

  (void)key;
  (void)key_len;

  package_json = nr_php_package_to_json((nr_php_package_t*)value);
  if (package_json) {
    if (json_builder->package_added) {
      nr_buffer_add(json_builder->buf, NR_PSTR(","));
    } else {
      json_builder->package_added = true;
    }
    nr_buffer_add(json_builder->buf, package_json, nr_strlen(package_json));
    nr_free(package_json);
  }
}

bool nr_php_packages_to_json_buffer(nr_php_packages_t* h, nrbuf_t* buf) {
  nr_php_package_json_builder_t json_builder = {buf, false};

  if (NULL == h || NULL == h->data || NULL == buf) {
    return false;
  }

  nr_buffer_add(buf, NR_PSTR("["));
  nr_hashmap_apply(h->data, apply_package_to_json_conversion, &json_builder);

  nr_buffer_add(buf, NR_PSTR("]"));
  return true;
}

char* nr_php_packages_to_json(nr_php_packages_t* h) {
  nrbuf_t* buf = nr_buffer_create(0, 0);
  char* json = NULL;

  if (!nr_php_packages_to_json_buffer(h, buf)) {
    goto end;
  }

  nr_buffer_add(buf, NR_PSTR("\0"));
  json = nr_strdup(nr_buffer_cptr(buf));

end:
  nr_buffer_destroy(&buf);
  return json;
}
