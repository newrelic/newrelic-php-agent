/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIB_PREDIS_PRIVATE_HDR
#define LIB_PREDIS_PRIVATE_HDR

extern const zend_long nr_predis_default_database;
extern const char* nr_predis_default_host;
extern const char* nr_predis_default_path;
extern const zend_long nr_predis_default_port;

/*
 * Purpose : Return the operation name from a command object.
 *
 * Params  : 1. The command object.
 *
 * Returns : The lowercased operatiion name, which is owned by the caller, or
 *           NULL if the command was invalid or the operation could not be read.
 */
extern char* nr_predis_get_operation_name_from_object(
    zval* command_obj TSRMLS_DC);

/*
 * Purpose : Create a datastore instance from a set of zvals containing the
 *           various URL parts that are used to connect via Predis.
 *
 * Params  : 1. The scheme. This is ignored if the scheme is not a string or
 *              NULL; the only value that has meaning is "unix" (which implies
 *              that the caller is connecting via a UNIX socket).
 *           2. The host name. If this is NULL or not a string, the default
 *              value above is assumed. If the scheme is "unix", this is
 *              ignored.
 *           3. The TCP port. If this is NULL or not an integer, the default
 *              value above is assumed. If the scheme is "unix", this is
 *              ignored.
 *           4. The path. If this is NULL or not a string, the default
 *              value above is assumed. If the scheme is not "unix", this is
 *              ignored.
 *           5. The database number. If this is NULL or not a scalar, the
 *              default value above is assumed.
 *
 * Returns : A new datastore instance structure, which is owned by the caller,
 *           or NULL if an error occurred.
 */
extern nr_datastore_instance_t* nr_predis_create_datastore_instance_from_fields(
    zval* scheme,
    zval* host,
    zval* port,
    zval* path,
    zval* database);

/*
 * Purpose : Create a datastore instance from an array in the format
 *           Predis\Client::__construct() accepts.
 *
 * Params  : 1. The array.
 *
 * Returns : A new datastore instance structure, which is owned by the caller,
 *           or NULL if an error occurred.
 */
extern nr_datastore_instance_t* nr_predis_create_datastore_instance_from_array(
    zval* params);

/*
 * Purpose : Create a datastore instance from a
 *           Predis\Connection\ParametersInterface object.
 *
 * Params  : 1. The object.
 *
 * Returns : A new datastore instance structure, which is owned by the caller,
 *           or NULL if an error occurred.
 */
extern nr_datastore_instance_t*
nr_predis_create_datastore_instance_from_parameters_object(
    zval* params TSRMLS_DC);

/*
 * Purpose : Create a datastore instance from a string in the format
 *           Predis\Client::__construct() accepts (generally speaking, a valid
 *           URI).
 *
 * Params  : 1. The string.
 *
 * Returns : A new datastore instance structure, which is owned by the caller,
 *           or NULL if an error occurred.
 */
extern nr_datastore_instance_t* nr_predis_create_datastore_instance_from_string(
    zval* params TSRMLS_DC);

/*
 * Purpose : Create a datastore instance from either the parameter given to the
 *           Predis\Connection\ConnectionInterface constructor or returned by
 *           Predis\Connection\NodeConnectionInterface::getParameters().
 *
 * Params  : 1. The parameter.
 *
 * Returns : A new datastore instance structure, which is owned by the caller,
 *           or NULL if an error occurred.
 */
extern nr_datastore_instance_t*
nr_predis_create_datastore_instance_from_connection_params(
    zval* params TSRMLS_DC);

/*
 * Purpose : Create and save a new datastore instance for the given
 *           Predis\Connection\ConnectionInterface object.
 *
 * Params  : 1. The Predis\Connection\ConnectionInterface object.
 *           2. The connection parameter given when constructing the object.
 *
 * Returns : A new datastore instance, which is NOT owned by the caller, or NULL
 *           if an error occurred.
 */
extern nr_datastore_instance_t* nr_predis_save_datastore_instance(
    const zval* conn,
    zval* params TSRMLS_DC);

/*
 * Purpose : Retrieve a datastore instance for the given
 *           Predis\Connection\ConnectionInterface object.
 *
 * Params  : 1. The Predis\Connection\ConnectionInterface object.
 *
 * Returns : The datastore instance, which is NOT owned by the caller, or NULL
 *           if an error occurred or no datastore instance exists for that
 *           object.
 */
extern nr_datastore_instance_t* nr_predis_retrieve_datastore_instance(
    const zval* conn TSRMLS_DC);

/*
 * Purpose : Retrieve a parameter from a Predis\Connection\ParametersInterface
 *           object.
 *
 * Params  : 1. The parameters object.
 *           2. The parameter name.
 *
 * Returns : The parameter value, which is owned by the caller, or NULL if an
 *           error occurred.
 */
extern zval* nr_predis_get_parameter(zval* params, const char* name TSRMLS_DC);

/*
 * Purpose : Check if the given object implements
 *           Predis\Connection\AggregateConnectionInterface.
 *
 * Params  : 1. The object to check.
 *
 * Returns : Non-zero if the object implements the interface; zero otherwise.
 */
extern int nr_predis_is_aggregate_connection(const zval* obj TSRMLS_DC);

/*
 * Purpose : Check if the given object implements
 *           Predis\Command\CommandInterface.
 *
 * Params  : 1. The object to check.
 *
 * Returns : Non-zero if the object implements the interface; zero otherwise.
 */
extern int nr_predis_is_command(const zval* obj TSRMLS_DC);

/*
 * Purpose : Check if the given object implements
 *           Predis\Connection\ConnectionInterface.
 *
 * Params  : 1. The object to check.
 *
 * Returns : Non-zero if the object implements the interface; zero otherwise.
 */
extern int nr_predis_is_connection(const zval* obj TSRMLS_DC);

/*
 * Purpose : Check if the given object implements
 *           Predis\Connection\NodeConnectionInterface.
 *
 * Params  : 1. The object to check.
 *
 * Returns : Non-zero if the object implements the interface; zero otherwise.
 */
extern int nr_predis_is_node_connection(const zval* obj TSRMLS_DC);

/*
 * Purpose : Check if the given object implements
 *           Predis\Connection\ParametersInterface.
 *
 * Params  : 1. The object to check.
 *
 * Returns : Non-zero if the object implements the interface; zero otherwise.
 */
extern int nr_predis_is_parameters(const zval* obj TSRMLS_DC);

#endif /* LIB_PREDIS_PRIVATE_HDR */
