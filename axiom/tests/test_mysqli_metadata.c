/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_mysqli_metadata.h"
#include "nr_mysqli_metadata_private.h"
#include "util_object.h"
#include "util_strings.h"

#include "tlib_main.h"

static void test_create(void) {
  nr_mysqli_metadata_t* metadata;

  metadata = nr_mysqli_metadata_create();
  tlib_pass_if_not_null("pointer", metadata);
  tlib_pass_if_not_null("connections", metadata->links);
  tlib_fail_if_int_equal("connections type", 0,
                         NR_OBJECT_HASH == nro_type(metadata->links));

  nr_mysqli_metadata_destroy(&metadata);
}

static void test_destroy(void) {
  nr_mysqli_metadata_t* metadata = NULL;

  /*
   * Test : Bad parameters.
   */
  nr_mysqli_metadata_destroy(NULL);
  nr_mysqli_metadata_destroy(&metadata);

  /*
   * Test : Normal operation.
   */
  metadata = nr_mysqli_metadata_create();
  nr_mysqli_metadata_destroy(&metadata);
  tlib_pass_if_null("pointer", metadata);
}

static void test_get(void) {
  nr_mysqli_metadata_link_t link;
  nr_mysqli_metadata_t* metadata = NULL;

  metadata = nr_mysqli_metadata_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure("NULL metadata",
                              nr_mysqli_metadata_get(NULL, 1, &link));
  tlib_pass_if_status_failure("NULL link",
                              nr_mysqli_metadata_get(metadata, 1, NULL));
  tlib_pass_if_status_failure("missing link",
                              nr_mysqli_metadata_get(metadata, 1, &link));

  /*
   * Test : Normal operation.
   */
  nr_mysqli_metadata_set_connect(metadata, 1, NULL, NULL, NULL, NULL, 0, NULL,
                                 0);
  tlib_pass_if_status_success("NULL fields",
                              nr_mysqli_metadata_get(metadata, 1, &link));
  tlib_pass_if_null("host", link.host);
  tlib_pass_if_null("user", link.user);
  tlib_pass_if_null("password", link.password);
  tlib_pass_if_null("database", link.database);
  tlib_pass_if_null("socket", link.socket);
  tlib_pass_if_int_equal("port", 0, (int)link.port);
  tlib_pass_if_long_equal("flags", 0, (int)link.flags);

  nr_mysqli_metadata_set_connect(metadata, 1, "db-host", "db-user",
                                 "db-password", "db-database", 3306,
                                 "db-socket", 1);
  nr_mysqli_metadata_set_option(metadata, 1, 2, "foo");
  tlib_pass_if_status_success("set fields",
                              nr_mysqli_metadata_get(metadata, 1, &link));
  tlib_pass_if_str_equal("host", "db-host", link.host);
  tlib_pass_if_str_equal("user", "db-user", link.user);
  tlib_pass_if_str_equal("password", "db-password", link.password);
  tlib_pass_if_str_equal("socket", "db-socket", link.socket);
  tlib_pass_if_str_equal("database", "db-database", link.database);
  tlib_pass_if_int_equal("port", 3306, (int)link.port);
  tlib_pass_if_long_equal("flags", 1, (int)link.flags);
  tlib_pass_if_not_null("options", link.options);
  tlib_pass_if_int_equal("options", 1, nro_getsize(link.options));
  tlib_pass_if_long_equal(
      "option", 2,
      nro_get_hash_long(nro_get_array_hash(link.options, 1, NULL), "option",
                        NULL));
  tlib_pass_if_str_equal(
      "value", "foo",
      nro_get_hash_string(nro_get_array_hash(link.options, 1, NULL), "value",
                          NULL));

  nr_mysqli_metadata_destroy(&metadata);
}

