/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_datastore_instance.h"
#include "php_agent.h"
#include "php_pdo.h"
#include "php_pdo_mysql.h"
#include "util_system.h"

static inline char* nr_php_pdo_mysql_default_socket(void) {
  /*
   * It is impossible for pdo_mysql.default_socket to be an empty string, as its
   * modify handler is defined as OnUpdateStringUnempty for all versions of PHP.
   * Similarly, it's impossible for it to be NULL, as if PDO is loaded the
   * configuration setting always exists.
   *
   * Given the above, we can just return the value as is rather than having to
   * check for empty or NULL strings.
   */
  return nr_php_zend_ini_string(NR_PSTR("pdo_mysql.default_socket"), 0);
}

nr_datastore_instance_t* nr_php_pdo_mysql_create_datastore_instance(
    pdo_dbh_t* dbh TSRMLS_DC) {
  nr_datastore_instance_t* instance = NULL;
  char* host = NULL;
  char* port_path_or_id = NULL;
  char* database_name = NULL;

  /*
   * This isn't the full set of possible keys, even for MySQL, but these are the
   * ones we need to get the instance metadata. php_pdo_parse_data_source()
   * silently drops key/value pairs that don't exist in this structure, so we
   * can afford to provide a subset.
   *
   * The default values match those in the PDO MySQL driver.
   */
  struct pdo_data_src_parser vars[] = {
      {"dbname", "", 0},
      {"host", "localhost", 0},
      {"port", "3306", 0},
      {"unix_socket", nr_php_pdo_mysql_default_socket(), 0},
  };
  size_t var_len = sizeof(vars) / sizeof(struct pdo_data_src_parser);

  NR_UNUSED_TSRMLS;

  if (NULL == dbh) {
    return NULL;
  }

  if (NR_SUCCESS
      != nr_php_pdo_parse_data_source(dbh->data_source, dbh->data_source_len,
                                      vars, var_len)) {
    goto end;
  }

  database_name = nr_strdup(vars[0].optval);
  host = nr_strdup(vars[1].optval);

  if (0 == nr_strcmp(vars[1].optval, "localhost")) {
    /*
     * As in earlier MySQL extensions, the specific string "localhost" as the
     * host name triggers different behaviour in the PDO MySQL driver: namely,
     * looking solely at the UNIX socket path rather than the port number when
     * trying to connect.
     */
    if (!nr_strempty(vars[3].optval)) {
      port_path_or_id = nr_strdup(vars[3].optval);
    } else {
      port_path_or_id = nr_strdup("default");
    }
  } else if (vars[2].optval) {
    port_path_or_id = nr_strdup(vars[2].optval);
  } else {
    port_path_or_id = nr_strdup("default");
  }

  instance = nr_datastore_instance_create(host, port_path_or_id, database_name);

end:
  nr_php_pdo_free_data_sources(vars, var_len);
  nr_free(host);
  nr_free(port_path_or_id);
  nr_free(database_name);
  return instance;
}
