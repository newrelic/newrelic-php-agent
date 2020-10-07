/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_datastore_instance.h"
#include "php_agent.h"
#include "php_datastore.h"
#include "php_mysql.h"
#include "php_mysql_private.h"
#include "util_system.h"

/*
 * Port fallback logic:
 * https://github.com/php/php-src/blob/PHP-5.6/ext/mysql/php_mysql.c#L752-L761
 */
char* nr_php_mysql_default_port() {
  char* port = nr_php_zend_ini_string(NR_PSTR("mysql.default_port"), 0);

  if (nr_strempty(port)) {
    port = nr_system_get_service_port("mysql", "tcp");
    if (NULL != port) {
      return port;
    } else {
      port = getenv("MYSQL_TCP_PORT");
    }
  }

  return nr_strdup(port);
}

char* nr_php_mysql_default_host() {
  char* host = nr_php_zend_ini_string(NR_PSTR("mysql.default_host"), 0);

  if (nr_strempty(host)) {
    host = "localhost";
  }

  return nr_strdup(host);
}

char* nr_php_mysql_default_socket() {
  char* socket = nr_php_zend_ini_string(NR_PSTR("mysql.default_socket"), 0);

  return nr_strdup(socket);
}

void nr_php_mysql_get_host_and_port_path_or_id(const char* host_and_port,
                                               char** host_ptr,
                                               char** port_path_or_id_ptr) {
  char* colon;
  char* host = NULL;
  char* port_path_or_id = NULL;

  if ((NULL != *host_ptr) || (NULL != *port_path_or_id_ptr)) {
    return;
  }

  /*
   * If we weren't given a host_and_port, use the default host.
   * https://github.com/php/php-src/blob/PHP-5.6/ext/mysql/php_mysql.c#L791-L792
   */
  if (nr_strempty(host_and_port)) {
    host = nr_php_mysql_default_host();
  } else {
    host = nr_strdup(host_and_port);
  }

  /*
   * If host_and_port contains ":", use the two parts as host and
   * port_path_or_id, otherwise just use host_and_port as host and grab the
   * default port.
   * https://github.com/php/php-src/blob/PHP-5.6/ext/mysql/php_mysql.c#L825-L841
   */
  colon = nr_strchr(host, ':');
  if (NULL != colon) {
    if (!nr_strempty(colon + 1)) {
      port_path_or_id = nr_strdup(colon + 1);
    } else {
      port_path_or_id = nr_php_mysql_default_port();
    }

    *colon = '\0';
    if (nr_strempty(host)) {
      nr_free(host);
      host = nr_php_mysql_default_host();
    }
  } else {
    port_path_or_id = nr_php_mysql_default_port();
  }

  /*
   * Host, port, and socket are all passed to the mysql/mysqlnd driver.
   * https://github.com/php/php-src/blob/PHP-5.6/ext/mysql/php_mysql.c#L888-L891
   *
   * If the host is exactly "localhost" and we were not given a path, use the
   * default socket instead.
   * https://github.com/php/php-src/blob/PHP-5.6/ext/mysqlnd/mysqlnd.c#L961-L967
   */
  if (0 == nr_stricmp(host, "localhost")) {
    if (port_path_or_id[0] != '/') {
      nr_free(port_path_or_id);
      port_path_or_id = nr_php_mysql_default_socket();
    }
  }

  *host_ptr = host;
  *port_path_or_id_ptr = port_path_or_id;
}

nr_datastore_instance_t* nr_php_mysql_create_datastore_instance(
    const char* host_and_port) {
  nr_datastore_instance_t* instance;
  char* host = NULL;
  char* port_path_or_id = NULL;

  nr_php_mysql_get_host_and_port_path_or_id(host_and_port, &host,
                                            &port_path_or_id);
  instance = nr_datastore_instance_create(host, port_path_or_id, NULL);

  nr_free(host);
  nr_free(port_path_or_id);

  return instance;
}

void nr_php_mysql_save_datastore_instance(const zval* mysql_conn,
                                          const char* host_and_port TSRMLS_DC) {
  nr_datastore_instance_t* instance;
  char* key = nr_php_datastore_make_key(mysql_conn, "mysql");

  if (nr_php_datastore_has_conn(key TSRMLS_CC)) {
    nr_free(key);
    return;
  }

  instance = nr_php_mysql_create_datastore_instance(host_and_port);
  nr_php_datastore_instance_save(key, instance TSRMLS_CC);

  nr_free(NRPRG(mysql_last_conn));
  NRPRG(mysql_last_conn) = key;
}

nr_datastore_instance_t* nr_php_mysql_retrieve_datastore_instance(
    const zval* mysql_conn TSRMLS_DC) {
  nr_datastore_instance_t* instance;
  char* key = NULL;

  if (NULL == mysql_conn) {
    /*
     * If we have an existing connection, use that as the key. Otherwise,
     * create a default mysql instance and make a key with the NULL zval.
     */
    if (NRPRG(mysql_last_conn)) {
      key = nr_strdup(NRPRG(mysql_last_conn));
    } else {
      nr_php_mysql_save_datastore_instance(mysql_conn, NULL TSRMLS_CC);
      key = nr_php_datastore_make_key(mysql_conn, "mysql");
    }
  } else {
    key = nr_php_datastore_make_key(mysql_conn, "mysql");
  }

  instance = nr_php_datastore_instance_retrieve(key TSRMLS_CC);
  nr_free(key);

  return instance;
}

void nr_php_mysql_remove_datastore_instance(const zval* mysql_conn TSRMLS_DC) {
  char* key = NULL;

  /*
   * If the connection is NULL but we have an existing connection, use that as
   * the key. Otherwise make a key with the NULL zval.
   */
  if ((NULL == mysql_conn) && (NRPRG(mysql_last_conn))) {
    key = nr_strdup(NRPRG(mysql_last_conn));
  } else {
    key = nr_php_datastore_make_key(mysql_conn, "mysql");
  }

  nr_php_datastore_instance_remove(key TSRMLS_CC);

  if (0 == nr_strcmp(key, NRPRG(mysql_last_conn))) {
    nr_free(NRPRG(mysql_last_conn));
  }

  nr_free(key);
}
