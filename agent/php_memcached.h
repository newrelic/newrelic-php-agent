/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_MEMCACHED_HDR
#define PHP_MEMCACHED_HDR

#include "nr_datastore_instance.h"
#include "php_includes.h"

extern nr_datastore_instance_t* nr_php_memcached_create_datastore_instance(
    const char* host,
    zend_long port);

#endif
