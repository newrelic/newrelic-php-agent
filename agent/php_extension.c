/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_extension.h"
#include "util_logging.h"
#include "util_memory.h"
#include "php_hash.h"

/* Internal structures used to track extensions. */
typedef struct _nr_php_extension_t {
  const char* name;
  int type;
  int module_number;
  int (*request_shutdown_func)(SHUTDOWN_FUNC_ARGS);
} nr_php_extension_t;

struct _nr_php_extensions_t {
  nr_php_extension_t* extensions;
  int allocated;
  int count;
};

/*
 * Transaction trace naming constants.
 */
#define NR_EXTENSION_PREFIX "Custom/"
#define NR_EXTENSION_PREFIX_UNKNOWN NR_EXTENSION_PREFIX "unknown"
#define NR_EXTENSION_RSHUTDOWN_SUFFIX "/RSHUTDOWN"

/*
 * A minimal install of PHP 5.3 has 7 extensions. Let's use 8 extensions as
 * the chunk size for reallocation.
 */
#define NR_EXTENSION_CHUNK_SIZE 8

/*
 * Purpose : Find an extension based on its type and number.
 *
 * Params  : 1. The extensions structure.
 *           2. The module type.
 *           3. The module number.
 *
 * Returns : A pointer to the extension record, or NULL if the type and number
 *           don't exist.
 */
static nr_php_extension_t* nr_php_extension_find(
    nr_php_extensions_t* extensions,
    int type,
    int module_number) {
  int i;

  for (i = 0; i < extensions->count; i++) {
    nr_php_extension_t* extension = &extensions->extensions[i];

    if ((extension->type == type)
        && (extension->module_number == module_number)) {
      return extension;
    }
  }

  return NULL;
}

/*
 * Purpose : Return the next unused extension.
 *
 * Params  : 1. The extensions structure.
 *
 * Returns : A pointer to the blank extension.
 */
static nr_php_extension_t* nr_php_extension_next(
    nr_php_extensions_t* extensions) {
  int count = extensions->count;

  /* Check if we need to reallocate. */
  if (count >= extensions->allocated) {
    extensions->allocated += NR_EXTENSION_CHUNK_SIZE;
    extensions->extensions = (nr_php_extension_t*)nr_realloc(
        extensions->extensions,
        extensions->allocated * sizeof(nr_php_extension_t));
  }

  /* Increment extensions->count and return. */
  ++extensions->count;
  return extensions->extensions + count;
}

/*
 * Purpose : Create the name to be used for an extension trace node.
 *
 * Params  : 1. The extension name. This may be NULL (although shouldn't be in
 *              practice). If it's a real string, it must be null terminated.
 *           2. The null terminated suffix to append to the extension name
 *              indicating the action that was traced, such as RSHUTDOWN.
 *           3. The buffer to write the name to.
 *           4. The length of the buffer.
 */
static void nr_php_extension_trace_name(const char* ext_name,
                                        const char* suffix,
                                        char* buffer,
                                        size_t buffer_len) {
  snprintf(buffer, buffer_len, "%s/%s/%s", NR_EXTENSION_PREFIX,
           ext_name ? ext_name : NR_EXTENSION_PREFIX_UNKNOWN, suffix);
}

/*
 * Purpose : Ends a segment of an extension trace.
 *
 * Params  : 1. The active segment.
 *           4. The extension name, or NULL if it isn't defined.
 *           5. The suffix to append to the extension name indicating the
 *              action that was traced, such as RSHUTDOWN.
 */
static void nr_php_extension_segment_end(nr_segment_t** segment_ptr,
                                         const char* ext_name,
                                         const char* suffix) {
  char name[512];

  if (nrunlikely(NULL == segment_ptr)) {
    return;
  }

  nr_php_extension_trace_name(ext_name, suffix, name, sizeof(name));

  nr_segment_set_name(*segment_ptr, name);
  nr_segment_end(segment_ptr);
}

/*
 * Purpose : Wrap an extension request shutdown function and time the original.
 *
 * Params  : 1. The module type.
 *           2. The module number.
 *
 * Returns : SUCCESS if the function was executed successfully, or if the
 *           extension doesn't define a request shutdown function. FAILURE
 *           otherwise. Note that, although the API specified by the Zend
 *           Engine requires these return values, they aren't actually checked!
 */
static int nr_php_extension_shutdown_wrapper(SHUTDOWN_FUNC_ARGS) {
  nr_php_extension_t* extension
      = nr_php_extension_find(NRPRG(extensions), type, module_number);

  if (NULL != extension) {
    /*
     * We checked for a request_shutdown_func earlier, but let's be defensive
     * in case other people are also mucking around with the module registry.
     */
    if (NULL != extension->request_shutdown_func) {
      int retval;
      nr_segment_t* segment = NULL;
      nrtxn_t* txn = NRPRG(txn);

      segment = nr_segment_start(txn, NULL, NULL);
      retval = extension->request_shutdown_func(SHUTDOWN_FUNC_ARGS_PASSTHRU);

      /*
       * There's no threshold right now: even a 0ms RSHUTDOWN will get an
       * interesting node created. If this becomes an external feature,
       * consider adding a threshold.
       */
      nr_php_extension_segment_end(&segment, extension->name,
                                   NR_EXTENSION_RSHUTDOWN_SUFFIX);

      return retval;
    }

    nrl_warning(NRL_INSTRUMENT,
                "Extension RSHUTDOWN wrapper called for extension %s with no "
                "shutdown function",
                extension->name ? extension->name : "(no name)");

    /* Did nothing, successfully. */
    return SUCCESS;
  }

  nrl_warning(NRL_INSTRUMENT,
              "Extension RSHUTDOWN wrapper called for unknown extension");
  return FAILURE;
}

