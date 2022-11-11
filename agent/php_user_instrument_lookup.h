#if LOOKUP_METHOD == LOOKUP_USE_OP_ARRAY

/* This method stores wraprecs in a vector and uses reserved array in
 * zend_function's op_array to store the index into the vector that has
 * wraprec associated with zend_function. This method no longer works
 * when agent runs within php-fpm with opcache enabled (7.4+). */
#include "php_user_instrument_op_array.h"

#elif LOOKUP_METHOD == LOOKUP_USE_LINKED_LIST

/* This method stores wraprecs in a linked list and uses zend_function's
 * metadata (filename, lineno, scope and function name) to find a match
 * in linked list when looking for a wraprec associated with zend_function.
 * This method was created to address the issue with op_array method but
 * was found to have performance issues when list of wraprecs is long. */
#include "php_user_instrument_llist.h"

#elif LOOKUP_METHOD == LOOKUP_USE_UTIL_HASHMAP

/* This method stores wraprecs in a hashmap (axiom's implementation) and uses
 * zend_function's metadata (filename, lineno, scope and function name) to
 * create a string key that axiom's implementation of a hashmap uses to find
 * a match when looking for a wraprec associated with zend_function. Axiom's
 * hashmap implementation uses linked list to store values that have the same
 * hash. This method was created to address the performance issue with linked
 * list method however it was found to be slower than linked list method
 * because the operation of creating string key and then converting it to hash
 * is slower than walking a short list and comparing metadata. */
#include "php_user_instrument_util_hashmap.h"

#elif LOOKUP_METHOD == LOOKUP_USE_WRAPREC_HASHMAP

/* This method stores wraprecs in a hashmap (wraprec specialized implementation)
 * and uses zend_function's metadata (filename, lineno, scope and function name)
 * to create a numeric hash that wraprec specialized implementation of a hashmap
 * uses to find a bucket (a linked list) that is further scanned for exact match
 * of metadata when looking for a wraprec associated with zend_function.
 * wraprec specialized implementation uses linked list to store values that have
 * the same hash. This method was created to address the performance issue with
 * axiom's hashmap.  It uses optimized hash generation as well as optimized
 * metadata matcher. */
#include "php_user_instrument_wraprec_hash.h"

#else

#error "Unknown wraprec lookup method"

#endif