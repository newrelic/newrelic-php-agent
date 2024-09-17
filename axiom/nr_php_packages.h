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

#define PHP_PACKAGE_VERSION_UNKNOWN " "

typedef enum {
  NR_PHP_PACKAGE_SOURCE_LEGACY,
  NR_PHP_PACKAGE_SOURCE_COMPOSER
} nr_php_package_source_priority_t;

typedef struct _nr_php_package_t {
  char* package_name;
  char* package_version;
  nr_php_package_source_priority_t source_priority;
} nr_php_package_t;

typedef struct _nr_php_packages_t {
  nr_hashmap_t* data;
} nr_php_packages_t;

/*
 * Purpose : Create a new php package with desired source priority. If the name is null, then no package will
 *           be created. If the version is null (version = NULL), then
 *           the package will still be created and the version will be set to an
 *           empty string with a space.
 *
 * Params  : 1. Package name
 *           2. Package version
 *           3. Package source priority (legacy or composer)
 *
 * Returns : A php package that has a name and version. If
 *           nr_php_packages_add_package() is not called, then it must be freed
 *           by nr_php_package_destroy()
 */
extern nr_php_package_t* nr_php_package_create_with_source(char* name, char* version, const nr_php_package_source_priority_t source_priority);

/*
 * Purpose : Create a new php package with legacy source priority. If the name is null, then no package will
 *           be created. If the version is null (version = NULL), then
 *           the package will still be created and the version will be set to an
 *           empty string with a space.
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
 * Returns : Nothing
 */
extern void nr_php_package_destroy(nr_php_package_t* p);

/*
 * Purpose : Allocate memory for new collection that will hold packages
 *
 * Returns : A collection that is allocated of type nr_php_packages_t
 */
extern nr_php_packages_t* nr_php_packages_create(void);

/*
 * Purpose : Add new php package to collection. If a package with the same key
 *           but different value is added, then the newer value will be kept.
 *           Regardless of whether or not there is a name collision,
 *           the caller is not responsible for destroying the package
 *
 * Params  : 1. A pointer to nr_php_packages_t where packages
 *              will be added
 *           2. A pointer to the php package that needs to be added to the
 *              collection
 *
 * Returns : Nothing
 */
extern void nr_php_packages_add_package(nr_php_packages_t* h,
                                        nr_php_package_t* p);

/*
 * Purpose : Destroy/free the collection
 *
 * Params  : 1. A pointer to the pointer of nr_php_packages_t
 *
 * Returns : Nothing
 */
static inline void nr_php_packages_destroy(nr_php_packages_t** h) {
  if (nrlikely(NULL != h && NULL != *h)) {
    if (NULL != (*h)->data) {
      nr_hashmap_destroy(&(*h)->data);
    }
    nr_free(*h);
    *h = NULL;
  }
}

/*
 * Purpose : Count how many elements are inside of the collection
 *
 * Params  : 1. A pointer to nr_php_packages_t
 *
 * Returns : The number of elements in the collection
 */
static inline size_t nr_php_packages_count(nr_php_packages_t* h) {
  if (nrlikely(NULL != h && NULL != h->data)) {
    return nr_hashmap_count(h->data);
  }
  return 0;
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
  if (nrlikely(NULL != h && NULL != h->data)) {
    return nr_hashmap_has(h->data, package_name, package_len);
  }
  return 0;
}

/*
 * Purpose : Converts a package to a json
 *
 * Params  : 1. A pointer to the package
 *
 * Returns : An allocated string containing the JSON representation of the
 *           package. Caller takes ownership of this string.
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
 * Returns : An allocated string containing the JSON representation of the
 *           packages collection. Caller takes ownership of this string.
 */
extern char* nr_php_packages_to_json(nr_php_packages_t* h);

#endif /* nr_php_packages_HDR */