/*
 * Purpose : Instrument the given Zend extension.
 *
 * Params  : 1. The Zend extension (module, in ZE parlance) to be instrumented.
 *           2. The structure containing instrumented extensions.
 *
 * Returns : ZEND_HASH_ARRAY_KEEP, to indicate to zend_hash_apply that the
 *           module entry should be kept.
 */
static int nr_php_extension_instrument(zend_module_entry* entry,
                                       nr_php_extensions_t* extensions,
                                       zend_hash_key* key NRUNUSED TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

  if (NULL != entry) {
    /*
     * If it's already instrumented, we don't need to do anything. Beyond that,
     * if there's no shutdown function, there's no need to instrument it.
     */
    if ((NULL != entry->request_shutdown_func)
        && (NULL
            == nr_php_extension_find(extensions, entry->type,
                                     entry->module_number))) {
      nr_php_extension_t* extension = nr_php_extension_next(extensions);

      extension->name = entry->name;
      extension->type = entry->type;
      extension->module_number = entry->module_number;
      extension->request_shutdown_func = entry->request_shutdown_func;

      /* Replace the request shutdown function with our own wrapper. */
      entry->request_shutdown_func = nr_php_extension_shutdown_wrapper;
    }
  } else {
    nrl_error(NRL_INIT, "Attempted to instrument a NULL zend_module_entry");
  }

  return ZEND_HASH_APPLY_KEEP;
}

/*
 * Purpose : Remove instrumentation from the given Zend extension.
 *
 * Params  : 1. The Zend extension (module, in ZE parlance) to have its
 *              instrumentation removed.
 *           2. The structure containing instrumented extensions.
 *
 * Returns : ZEND_HASH_ARRAY_KEEP, to indicate to zend_hash_apply that the
 *           module entry should be kept.
 */
static int nr_php_extension_uninstrument(zend_module_entry* entry,
                                         nr_php_extensions_t* extensions,
                                         zend_hash_key* key NRUNUSED
                                             TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

  if (NULL != entry) {
    /*
     * There's no error logging if this test fails as it's not really an error:
     * it just means that the extension wasn't instrumented, most likely
     * because it had a NULL shutdown function when it was first registered.
     */
    if (nr_php_extension_shutdown_wrapper == entry->request_shutdown_func) {
      nr_php_extension_t* extension = nr_php_extension_find(
          extensions, entry->type, entry->module_number);

      if (NULL != extension) {
        /* Restore the shutdown function we have saved for this extension. */
        entry->request_shutdown_func = extension->request_shutdown_func;
      } else {
        nrl_error(NRL_SHUTDOWN,
                  "Extension %s is instrumented, but the original shutdown "
                  "function cannot be found",
                  entry->name ? entry->name : "(no name)");
      }
    }
  } else {
    nrl_error(NRL_SHUTDOWN,
              "Attempted to uninstrument a NULL zend_module_entry");
  }

  return ZEND_HASH_APPLY_KEEP;
}

nr_php_extensions_t* nr_php_extension_instrument_create(void) {
  nr_php_extensions_t* extensions
      = (nr_php_extensions_t*)nr_malloc(sizeof(nr_php_extensions_t));

  extensions->count = 0;
  extensions->allocated = NR_EXTENSION_CHUNK_SIZE;
  extensions->extensions = (nr_php_extension_t*)nr_calloc(
      extensions->allocated, sizeof(nr_php_extension_t));

  return extensions;
}

void nr_php_extension_instrument_rescan(
    nr_php_extensions_t* extensions TSRMLS_DC) {
  if (NULL != extensions) {
    /* Walk the module registry and instrument the interesting extensions. */
    nr_php_zend_hash_ptr_apply(&module_registry,
                               (nr_php_ptr_apply_t)nr_php_extension_instrument,
                               extensions TSRMLS_CC);
  } else {
    nrl_error(NRL_INIT, "Cannot scan with a NULL extensions structure");
  }
}

void nr_php_extension_instrument_destroy(nr_php_extensions_t** extensions_ptr) {
  nr_php_extensions_t* extensions = NULL;

  TSRMLS_FETCH();

  if ((NULL == extensions_ptr) || (NULL == *extensions_ptr)) {
    return;
  }

  extensions = *extensions_ptr;

  /* Restore the original shutdown functions. */
  nr_php_zend_hash_ptr_apply(&module_registry,
                             (nr_php_ptr_apply_t)nr_php_extension_uninstrument,
                             extensions TSRMLS_CC);

  nr_free(extensions->extensions);
  nr_realfree((void**)extensions_ptr);
}