static void test_set_connect(void) {
  const nrobj_t* link = NULL;
  nr_mysqli_metadata_t* metadata = NULL;

  metadata = nr_mysqli_metadata_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure(
      "NULL metadata", nr_mysqli_metadata_set_connect(NULL, 1, NULL, NULL, NULL,
                                                      NULL, 0, NULL, 0));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_status_success(
      "NULL fields", nr_mysqli_metadata_set_connect(metadata, 1, NULL, NULL,
                                                    NULL, NULL, 0, NULL, 0));
  link = nro_get_hash_value(metadata->links, "1", NULL);
  tlib_pass_if_not_null("link", link);
  tlib_pass_if_null("host", nro_get_hash_string(link, "host", NULL));
  tlib_pass_if_null("user", nro_get_hash_string(link, "user", NULL));
  tlib_pass_if_null("password", nro_get_hash_string(link, "password", NULL));
  tlib_pass_if_null("database", nro_get_hash_string(link, "database", NULL));
  tlib_pass_if_null("socket", nro_get_hash_string(link, "socket", NULL));
  tlib_pass_if_int_equal("port", 0, nro_get_hash_int(link, "port", NULL));
  tlib_pass_if_int64_t_equal("flags", 0,
                             nro_get_hash_long(link, "flags", NULL));

  tlib_pass_if_status_success(
      "set fields", nr_mysqli_metadata_set_connect(
                        metadata, 1, "db-host", "db-user", "db-password",
                        "db-database", 3306, "db-socket", 1));
  link = nro_get_hash_value(metadata->links, "1", NULL);
  tlib_pass_if_not_null("link", link);
  tlib_pass_if_str_equal("host", "db-host",
                         nro_get_hash_string(link, "host", NULL));
  tlib_pass_if_str_equal("user", "db-user",
                         nro_get_hash_string(link, "user", NULL));
  tlib_pass_if_str_equal("password", "db-password",
                         nro_get_hash_string(link, "password", NULL));
  tlib_pass_if_str_equal("database", "db-database",
                         nro_get_hash_string(link, "database", NULL));
  tlib_pass_if_str_equal("socket", "db-socket",
                         nro_get_hash_string(link, "socket", NULL));
  tlib_pass_if_int_equal("port", 3306, nro_get_hash_int(link, "port", NULL));
  tlib_pass_if_int64_t_equal("flags", 1,
                             nro_get_hash_long(link, "flags", NULL));

  nr_mysqli_metadata_destroy(&metadata);
}

static void test_set_database(void) {
  nr_mysqli_metadata_link_t link;
  nr_mysqli_metadata_t* metadata = NULL;

  metadata = nr_mysqli_metadata_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure(
      "NULL metadata", nr_mysqli_metadata_set_database(NULL, 1, "db-name"));
  tlib_pass_if_status_failure(
      "NULL database", nr_mysqli_metadata_set_database(metadata, 1, NULL));

  /*
   * Test: Normal operation.
   */
  tlib_pass_if_status_success(
      "set database", nr_mysqli_metadata_set_database(metadata, 1, "db-name"));
  nr_mysqli_metadata_get(metadata, 1, &link);
  tlib_pass_if_str_equal("database", "db-name", link.database);

  nr_mysqli_metadata_destroy(&metadata);
}

static void test_set_option(void) {
  nr_mysqli_metadata_link_t link;
  nr_mysqli_metadata_t* metadata = NULL;
  const nrobj_t* option;

  metadata = nr_mysqli_metadata_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure("NULL metadata",
                              nr_mysqli_metadata_set_option(NULL, 1, 1, "foo"));
  tlib_pass_if_status_failure(
      "NULL value", nr_mysqli_metadata_set_option(metadata, 1, 1, NULL));

  /*
   * Test: Normal operation.
   */
  tlib_pass_if_status_success(
      "set option", nr_mysqli_metadata_set_option(metadata, 1, 1, "foo"));
  nr_mysqli_metadata_get(metadata, 1, &link);
  tlib_pass_if_int_equal("option count", 1, nro_getsize(link.options));
  option = nro_get_array_hash(link.options, 1, NULL);
  tlib_pass_if_not_null("option hash", option);
  tlib_pass_if_int64_t_equal("option", 1,
                             nro_get_hash_long(option, "option", NULL));
  tlib_pass_if_str_equal("value", "foo",
                         nro_get_hash_string(option, "value", NULL));

  nr_mysqli_metadata_destroy(&metadata);
}

