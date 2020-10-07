/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Functions related to doing evil things that are specific to the Zend Engine.
 */
#ifndef PHP_VM_HDR
#define PHP_VM_HDR

/*
 * Purpose : Set up our user opcode handlers.
 *
 * Warning : This function should only ever be called from MINIT.
 */
extern void nr_php_set_opcode_handlers(void);

/*
 * Purpose : Remove our user opcode handlers.
 *
 * Warning : This function should only ever be called from MSHUTDOWN.
 */
extern void nr_php_remove_opcode_handlers(void);

#endif /* PHP_VM_HDR */
