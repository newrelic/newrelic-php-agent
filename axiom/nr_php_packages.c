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

nr_php_package_t* nr_php_package_create(char* name, char* version) {
  nr_php_package_t* p = NULL;

  if (name == NULL || version == NULL) {
    return NULL;
  }

  p = (nr_php_package_t*)nr_malloc(sizeof(nr_php_package_t));
  if (p == NULL) {
    return NULL;
  }

  p->package_name = nr_strdup(name);
  p->package_version = nr_strdup(version);

  return p;
}

void nr_php_package_destroy(nr_php_package_t* p) {
  if (p != NULL) {
    nr_free(p->package_name);
    nr_free(p->package_version);
    nr_free(p);
  }
}

void nr_php_packages_add_package(nr_php_packages_t** h, nr_php_package_t* p) {
  if (h == NULL) {
    return;
  }

  if (*h == NULL) {
    *h = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_php_package_destroy);
  }

  if (p == NULL || p->package_name == NULL || p->package_version == NULL) {
    return;
  }

  // If package with the same key already exists, we will check if the value is
  // different. If it is different, then we will update the value of the package
  if (0
      != nr_php_packages_has_package(*h, p->package_name,
                                     nr_strlen(p->package_name))) {
    nr_php_package_t* package = (nr_php_package_t*)nr_hashmap_get(
        *h, p->package_name, nr_strlen(p->package_name));
    if (NULL != package) {
      if (0 != nr_strcmp(package->package_version, p->package_version)) {
        nr_free(package->package_version);
        package->package_version = nr_strdup(p->package_version);
      }
    }
    nr_php_package_destroy(p);
    return;
  }

  nr_hashmap_set(*h, p->package_name, nr_strlen(p->package_name), p);
}

void nr_php_packages_destroy(nr_php_packages_t** h) {
  nr_hashmap_destroy(h);
}

size_t nr_php_packages_count(nr_php_packages_t* h) {
  return nr_hashmap_count(h);
}

size_t nr_hashmap_count_buckets(const nr_php_packages_t* hashmap) {
  return (size_t)(1 << hashmap->log2_num_buckets);
}

int nr_php_packages_has_package(nr_php_packages_t* h,
                                char* package_name,
                                size_t package_len) {
  return nr_hashmap_has(h, package_name, package_len);
}

char* nr_php_package_to_json(nr_php_package_t* package) {
  char* json;
  nrbuf_t* buf = NULL;

  if (package == NULL || package->package_name == NULL
      || package->package_version == NULL) {
    return NULL;
  }
  buf = nr_buffer_create(0, 0);
  nr_buffer_add(buf, NR_PSTR("{\"name\":\""));
  nr_buffer_add(buf, package->package_name, nr_strlen(package->package_name));
  nr_buffer_add(buf, NR_PSTR("\",\"version\":\""));
  nr_buffer_add(buf, package->package_version,
                nr_strlen(package->package_version));
  nr_buffer_add(buf, NR_PSTR("\"}"));
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
    for (bucket = h->buckets[i]; bucket != NULL; bucket = bucket->next) {
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
