/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Most of the work is done in the axiom library.
 * Here we set up the function pointers to php-specific workers.
 */

#include "php_agent.h"
#include "php_autorum.h"
#include "php_globals.h"
#include "php_header.h"
#include "php_output.h"
#include "nr_header.h"
#include "nr_rum.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

static char* nr_php_rum_malloc(int len) {
  nrl_verbosedebug(NRL_AUTORUM, "autorum: resizing buffer to %d bytes", len);
  return (char*)emalloc(len);
}

/*
 * Callback function for iterating the list of response headers that
 * prints the value of the Content-Type header, if present. This is so
 * we can identify cases where PHP and/or the agent are not correctly
 * parsing the Content-Type and turn them into unit tests.
 */
static void nr_php_rum_log_content_type(void* data TSRMLS_DC) {
  sapi_header_struct* hdr = (sapi_header_struct*)data;

  NR_UNUSED_TSRMLS;

  if (0 == nr_strnicmp(hdr->header, NR_PSTR("Content-Type:"))) {
    nrl_verbosedebug(NRL_AUTORUM, "autorum: " NRP_FMT,
                     NRSAFELEN(hdr->header_len), hdr->header);
  }
}

/*
 * This function must be of type php_output_handler_func_t.
 */
void nr_php_rum_output_handler(
    char* output,
    nr_output_buffer_string_len_t output_len,
    char** handled_output,
    nr_output_buffer_string_len_t* handled_output_len,
    int mode TSRMLS_DC) {
  nr_rum_control_block_t control_block;
  char* mimetype = NULL;
  size_t handled_output_len_temp = 0;
  int has_response_content_length = 0;
  int is_recording = 0;
  int debug_autorum = NR_PHP_PROCESS_GLOBALS(special_flags).debug_autorum;

  if (debug_autorum) {
    nrl_verbosedebug(NRL_AUTORUM, "autorum: output handler starting: mode=%d",
                     mode);
  }

  /*
   * PHP should set these to sensible values before calling the output handler,
   * but let's set them just to be safe.
   */
  if (handled_output) {
    *handled_output = 0;
  }
  if (handled_output_len) {
    *handled_output_len = 0;
  }

  if (0 == nr_php_output_has_content(mode)) {
    return;
  }

  is_recording = nr_php_recording(TSRMLS_C);
  if (0 == is_recording) {
    if (debug_autorum) {
      nrl_verbosedebug(NRL_AUTORUM, "autorum: exiting due to not recording");
    }
    return;
  }

  if (debug_autorum) {
    zend_llist_apply(nr_php_response_headers(TSRMLS_C),
                     nr_php_rum_log_content_type TSRMLS_CC);
  }

  nr_memset(&control_block, 0, sizeof(control_block));
  control_block.produce_header = nr_rum_produce_header;
  control_block.produce_footer = nr_rum_produce_footer;
  control_block.malloc_worker = nr_php_rum_malloc;

  has_response_content_length = nr_php_has_response_content_length(TSRMLS_C);
  mimetype = nr_php_get_response_content_type(TSRMLS_C);

  nr_rum_output_handler_worker(
      &control_block, NRPRG(txn), output, (size_t)output_len, handled_output,
      &handled_output_len_temp, has_response_content_length, mimetype,
      debug_autorum);

  if (handled_output_len) {
    *handled_output_len
        = (nr_output_buffer_string_len_t)handled_output_len_temp;
  }

  nr_free(mimetype);
}
