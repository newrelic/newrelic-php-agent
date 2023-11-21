/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIB_MONOLOG_PRIVATE_HDR
#define LIB_MONOLOG_PRIVATE_HDR

/*
 * Purpose : ONLY for testing to verify that the appropriate behavior of
 *           the conversion of zvals to attribute via nro.
 *
 * Returns : Pointer to nr_object_t representation of zval or
 *           NULL if zval is not a supported type for conversion
 *           to an attribute
 */
extern nrobj_t* nr_monolog_context_data_zval_to_attribute_obj(
    const zval* z TSRMLS_DC);

/*
 * Purpose : ONLY for testing to verify that the appropriate behavior of
 *           the conversion of a Monolog context array to attributes.
 *
 * Returns : Caller takes ownership of attributes struct
 *
 */
extern nr_attributes_t* nr_monolog_convert_context_data_to_attributes(
    zval* context_data TSRMLS_DC);
#endif /* LIB_MONOLOG_PRIVATE_HDR */
