/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Real user monitoring (RUM), a functionality of New Relic Browser.
 */
#ifndef NR_RUM_HDR
#define NR_RUM_HDR

#include "nr_txn.h"

/*
 * Purpose : Determine if the current transaction should have autorum.
 *
 * Returns : 1 if the txn should get autorum, and 0 otherwise.
 */
extern int nr_rum_do_autorum(const nrtxn_t* txn);

/*
 * Purpose : Produce the RUM header for a transaction.
 *
 * Params  : 1. The transaction pointer.
 *           2. Whether or not to use start tags.
 *           3. Whether or not this is being inserted by auto-RUM.
 *
 * Returns : The string to insert, allocated. If the header has been produced
 *           before, returns NULL. That is, this function can only be called
 *           once per transaction.
 */
extern char* nr_rum_produce_header(nrtxn_t* txn, int tags, int autorum);

/*
 * Purpose : Produce the RUM footer for a transaction.
 *
 * Params  : 1. The transaction pointer.
 *           2. Whether or not to use tags.
 *           3. Whether or not this is being inserted by auto-RUM.
 *
 * Returns : The string to insert, allocated. If the header has been produced
 *           before, returns NULL. That is, this function can only be called
 *           once per transaction.
 */
extern char* nr_rum_produce_footer(nrtxn_t* txn, int tags, int autorum);

/*
 * Purpose : Scan html looking for a heuristically good place in <head> to put
 *           RUM header code.
 *
 * Params  : 1. The input string, which is putatively well-formed HTML ready to
 *              be output.
 *           2. The length of the input string.
 *
 * Returns : The place in input where the text of the <head> tag starts.
 *
 * Watch out: This code uses a simplistic pure lexical approach for scanning the
 * HTML. It does not work if the input/input_len is only a fragment of the
 * entire HTML being generated, as for example when output buffer(s) are
 * flushed. Further, it will easily get confused if the HTML contains strings
 * (as for example inside of <script> that has strings) and those strings
 * contain HTML.
 */
extern const char* nr_rum_scan_html_for_head(const char* input,
                                             const uint input_len);

/*
 * This is a control block for the nr_run_output_handler_worker.
 *
 * In this way, nr_run_output_handler_worker can by unit tested more easily.
 */
typedef struct _nr_rum_control_block nr_rum_control_block_t;

struct _nr_rum_control_block {
  /*
   * Bound to a malloc-like function.
   */
  char* (*malloc_worker)(int size);

  /*
   * Typically bound to nr_rum_produce_header.
   */
  char* (*produce_header)(nrtxn_t* txn, int tags, int autorum);

  /*
   * Typically bound to nr_rum_produce_footer.
   */
  char* (*produce_footer)(nrtxn_t* txn, int tags, int autorum);
};

/*
 * Purpose : inject rum header and footer into output, returning new
 *           handled_output.
 *
 * Params  : 1.  A control block.
 *           2.  The transaction being worked on.
 *           3.  The original output string, which is putatively well formed
 *               HTML.
 *           4.  The original output string length to consider.
 *           5.  A pointer to a string to hold the transformed (header and
 *               footer injected) output.
 *           6.  A pointer to an int to hold the length of the transformed
 *               output.
 *           7.  non-zero if the response has a content length set, in which
 *               case we can't add rum js.
 *           8.  The mimetype of the http request/respnonse.
 *           9.  True if the function is to debug the autorum work.
 */
extern void nr_rum_output_handler_worker(
    const nr_rum_control_block_t* control_block,
    nrtxn_t* txn,
    char* output,
    size_t output_len,
    char** handled_output,
    size_t* handled_output_len,
    int has_response_content_length,
    const char* mimetype,
    int debug_autorum);

#endif /* NR_RUM_HDR */
