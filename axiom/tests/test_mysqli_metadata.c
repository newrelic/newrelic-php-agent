/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

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

static void test_set_user(void) {
  nr_mysqli_metadata_link_t link;
  nr_mysqli_metadata_t* metadata = NULL;

  metadata = nr_mysqli_metadata_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure(
      "NULL metadata", nr_mysqli_metadata_set_user(NULL, 1, "u", "p", "d"));
  tlib_pass_if_status_failure(
      "NULL user", nr_mysqli_metadata_set_user(metadata, 1, NULL, "p", "d"));

  /*
   * Test : The user, password and database replace the connect values; the
   * host, port and options are kept.
   */
  nr_mysqli_metadata_set_connect(metadata, 1, "db-host", "db-user",
                                 "db-password", "db-database", 3306, NULL, 0);
  nr_mysqli_metadata_set_option(metadata, 1, 7, "opt");
  tlib_pass_if_status_success(
      "set user", nr_mysqli_metadata_set_user(metadata, 1, "new-user",
                                              "new-password", "new-database"));
  tlib_pass_if_status_success("get",
                              nr_mysqli_metadata_get(metadata, 1, &link));
  tlib_pass_if_str_equal("user", "new-user", link.user);
  tlib_pass_if_str_equal("password", "new-password", link.password);
  tlib_pass_if_str_equal("database", "new-database", link.database);
  tlib_pass_if_str_equal("host kept", "db-host", link.host);
  tlib_pass_if_int_equal("port kept", 3306, link.port);
  tlib_pass_if_not_null("options kept", link.options);

  /*
   * Test : An omitted password or database clears the earlier value.
   */
  tlib_pass_if_status_success(
      "set user without password and database",
      nr_mysqli_metadata_set_user(metadata, 1, "other-user", NULL, NULL));
  nr_mysqli_metadata_get(metadata, 1, &link);
  tlib_pass_if_str_equal("user", "other-user", link.user);
  tlib_pass_if_null("password cleared", link.password);
  tlib_pass_if_null("database cleared", link.database);

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

static void test_remove(void) {
  nr_mysqli_metadata_t* metadata = nr_mysqli_metadata_create();
  nr_mysqli_metadata_link_t link;

  /* Bad parameters do nothing. */
  nr_mysqli_metadata_remove(NULL, 1);
  nr_mysqli_metadata_remove(metadata, 1);
  tlib_pass_if_status_failure("nothing to remove",
                              nr_mysqli_metadata_get(metadata, 1, &link));

  nr_mysqli_metadata_set_connect(metadata, 1, "db1", "u1", "secret1", "d1",
                                 3306, NULL, 0);
  nr_mysqli_metadata_set_connect(metadata, 2, "db2", "u2", "secret2", "d2",
                                 3306, NULL, 0);

  nr_mysqli_metadata_remove(metadata, 1);
  tlib_pass_if_status_failure("link 1 is gone",
                              nr_mysqli_metadata_get(metadata, 1, &link));
  tlib_pass_if_status_success("link 2 is kept",
                              nr_mysqli_metadata_get(metadata, 2, &link));
  tlib_pass_if_str_equal("link 2 host", "db2", link.host);
  tlib_pass_if_str_equal("link 2 password", "secret2", link.password);

  /*
   * A link on the same handle that omits the password must not inherit the
   * earlier link's.
   */
  nr_mysqli_metadata_set_connect(metadata, 1, "db3", "u3", NULL, "d3", 3306,
                                 NULL, 0);
  tlib_pass_if_status_success("new link 1",
                              nr_mysqli_metadata_get(metadata, 1, &link));
  tlib_pass_if_str_equal("new link 1 host", "db3", link.host);
  tlib_pass_if_null("new link 1 has no password", link.password);

  nr_mysqli_metadata_destroy(&metadata);
}

static void test_password_survives_updates(void) {
  nr_mysqli_metadata_t* metadata = nr_mysqli_metadata_create();
  nr_mysqli_metadata_link_t link;

  /*
   * Every setter works on a copy of the link and saves it again, which copies
   * the password. It must come out unchanged.
   */
  nr_mysqli_metadata_set_connect(metadata, 1, "host", "user", "secret", "db",
                                 3306, NULL, 0);
  nr_mysqli_metadata_set_database(metadata, 1, "other-db");
  nr_mysqli_metadata_set_option(metadata, 1, 7, "opt");
  nr_mysqli_metadata_set_ssl(metadata, 1, "key", "cert", "ca", NULL, NULL);
  nr_mysqli_metadata_set_connect(metadata, 2, "host2", "user2", "secret2",
                                 "db2", 3306, NULL, 0);
  nr_mysqli_metadata_remove(metadata, 2);

  tlib_pass_if_status_success("get",
                              nr_mysqli_metadata_get(metadata, 1, &link));
  tlib_pass_if_str_equal("password", "secret", link.password);
  tlib_pass_if_str_equal("database", "other-db", link.database);

  nr_mysqli_metadata_destroy(&metadata);
}

static void test_complete(void) {
  nr_mysqli_metadata_t* metadata = nr_mysqli_metadata_create();
  nr_mysqli_metadata_link_t link;

  /* A setter can make an entry for a link that was never seen connecting. */
  nr_mysqli_metadata_set_database(metadata, 1, "db");
  tlib_pass_if_status_success("get",
                              nr_mysqli_metadata_get(metadata, 1, &link));
  tlib_pass_if_int_equal("database only is not complete", 0, link.complete);

  /* Options and TLS settings set before the connect are part of the link. */
  nr_mysqli_metadata_set_option(metadata, 2, 7, "opt");
  nr_mysqli_metadata_set_ssl(metadata, 2, "key", NULL, NULL, NULL, NULL);
  nr_mysqli_metadata_get(metadata, 2, &link);
  tlib_pass_if_int_equal("options only is not complete", 0, link.complete);
  nr_mysqli_metadata_set_connect(metadata, 2, "host", "user", "pw", "db", 3306,
                                 NULL, 0);
  nr_mysqli_metadata_get(metadata, 2, &link);
  tlib_pass_if_int_equal("connect completes it", 1, link.complete);
  tlib_pass_if_not_null("options kept", link.options);
  tlib_pass_if_str_equal("ssl key kept", "key", link.ssl_key);

  nr_mysqli_metadata_destroy(&metadata);
}

static void test_set_connect_replaces(void) {
  nr_mysqli_metadata_t* metadata = nr_mysqli_metadata_create();
  nr_mysqli_metadata_link_t link;

  nr_mysqli_metadata_set_option(metadata, 1, 7, "opt");
  nr_mysqli_metadata_set_ssl(metadata, 1, "key", "cert", NULL, NULL, NULL);
  nr_mysqli_metadata_set_connect(metadata, 1, "host1", "user1", "pw1", "db1",
                                 3306, "sock1", 5);

  /* A second connect that omits values clears the earlier ones. */
  nr_mysqli_metadata_set_connect(metadata, 1, "host2", "user2", NULL, NULL, 0,
                                 NULL, 0);
  tlib_pass_if_status_success("get",
                              nr_mysqli_metadata_get(metadata, 1, &link));
  tlib_pass_if_str_equal("host", "host2", link.host);
  tlib_pass_if_str_equal("user", "user2", link.user);
  tlib_pass_if_null("password cleared", link.password);
  tlib_pass_if_null("database cleared", link.database);
  tlib_pass_if_null("socket cleared", link.socket);
  tlib_pass_if_int_equal("flags", 0, (int)link.flags);

  /* The options and the TLS settings are not connection parameters. */
  tlib_pass_if_not_null("options kept", link.options);
  tlib_pass_if_str_equal("ssl key kept", "key", link.ssl_key);
  tlib_pass_if_str_equal("ssl cert kept", "cert", link.ssl_cert);

  nr_mysqli_metadata_destroy(&metadata);
}

static void test_remove_many(void) {
  nr_mysqli_metadata_t* metadata = nr_mysqli_metadata_create();
  nr_mysqli_metadata_link_t link;
  int i;

  for (i = 1; i <= 5; i++) {
    char user[8];
    char pw[8];

    snprintf(user, sizeof(user), "u%d", i);
    snprintf(pw, sizeof(pw), "pw%d", i);
    nr_mysqli_metadata_set_connect(metadata, i, "host", user, pw, "db", 3306,
                                   NULL, 0);
  }

  /* Middle, first and last. The others keep their data, password included. */
  nr_mysqli_metadata_remove(metadata, 3);
  nr_mysqli_metadata_remove(metadata, 1);
  nr_mysqli_metadata_remove(metadata, 5);
  tlib_pass_if_status_failure("1 gone",
                              nr_mysqli_metadata_get(metadata, 1, &link));
  tlib_pass_if_status_failure("3 gone",
                              nr_mysqli_metadata_get(metadata, 3, &link));
  tlib_pass_if_status_failure("5 gone",
                              nr_mysqli_metadata_get(metadata, 5, &link));
  tlib_pass_if_status_success("2 kept",
                              nr_mysqli_metadata_get(metadata, 2, &link));
  tlib_pass_if_str_equal("2 password", "pw2", link.password);
  tlib_pass_if_status_success("4 kept",
                              nr_mysqli_metadata_get(metadata, 4, &link));
  tlib_pass_if_str_equal("4 password", "pw4", link.password);

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
  test_set_user();
  test_set_ssl();
  test_remove();
  test_password_survives_updates();
  test_complete();
  test_set_connect_replaces();
  test_remove_many();
  test_id();
  test_copy();
}