static void test_set_ssl(void) {
  nr_mysqli_metadata_link_t link;
  nr_mysqli_metadata_t* metadata = NULL;

  metadata = nr_mysqli_metadata_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure(
      "NULL metadata", nr_mysqli_metadata_set_ssl(NULL, 1, "key", "cert", "ca",
                                                  "capath", "cipher"));

  /*
   * Test : No link metadata yet: ssl_set creates it, like mysqli_options().
   */
  tlib_pass_if_status_failure("no link",
                              nr_mysqli_metadata_get(metadata, 1, &link));
  tlib_pass_if_status_success(
      "set ssl", nr_mysqli_metadata_set_ssl(metadata, 1, "key", "cert", "ca",
                                            "capath", "cipher"));
  tlib_pass_if_status_success("get",
                              nr_mysqli_metadata_get(metadata, 1, &link));
  tlib_pass_if_str_equal("key", "key", link.ssl_key);
  tlib_pass_if_str_equal("cert", "cert", link.ssl_cert);
  tlib_pass_if_str_equal("ca", "ca", link.ssl_ca);
  tlib_pass_if_str_equal("capath", "capath", link.ssl_capath);
  tlib_pass_if_str_equal("cipher", "cipher", link.ssl_cipher);

  /*
   * Test : A link that never called ssl_set has no ssl values.
   */
  nr_mysqli_metadata_set_connect(metadata, 2, "db-host", "db-user",
                                 "db-password", "db-database", 3306, NULL, 0);
  tlib_pass_if_status_success("get link 2",
                              nr_mysqli_metadata_get(metadata, 2, &link));
  tlib_pass_if_null("key", link.ssl_key);
  tlib_pass_if_null("cert", link.ssl_cert);
  tlib_pass_if_null("ca", link.ssl_ca);
  tlib_pass_if_null("capath", link.ssl_capath);
  tlib_pass_if_null("cipher", link.ssl_cipher);

  /*
   * Test : A later call replaces all of the values, as in PHP: omitted
   * values are cleared.
   */
  nr_mysqli_metadata_set_ssl(metadata, 1, NULL, NULL, "other-ca", NULL, NULL);
  nr_mysqli_metadata_get(metadata, 1, &link);
  tlib_pass_if_null("key cleared", link.ssl_key);
  tlib_pass_if_null("cert cleared", link.ssl_cert);
  tlib_pass_if_str_equal("ca replaced", "other-ca", link.ssl_ca);
  tlib_pass_if_null("capath cleared", link.ssl_capath);
  tlib_pass_if_null("cipher cleared", link.ssl_cipher);

  /*
   * Test : ssl values survive connect and option calls on the same link, and
   * the connect values survive an ssl_set call.
   */
  nr_mysqli_metadata_set_ssl(metadata, 3, "key", "cert", "ca", NULL, NULL);
  nr_mysqli_metadata_set_option(metadata, 3, 7, "opt");
  nr_mysqli_metadata_set_connect(metadata, 3, "db-host", "db-user",
                                 "db-password", NULL, 0, NULL, 2048);
  nr_mysqli_metadata_get(metadata, 3, &link);
  tlib_pass_if_str_equal("key kept", "key", link.ssl_key);
  tlib_pass_if_str_equal("ca kept", "ca", link.ssl_ca);
  tlib_pass_if_str_equal("host kept", "db-host", link.host);
  tlib_pass_if_long_equal("flags kept", 2048, link.flags);
  tlib_pass_if_int_equal("option count", 1, nro_getsize(link.options));
  nr_mysqli_metadata_set_ssl(metadata, 3, "key2", NULL, NULL, NULL, NULL);
  nr_mysqli_metadata_get(metadata, 3, &link);
  tlib_pass_if_str_equal("key replaced", "key2", link.ssl_key);
  tlib_pass_if_str_equal("host kept after ssl_set", "db-host", link.host);
  tlib_pass_if_int_equal("option kept after ssl_set", 1,
                         nro_getsize(link.options));

  nr_mysqli_metadata_destroy(&metadata);
}

static void test_id(void) {
  char id[NR_MYSQLI_METADATA_ID_SIZE];

  /*
   * Test : Bad parameters.
   */
  nr_mysqli_metadata_id(1, NULL);

  /*
   * Test : Normal operation.
   */
  nr_mysqli_metadata_id(0, id);
  tlib_pass_if_str_equal("0", "0", id);

  nr_mysqli_metadata_id(1, id);
  tlib_pass_if_str_equal("1", "1", id);
}

static void test_copy(void) {
  nr_mysqli_metadata_t* src = NULL;
  nr_mysqli_metadata_t* dest = NULL;
  const nrobj_t* link = NULL;

  src = nr_mysqli_metadata_create();

  nr_mysqli_metadata_set_connect(src, 1, "db-host", "db-user", "db-password",
                                 "db-database", 3306, "db-socket", 1);

  dest = nr_mysqli_metadata_copy(src);

  tlib_pass_if_not_null("'dest' metadata is not NULL", dest);

  link = nro_get_hash_value(dest->links, "1", NULL);

  tlib_pass_if_not_null("link", link);
  tlib_pass_if_str_equal("host", "db-host",
                         nro_get_hash_string(link, "host", NULL));
  tlib_pass_if_str_equal("user", "db-user",
                         nro_get_hash_string(link, "user", NULL));
  tlib_pass_if_str_equal("password", "db-password",
                         nro_get_hash_string(link, "password", NULL));
  tlib_pass_if_str_equal("database", "db-database",
                         nro_get_hash_string(link, "database", NULL));
  tlib_pass_if_str_equal("socket", "db-socket",
                         nro_get_hash_string(link, "socket", NULL));
  tlib_pass_if_int_equal("port", 3306, nro_get_hash_int(link, "port", NULL));
  tlib_pass_if_int64_t_equal("flags", 1,
                             nro_get_hash_long(link, "flags", NULL));

  nr_mysqli_metadata_destroy(&src);
  nr_mysqli_metadata_destroy(&dest);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_create();
  test_destroy();
  test_get();
  test_set_connect();
  test_set_database();
  test_set_option();
  test_set_ssl();
  test_id();
  test_copy();
}
