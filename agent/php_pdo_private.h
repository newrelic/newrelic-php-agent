/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PHP_PDO_PRIVATE_HDR
#define PHP_PDO_PRIVATE_HDR

/*
 * Mappings from PDO driver names to New Relic datastore constants.
 */
typedef struct {
  const char* driver_name;
  nr_datastore_t datastore;
} nr_php_pdo_datastore_mapping_t;

static const nr_php_pdo_datastore_mapping_t nr_php_pdo_datastore_mappings[] = {
    /*
     * We use NR_DATASTORE_PDO as a placeholder for the unusual drivers,
     * and use specific names for the usual databases which match other agents.
     * http://php.net/manual/en/pdo.drivers.php
     * For the actual name used, search php-src for PDO_DRIVER_HEADER
     */
    {"mysql", NR_DATASTORE_MYSQL},
    {"pgsql", NR_DATASTORE_POSTGRES},
    {"oci", NR_DATASTORE_ORACLE},
    {"sqlite", NR_DATASTORE_SQLITE},
    {"sqlite2", NR_DATASTORE_SQLITE},
    {"mssql", NR_DATASTORE_MSSQL},
    {"dblib", NR_DATASTORE_MSSQL},
    {"firebird", NR_DATASTORE_FIREBIRD},
    {"odbc", NR_DATASTORE_ODBC},
    {"sybase", NR_DATASTORE_SYBASE},
    {"informix", NR_DATASTORE_INFORMIX},
    {"sqlsrv", NR_DATASTORE_MSSQL},

    /*
     * The last element should always have a driver_name of NULL.
     */
    {NULL, NR_DATASTORE_PDO},
};

/*
 * Purpose : Create a unique key for the given PDO connection in a format
 *           usable by the datastore instance implementation.
 *
 * Params  : 1. The PDO connection.
 *
 * Returns : An allocated string, which the caller will own, or NULL if an
 *           error occurred.
 */
extern char* nr_php_pdo_datastore_make_key(pdo_dbh_t* dbh);

/*
 * Purpose : Return the pdo_dbh_t struct for either a PDO or PDOStatement
 *           object.
 *
 * Params  : 1. A PDO or PDOStatement object.
 *
 * Returns : A pointer to the internal pdo_dbh_t structure, or NULL on error.
 */
extern pdo_dbh_t* nr_php_pdo_get_database_object_from_object(
    zval* obj TSRMLS_DC);

/*
 * Purpose : Return the datastore that corresponds to the given PDO driver
 *           name.
 *
 * Params  : 1. The PDO driver name.
 *
 * Returns : The datastore constant, or NR_DATASTORE_PDO if the connection type
 *           is unknown.
 */
extern nr_datastore_t nr_php_pdo_get_datastore_for_driver(
    const char* driver_name);

/*
 * Purpose : Return the datastore that corresponds to the given PDO connection.
 *
 * Params  : 1. The PDO connection.
 *
 * Returns : The datastore constant, or NR_DATASTORE_PDO if the connection type
 *           is unknown.
 */
extern nr_datastore_t nr_php_pdo_get_datastore_internal(pdo_dbh_t* dbh);

/*
 * Purpose : Return the driver name for the given PDO connection.
 *
 * Params  : 1. The PDO connection.
 *
 * Returns : The driver name, or NULL on error.
 */
extern const char* nr_php_pdo_get_driver_internal(pdo_dbh_t* dbh);

/*
 * Purpose : Iterator function to take the given bound parameter and apply it
 *           to another PDOStatement object.
 *
 * Params  : 1. The bound parameter.
 *           2. The object to bind it to.
 *           3. The parameter name.
 *
 * Returns : ZEND_HASH_KEY_APPLY (to continue iteration).
 */
extern int nr_php_pdo_rebind_apply_parameter(struct pdo_bound_param_data* param,
                                             zval* stmt,
                                             zend_hash_key* hash_key TSRMLS_DC);

/*
 * Purpose : Get the internal pdo_dbh_t struct for the given PDO object.
 *
 * Params  : 1. The PDO object.
 *
 * Returns : A pointer to the pdo_dbh_t struct, or NULL on error.
 *
 * Warning : This function does NOT check if dbh is NULL, since most users will
 *           already have done so.
 */
static inline pdo_dbh_t* nr_php_pdo_get_database_object_internal(
    zval* dbh TSRMLS_DC) {
#ifdef PHP7
  return Z_PDO_DBH_P(dbh);
#else
  return (pdo_dbh_t*)zend_object_store_get_object(dbh TSRMLS_CC);
#endif /* PHP7 */
}

/*
 * Purpose : Get the internal pdo_stmt_t struct for the given PDOStatement
 *           object.
 *
 * Params  : 1. The PDOStatement object.
 *
 * Returns : A pointer to the pdo_stmt_t struct, or NULL on error.
 *
 * Warning : This function does NOT check if stmt is NULL, since most users will
 *           already have done so.
 */
static inline pdo_stmt_t* nr_php_pdo_get_statement_object_internal(
    zval* stmt TSRMLS_DC) {
#ifdef PHP7
  return Z_PDO_STMT_P(stmt);
#else
  return (pdo_stmt_t*)zend_object_store_get_object(stmt TSRMLS_CC);
#endif /* PHP7 */
}

/*
 * Purpose : Copy the given PDO options, disabling persistence if enabled.
 *
 * Params  : 1. An array zval of options to PDO::__construct().
 *
 * Returns : A copy of the original options with persistence disabled, which is
 *           owned by the caller, or NULL on error.
 */
extern zval* nr_php_pdo_disable_persistence(const zval* options TSRMLS_DC);

#endif /* PHP_PDO_PRIVATE_HDR */
