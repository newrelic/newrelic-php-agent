/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */


/*
 * This file provides utility functions for handling PDO and PDOStatement
 * objects.
 */
#ifndef PHP_PDO_HDR
#define PHP_PDO_HDR

#include "nr_datastore.h"
#include "nr_datastore_instance.h"

/*
 * Purpose : Executes the given PDO prepared statement.
 *
 * Params  : 1. The PDOStatement to execute.
 *           2. An optional parameters argument to provide to the execute
 *              method.
 *
 * Returns : NR_SUCCESS if the query was successful, NR_FAILURE otherwise.
 */
extern nr_status_t nr_php_pdo_execute_query(zval* stmt,
                                            zval* parameters TSRMLS_DC);

/*
 * Purpose : Prepares the given query string.
 *
 * Params  : 1. The PDO object.
 *           2. The query string to prepare.
 *
 * Returns : A PDOStatement object on success, or NULL otherwise.
 */
extern zval* nr_php_pdo_prepare_query(zval* dbh, const char* query TSRMLS_DC);

/*
 * Purpose : Rebinds all bound parameters from the source PDOStatement to the
 *           destination.
 *
 * Params  : 1. The source PDOStatement object.
 *           2. The destination PDOStatement object.
 */
extern void nr_php_pdo_rebind_parameters(zval* source,
                                         zval* destination TSRMLS_DC);

/*
 * Purpose : Returns the pdo_dbh_t struct that is contained in the object store
 *           for a PDO object.
 *
 * Params  : 1. The PDO object.
 *
 * Returns : A pointer to the pdo_dbh_t struct, or NULL if an error occurred.
 */
extern pdo_dbh_t* nr_php_pdo_get_database_object(zval* dbh TSRMLS_DC);

/*
 * Purpose : Returns the pdo_stmt_t struct that is contained in the object store
 *           for a PDOStatement object.
 *
 * Params  : 1. The PDOStatement object.
 *
 * Returns : A pointer to the pdo_stmt_t struct, or NULL if an error occurred.
 */
extern pdo_stmt_t* nr_php_pdo_get_statement_object(zval* stmt TSRMLS_DC);

/*
 * Purpose : Returns the PDO driver in use for the given PDO or PDOStatement
 *           object.
 *
 * Params  : 1. The PDO or PDOStatement object.
 *
 * Returns : The driver name.
 */
extern const char* nr_php_pdo_get_driver(zval* obj TSRMLS_DC);

/*
 * Purpose : Return the PDO driver in nr_datastore_t form.  This will return
 *           NR_DATASTORE_PDO if there is an error or the driver does not
 *           match one of the nr_datastore_t types.
 *
 * Params  : 1. The PDO or PDOStatement object.
 *
 * Returns : The nr_datastore_t struct, or NR_DATASTORE_PDO if an error
 *           occurred.
 */
extern nr_datastore_t nr_php_pdo_get_datastore(zval* obj TSRMLS_DC);

/*
 * Purpose : Return the datastore instance metadata for the given PDO object.
 *
 * Params  : 1. Either a PDO or PDOStatement object.
 *
 * Returns : The datastore instance metadata.
 */
extern nr_datastore_instance_t* nr_php_pdo_get_datastore_instance(
    zval* obj TSRMLS_DC);

/*
 * Purpose : Create a new SQL trace node for a PDO query.
 *
 * Params  : 1. The segment to end.
 *           2. The SQL string that was executed.
 *           3. The length of the SQL string.
 *           4. The PDOStatement object that was executed, if any.
 *           5. The parameters that were bound to the PDOStatement, if any.
 *           6. true to enable explain plans, false otherwise.
 */
extern void nr_php_pdo_end_segment_sql(nr_segment_t* segment,
                                       const char* sqlstr,
                                       size_t sqlstrlen,
                                       zval* stmt_obj,
                                       zval* parameters,
                                       bool try_explain TSRMLS_DC);

/*
 * Purpose : Duplicate a PDO connection.
 *
 * Params  : 1. The PDO object to duplicate.
 *
 * Returns : The new PDO object, or NULL if an error occurred.
 */
extern zval* nr_php_pdo_duplicate(zval* dbh TSRMLS_DC);

/*
 * Purpose : Save the options that were given when constructing a PDO object.
 *
 * Params  : 1. The PDO object.
 *           2. The options array.
 */
extern void nr_php_pdo_options_save(zval* dbh, zval* options TSRMLS_DC);

/*
 * Purpose : Wrap PHP's internal php_pdo_parse_data_source() function.
 *
 * Params  : 1. The DSN to parse.
 *           2. The length of the DSN.
 *           3. The structure containing the named parameters that should be
 *              parsed out of the DSN.
 *           4. The number of parameters.
 *
 * Returns : NR_SUCCESS if parsing succeeded, in which case parsed will have
 *           been updated with the parameter values. NR_FAILURE if parsing
 *           failed or the PDO function couldn't be referenced.
 */
extern nr_status_t nr_php_pdo_parse_data_source(
    const char* data_source,
    uint64_t data_source_len,
    struct pdo_data_src_parser* parsed,
    size_t nparams);

/*
 * Purpose : Free the contents of the named parameter structure modified by
 *           nr_php_pdo_parse_data_source().
 *
 * Params  : 1. The array to free.
 *           2. The number of elements.
 */
extern void nr_php_pdo_free_data_sources(struct pdo_data_src_parser* parsed,
                                         size_t nparams);

#endif
