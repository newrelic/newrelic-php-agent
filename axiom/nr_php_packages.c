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
#include "util_strings.h"

typedef struct {
  nrbuf_t* buf;
  bool package_added;
} nr_php_package_json_builder_t;

nr_php_package_t* nr_php_package_create(char* name, char* version) {
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
    p->package_version
        = nr_strdup(" ");  // if null, version is set to an empty
                           // string with a space according to spec
  }

  return p;
}

void nr_php_package_destroy(nr_php_package_t* p) {
  if (NULL != p) {
    nr_free(p->package_name);
    nr_free(p->package_version);
    nr_free(p);
  }
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

void nr_php_packages_add_package(nr_php_packages_t* h, nr_php_package_t* p) {
  nr_php_package_t* package;
  if (NULL == h) {
    return;
  }

  if (NULL == p || NULL == p->package_name || NULL == p->package_version) {
    return;
  }

  // If package with the same key already exists, we will check if the value is
  // different. If it is different, then we will update the value of the package
  package = (nr_php_package_t*)nr_hashmap_get(h->data, p->package_name,
                                              nr_strlen(p->package_name));
  if (NULL != package) {
    if (0 != nr_strcmp(package->package_version, p->package_version)) {
      nr_free(package->package_version);
      package->package_version = nr_strdup(p->package_version);
    }
    nr_php_package_destroy(p);
    return;
  }

  nr_hashmap_set(h->data, p->package_name, nr_strlen(p->package_name), p);
}

size_t nr_hashmap_count_buckets(const nr_php_packages_t* hashmap) {
  return (size_t)(1 << hashmap->log2_num_buckets);
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

bool nr_php_packages_to_json_buffer(nr_php_packages_t* h, nrbuf_t* buf) {
  size_t num_buckets;
  size_t i;
  nr_hashmap_bucket_t* bucket;
  bool package_added = false;
  char* package_json;

  if (NULL == h || NULL == buf) {
    return false;
  }

  num_buckets = nr_hashmap_count_buckets(h);
  nr_buffer_add(buf, NR_PSTR("["));

  for (i = 0; i < num_buckets; i++) {
    for (bucket = h->buckets[i]; NULL != bucket; bucket = bucket->next) {
      package_json = nr_php_package_to_json((nr_php_package_t*)bucket->value);
      if (package_json) {
        if (package_added) {
          nr_buffer_add(buf, NR_PSTR(","));
        }
        nr_buffer_add(buf, package_json, nr_strlen(package_json));
        nr_free(package_json);
        package_added = true;
      }
    }
  }

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
