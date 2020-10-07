/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FW_CODEIGNITER_HDR
#define FW_CODEIGNITER_HDR

/*
 * Purpose : Return the topmost user function op array on the PHP stack.
 *
 * Returns : The topmost op array, or NULL if there is no op array.
 *
 * Note    : This function is only exported for unit testing reasons.
 */
extern zend_op_array* nr_codeigniter_get_topmost_user_op_array(TSRMLS_D);

#endif /* FW_CODEIGNITER_HDR */
