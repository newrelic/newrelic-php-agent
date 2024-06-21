/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CSEC_METADATA_H
#define CSEC_METADATA_H

typedef struct _nr_php_csec_metadata_t {
  int high_security;  /* Indicates if high security been set locally for this
                         application */
  char* license;      /* License key provided */
  char* plicense;     /* Printable license (abbreviated for security) */
  char* host_name;    /* Local host name reported to the daemon */
  char* entity_name;  /* Entity name related to this application */
  char* entity_type;  /* Entity type */
  char* account_id;   /* Security : Added for getting account id */
  char* entity_guid;  /* Entity guid related to this application */
  char* agent_run_id; /* The collector's agent run ID; assigned from the
                         New Relic backend */
} nr_php_csec_metadata_t;

/*
 * Purpose : Return app meta data by populating nr_php_csec_metadata_t
 *           structure. The caller is responsible for freeing the memory
 *           allocated for the strings in the structure.
 *
 * Params  : Pointer to a nr_php_csec_metadata_t structure
 *
 * Returns : 0 for success
 *          -1 for invalid input
 *          -2 for invalid internal state
 */
extern int nr_php_csec_get_metadata(nr_php_csec_metadata_t*);
typedef int (*nr_php_csec_get_metadata_t)(nr_php_csec_metadata_t*);
#define NR_PHP_CSEC_GET_METADATA "nr_php_csec_get_metadata"
#endif
