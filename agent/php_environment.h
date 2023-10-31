/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains a function that describes the environment.
 */
#ifndef PHP_ENVIRONMENT_HDR
#define PHP_ENVIRONMENT_HDR

#define NR_METADATA_KEY_PREFIX "NEW_RELIC_METADATA_"
#define NR_LABELS_PLURAL_KEY "NEW_RELIC_LABELS"
#define NR_LABELS_SINGULAR_KEY_PREFIX "NEW_RELIC_LABEL_"

/*
 * Purpose : Produce the object that describes the invariant parts of the
 *           execution environment.
 *
 */
extern nrobj_t* nr_php_get_environment(TSRMLS_D);

/*
 * Purpose : Scan the given string looking for textual representations of
 *           key/value assignments.
 *
 *           The scanner looks for lines holding "hash rocket" style
 *           assignments:
 *
 *             key => value
 *
 *           The expected format delimits lines by newline characters, and
 *           expects single space characters before and after the literal '=>'.
 *           Any other spaces (before or after the key and/or value) will be
 *           included in the key or value as appropriate.
 *
 *           This format is generally seen with plain text phpinfo() output.
 *
 * Params  : 1. The string to scan.
 *           2. The length of the string to scan.
 *           3. The object that will have the key/value pairs added to it.
 *
 * Warning : The input string will be modified in place: key and value strings
 *           will have their trailing space or newline replaced with null
 *           bytes.
 */
void nr_php_parse_rocket_assignment_list(char* s, size_t len, nrobj_t* kv_hash);

/*
 * Purpose : Compare the given prefix to a key in a key value pair.  If matched,
 *           add the key value pair to the given hash.
 *
 *           The scanner looks for lines holding "=" style
 *           assignments:
 *
 *             key = value
 *
 *           This format is generally seen with system environment variable
 * output.
 *
 * Params  : 1. The prefix to scan for.
 *           2. The key to compare to the prefix.
 *           3. The value associated with the prefix.
 *           4. The object that will have the key/value pair added to it.
 *
 */
void nr_php_process_environment_variable_to_nrobj(const char* prefix,
                                                  const char* key,
                                                  const char* value,
                                                  nrobj_t* kv_hash);

/*
 * Purpose : Compare the given prefix to a key in a key value pair.  If matched,
 *           add the key value pair to the given hash.
 *
 *           The scanner looks for lines holding "=" style
 *           assignments:
 *
 *             key = value
 *
 *           This format is generally seen with system environment variable
 * output.
 *
 * Params  : 1. The prefix to scan for.
 *           2. The key to compare to the prefix.
 *           3. The value associated with the prefix.
 *           4. The string that will have the key/value pair added to it.
 *           5. The delimiter used to separate the key and value in the string.
 *           6. The delimiter used to separate key/value pairs in the string.
 *
 * Returns : String with matching key/value appended.
 */
char* nr_php_process_environment_variable_to_string(const char* prefix,
                                                    const char* key,
                                                    const char* value,
                                                    char* kv_hash,
                                                    const char* kv_delimeter,
                                                    const char* delimeter);

/*
 * Purpose : Parse the /proc/self/mountinfo file for the Docker cgroup v2 ID.
 *           Assign the value (if found) to the docker_id global.
 *
 * Params  : 1. The filepath of the mountinfo file to parse
 */
void nr_php_get_v2_docker_id(const char* cgroup_fname);

#endif /* PHP_ENVIRONMENT_HDR */
