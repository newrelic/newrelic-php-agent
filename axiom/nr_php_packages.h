/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_PHP_PACKAGES_HDR
#define NR_PHP_PACKAGES_HDR

#include "nr_log_event.h"
#include "util_random.h"
#include "util_vector.h"
#include "util_hashmap.h"

typedef struct _nr_php_package_t {
  char* package_name;
  char* package_version;
} nr_php_package_t;

typedef nr_hashmap_t nr_php_packages_t;

/*
 * Purpose : Create a new php package
 *
 * Params  : 1. Package name
 *           2. Package version
 *
 * Returns : A php package that has a name and version. If
 *           nr_php_packages_add_package() is not called, then it must be freed
 *           by nr_php_package_destroy()
 */
extern nr_php_package_t* nr_php_package_create(char* name, char* version);

/*
 * Purpose : Destroy/free php package
 *
 * Params  : The php package to free
 *
 * Returns : Nothing, it is void
 */
extern void nr_php_package_destroy(nr_php_package_t* p);

/*
 * Purpose : Add new php package to collection. If the collection does not exist
 *           and this is the first package being added, then a new collection
 *           will be created. If a package with the same key but different value
 *           is added, then the old value will be freed and the newer value will
 *           be kept. If a duplicate package with the same key and value is
 *           added, no action will be taken. This function also frees the
 *           package being added, so the user does not need to call
 *           nr_php_package_destroy()
 *
 * Params  : 1. A pointer to the pointer to nr_php_packages_t where packages
 *              will be added
 *           2. A pointer to the php package that needs to be added to the
 *              collection
 *
 * Returns : Nothing, it is void
 */
extern void nr_php_packages_add_package(nr_php_packages_t** h,
                                        nr_php_package_t* p);

/*
 * Purpose : Destroy/free the collection
 *
 * Params  : 1. A pointer to the pointer of nr_php_packages_t
 *
 * Returns : Nothing, it is void
 */
static inline void nr_php_packages_destroy(nr_php_packages_t** h) {
  nr_hashmap_destroy(h);
}

/*
 * Purpose : Count how many elements are inside of the collection
 *
 * Params  : 1. A pointer to nr_php_packages_t
 *
 * Returns : The number of elements in the collection
 */
static inline size_t nr_php_packages_count(nr_php_packages_t* h) {
  return nr_hashmap_count(h);
}

/*
 * Purpose : Check if a php package exists in the collection
 *
 * Params  : 1. A pointer to nr_php_packages_t
 *           2. The package to check
 *           3. Length of package to check
 *
 * Returns : Returns non-zero if the package exists
 */
static inline int nr_php_packages_has_package(nr_php_packages_t* h,
                                              char* package_name,
                                              size_t package_len) {
  return nr_hashmap_has(h, package_name, package_len);
}

/*
 * Purpose : Converts a package to a json
 *
 * Params  : 1. A pointer to the package
 *
 * Returns : Returns the package in json format
 */
extern char* nr_php_package_to_json(nr_php_package_t* package);

/*
 * Purpose : Iterates through all of the php packages in the collection and adds
 *           them to a buffer in JSON format.
 *
 * Params  : 1. A pointer to nr_php_packages_t
 *           2. The buffer to append too
 *
 * Returns : Returns true on success
 */
extern bool nr_php_packages_to_json_buffer(nr_php_packages_t* hashmap,
                                           nrbuf_t* buf);

/*
 * Purpose : Returns all of the packages in the collection as a JSON
 *
 * Params  : 1. A pointer to nr_php_packages_t
 *
 * Returns : A JSON
 */
extern char* nr_php_packages_to_json(nr_php_packages_t* h);

/*
 * Purpose : Returns how many buckets are in the collection
 *
 * Params  : 1. A pointer to nr_php_packages_t
 *
 * Returns : The number of buckets in the collection
 */
extern size_t nr_hashmap_count_buckets(const nr_php_packages_t* hashmap);

#endif /* nr_php_packages_HDR */
