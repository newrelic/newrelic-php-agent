/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_attributes.h"
#include "nr_rum.h"
#include "nr_rum_private.h"
#include "nr_txn.h"
#include "util_base64.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_obfuscate.h"
#include "util_object.h"
#include "util_regex.h"
#include "util_reply.h"
#include "util_strings.h"

int nr_rum_do_autorum(const nrtxn_t* txn) {
  if (0 == txn) {
    return 0;
  }
  if (txn->status.background) {
    /*
     * Note:  This background status can be changed at any time using the API:
     * use of nr_txn_set_as_background_job can prevent autorum from working.
     */
    return 0;
  }
  if (0 == txn->options.autorum_enabled) {
    return 0;
  }
  return 1;
}

static char* nr_rum_obfuscate(const char* input, const char* key) {
  return nr_obfuscate(input, key, NR_RUM_OBFUSCATION_KEY_LENGTH);
}

/*
 * Do not terminate these strings with a \n. If the fragments below end up
 * being inserted into the middle of a JavaScript string (for example the user
 * is doing something like document.write ('<title>' + somestring + '</title>')
 * then these strings are safe to insert if they do not contain a newline.
 * Otherwise they will end up breaking the string in the middle of a line which
 * will cause JavaScript errors, which will break a user's web site.
 */
static const char rum_start_tag[] = "<script type=\"text/javascript\">";
static const char rum_end_tag[] = "</script>";

/*
 * This array is needed by the axiom/tests. As such it is declared in
 * nr_rum_private.h and defined here.
 */
const char nr_rum_footer_prefix[] = "window.NREUM||(NREUM={});NREUM.info=";

char* nr_rum_produce_header(nrtxn_t* txn, int tags, int autorum) {
  char* return_header;
  size_t header_length;
  const char* loader;

  if (0 == txn) {
    return 0;
  }
  if (0 != txn->status.ignore) {
    return 0;
  }

  if (nrunlikely((0 != autorum) && (0 == txn->options.autorum_enabled))) {
    return 0;
  }

  if (0 != txn->status.rum_header) {
    nrl_debug(NRL_AUTORUM, "autorum: header empty due to previous %.32s call",
              (1 == txn->status.rum_header) ? "manual" : "auto-RUM");
    return 0;
  }

  loader = nro_get_hash_string(txn->app_connect_reply, "js_agent_loader", 0);
  if ((0 == loader) || (0 == loader[0])) {
    nrl_debug(NRL_AUTORUM, "autorum: header empty due to missing js loader");
    return 0;
  }

  txn->status.rum_header = 1 + (autorum != 0);

  header_length
      = (tags ? (nr_strlen(rum_start_tag) + nr_strlen(rum_end_tag)) : 0)
        + nr_strlen(loader) + 1;

  return_header = (char*)nr_malloc(header_length);
  return_header[0] = '\0';
  snprintf(return_header, header_length, "%s%s%s", tags ? rum_start_tag : "",
           loader, tags ? rum_end_tag : "");

  return return_header;
}

char* nr_rum_get_attributes(const nr_attributes_t* attributes) {
  char* json;
  nrobj_t* user;
  nrobj_t* agent;
  nrobj_t* hash;

  if (0 == attributes) {
    return 0;
  }

  user
      = nr_attributes_user_to_obj(attributes, NR_ATTRIBUTE_DESTINATION_BROWSER);
  agent = nr_attributes_agent_to_obj(attributes,
                                     NR_ATTRIBUTE_DESTINATION_BROWSER);

  if ((0 == agent) && (0 == user)) {
    return 0;
  }

  hash = nro_new_hash();
  if (user) {
    nro_set_hash(hash, "u", user);
  }
  if (agent) {
    nro_set_hash(hash, "a", agent);
  }

  nro_delete(user);
  nro_delete(agent);

  json = nro_to_json(hash);
  nro_delete(hash);

  return json;
}

static char* nr_rum_get_attributes_obfuscated(const nr_attributes_t* attributes,
                                              const char* rum_license) {
  char* json_obfuscated;
  char* json = nr_rum_get_attributes(attributes);

  if (0 == json) {
    return 0;
  }

  json_obfuscated = nr_rum_obfuscate(json, rum_license);
  nr_free(json);

  return json_obfuscated;
}

char* nr_rum_produce_footer(nrtxn_t* txn, int tags, int autorum) {
  char* txn_name;
  nrtime_t queue_time;
  nrtime_t app_time;
  nrobj_t* hash;
  char* hash_json;
  char* obfuscated_attributes;

  if (0 == txn) {
    return 0;
  }
  if (txn->status.ignore) {
    return 0;
  }
  if ((0 != autorum) && (0 == txn->options.autorum_enabled)) {
    return 0;
  }
  if (0 == txn->status.rum_header) {
    nrl_debug(NRL_AUTORUM, "autorum: footer empty due to no rum header");
    return 0;
  }
  if (txn->status.rum_footer) {
    nrl_debug(NRL_AUTORUM, "autorum: footer empty due to previous %.32s call",
              (1 == txn->status.rum_footer) ? "manual" : "auto-RUM");
    return 0;
  }

  /*
   * Finalize the web transaction name so we can put it in the footer.
   * Applying url rules may reveal that this txn should be ignored.
   */
  if (NR_FAILURE == nr_txn_freeze_name_update_apdex(txn)) {
    return 0;
  }

  app_time = nr_txn_unfinished_duration(txn);
  queue_time = nr_txn_queue_time(txn);
  txn_name = nr_rum_obfuscate(txn->name, txn->license);
  obfuscated_attributes
      = nr_rum_get_attributes_obfuscated(txn->attributes, txn->license);

  hash = nro_new_hash();
  nro_set_hash_string(hash, "beacon",
                      nro_get_hash_string(txn->app_connect_reply, "beacon", 0));
  nro_set_hash_string(
      hash, "licenseKey",
      nro_get_hash_string(txn->app_connect_reply, "browser_key", 0));
  nro_set_hash_string(
      hash, "applicationID",
      nro_get_hash_string(txn->app_connect_reply, "application_id", 0));
  nro_set_hash_string(hash, "transactionName", txn_name);
  nro_set_hash_long(hash, "queueTime", queue_time / NR_TIME_DIVISOR_MS);
  nro_set_hash_long(hash, "applicationTime", app_time / NR_TIME_DIVISOR_MS);
  nro_set_hash_string(hash, "atts", obfuscated_attributes);
  nro_set_hash_string(
      hash, "errorBeacon",
      nro_get_hash_string(txn->app_connect_reply, "error_beacon", 0));
  nro_set_hash_string(
      hash, "agent",
      nro_get_hash_string(txn->app_connect_reply, "js_agent_file", 0));

  nr_free(obfuscated_attributes);
  nr_free(txn_name);
  hash_json = nro_to_json(hash);
  nro_delete(hash);

  {
    char* footer;
    size_t footer_len;

    footer_len = (tags ? (sizeof(rum_start_tag) + sizeof(rum_end_tag)) : 0)
                 + sizeof(nr_rum_footer_prefix) + nr_strlen(hash_json) + 1;

    footer = (char*)nr_malloc(footer_len);
    footer[0] = '\0';
    snprintf(footer, footer_len, "%s%s%s%s", tags ? rum_start_tag : "",
             nr_rum_footer_prefix, hash_json, tags ? rum_end_tag : "");

    nr_free(hash_json);

    txn->status.rum_footer = 1 + (autorum != 0);
    return footer;
  }
}

static const char nr_rum_x_ua_compatible_regex[]
    = "<\\s*meta[^>]+http-equiv\\s*=\\s*['\"]x-ua-compatible['\"][^>]*>";
static const char nr_rum_charset_regex[] = "<\\s*meta[^>]+charset\\s*=[^>]*>";
/*
 * This head tag regex matches the whole tag (unlike the body tag regex) so
 * that we can easily insert after the head tag.
 */
static const char nr_rum_head_open_regex[] = "<head(\\s+[^>]*>|>)";
static const char nr_rum_body_open_regex[] = "<body[\\s>]";
const int nr_rum_regex_options = NR_REGEX_CASELESS | /* /i */
                                 NR_REGEX_MULTILINE; /* /m */

static void nr_rum_regex_search(const char* regex,
                                int regex_options,
                                const char* input,
                                const uint input_len,
                                const char** match_start,
                                const char** match_end) {
  nr_regex_t* re;
  nr_regex_substrings_t* ss;

  if (match_start) {
    *match_start = 0;
  }
  if (match_end) {
    *match_end = 0;
  }

  re = nr_regex_create(regex, regex_options, 0);
  if (NULL == re) {
    nrl_debug(NRL_AUTORUM,
              "autorum: unable to compile browser monitoring regex %.100s ",
              regex);
    return;
  }

  ss = nr_regex_match_capture(re, input, input_len);
  if (ss) {
    int offsets[2];

    if (NR_SUCCESS == nr_regex_substrings_get_offsets(ss, 0, offsets)) {
      if (match_start) {
        *match_start = input + offsets[0];
      }
      if (match_end) {
        *match_end = input + offsets[1];
      }
    }
  }

  nr_regex_substrings_destroy(&ss);
  nr_regex_destroy(&re);
}

const char* nr_rum_scan_html_for_head(const char* input, const uint input_len) {
  const char* x_ua_tag_start = 0;
  const char* x_ua_tag_end = 0;
  const char* charset_tag_start = 0;
  const char* charset_tag_end = 0;
  const char* match_start = 0;
  const char* match_end = 0;

  if (input_len < 6) {
    return 0;
  }

  /*
   * It is a little excessive to compile regexs for each scan, but timing
   * the compile reveals that it costs roughly ~30us the first time, and ~3us
   * thereafter, which is fast enough to prefer this approach over populating
   * globals.
   */
  nr_rum_regex_search(nr_rum_x_ua_compatible_regex, nr_rum_regex_options, input,
                      input_len, &x_ua_tag_start, &x_ua_tag_end);
  nr_rum_regex_search(nr_rum_charset_regex, nr_rum_regex_options, input,
                      input_len, &charset_tag_start, &charset_tag_end);

  if (x_ua_tag_end || charset_tag_end) {
    /* Insert after later match */
    return (x_ua_tag_end > charset_tag_end) ? x_ua_tag_end : charset_tag_end;
  }

  nr_rum_regex_search(nr_rum_head_open_regex, nr_rum_regex_options, input,
                      input_len, &match_start, &match_end);
  if (match_end) {
    return match_end;
  }

  nr_rum_regex_search(nr_rum_body_open_regex, nr_rum_regex_options, input,
                      input_len, &match_start, &match_end);
  if (match_start) {
    return match_start;
  }

  return 0;
}

const char* nr_rum_scan_html_for_foot(const char* input, const uint input_len) {
  int rv;

  if (0 == input) {
    return 0;
  }
  if (0 == input_len) {
    return 0;
  }

  rv = nr_strncaseidx_last_match(input, "</body>", input_len);
  if (-1 != rv) {
    return input + rv; /* before match */
  }

  return 0;
}

void nr_rum_output_handler_worker(const nr_rum_control_block_t* control_block,
                                  nrtxn_t* txn,
                                  char* output,
                                  size_t output_len,
                                  char** handled_output,
                                  size_t* handled_output_len,
                                  int has_response_content_length,
                                  const char* mimetype,
                                  int debug_autorum) {
  static const char autorum_text_html[] = "text/html";

  size_t final_len = output_len;
  const char* head = 0;
  const char* tail = 0;
  int done_head;
  int done_foot;
  char* rum_header = 0;
  char* rum_footer = 0;
  int header_len = 0;
  int footer_len = 0;
  int is_html = 0;

  if (0 == handled_output) {
    if (debug_autorum) {
      nrl_verbosedebug(NRL_AUTORUM,
                       "autorum: exiting due to no handled_output");
    }
    return;
  }
  *handled_output = NULL;

  if (0 == handled_output_len) {
    if (debug_autorum) {
      nrl_verbosedebug(NRL_AUTORUM,
                       "autorum: exiting due to no handled_output_len");
    }
    return;
  }
  *handled_output_len = 0;

  if (0 == control_block) {
    if (debug_autorum) {
      nrl_verbosedebug(NRL_AUTORUM, "autorum: exiting due to no control block");
    }
    return;
  }

  if (0 == txn) {
    if (debug_autorum) {
      nrl_verbosedebug(NRL_AUTORUM, "autorum: exiting due to no txn");
    }
    return;
  }

  if (0 == txn->options.autorum_enabled) {
    if (debug_autorum) {
      nrl_verbosedebug(NRL_AUTORUM,
                       "autorum: exiting due to txn->options.autorum_enabled");
    }
    return;
  }

  if (0 != txn->status.ignore) {
    if (debug_autorum) {
      nrl_verbosedebug(NRL_AUTORUM,
                       "autorum: exiting due to txn->status.ignore");
    }
    return;
  }

  if (0 != has_response_content_length) {
    if (debug_autorum) {
      nrl_verbosedebug(NRL_AUTORUM,
                       "autorum: exiting due to Content-Length header");
    }
    return;
  }

  if (0 == output_len) {
    return;
  }

  if (0 == mimetype) {
    return;
  }

  if (debug_autorum) {
    nrl_verbosedebug(NRL_AUTORUM, "autorum: mimetype=" NRP_FMT,
                     NRP_MIME(mimetype));
  }

  /*
   * Note that stopping at the length of the text_html string (rather than
   * a simple stricmp) allows us to properly inject into pages which have the
   * mimetype: "text/html; charset=utf-8"
   */
  if (0
      == nr_strnicmp(mimetype, autorum_text_html,
                     sizeof(autorum_text_html) - 1)) {
    is_html = 1;
  }

  if (0 == is_html) {
    if (debug_autorum) {
      nrl_verbosedebug(NRL_AUTORUM,
                       "autorum: ignoring non text/html (mimetype=" NRP_FMT
                       ") content",
                       NRP_MIME(mimetype));
    }
    return;
  }

  done_head = txn->status.rum_header;
  done_foot = txn->status.rum_footer;

  if (debug_autorum) {
    nrl_verbosedebug(NRL_AUTORUM, "autorum: done_head=%d done_foot=%d",
                     done_head, done_foot);
  }

  if (done_head && done_foot) {
    return;
  }

  if (output_len >= 6) {
    uint bytes_up_to_head = 0;
    uint bytes_after_head = 0;
    uint bytes_up_to_tail = 0;
    uint bytes_after_tail = 0;

    if (0 == done_head) {
      head = nr_rum_scan_html_for_head(output, output_len);

      if (debug_autorum) {
        nrl_verbosedebug(NRL_AUTORUM, "autorum: head=%p", head);
      }
      if (0 != head) {
        rum_header = (control_block->produce_header)(txn, 1, 1);
        if (debug_autorum) {
          nrl_verbosedebug(NRL_AUTORUM, "autorum: header=" NRP_FMT,
                           NRP_RUM(rum_header ? rum_header : "<NULL>"));
        }
        if (0 != rum_header) {
          header_len = nr_strlen(rum_header);
          bytes_up_to_head = (head - output);
          bytes_after_head = output_len - bytes_up_to_head;
          final_len += header_len;
        } else {
          head = 0;
        }
      }
    }

    if (((0 != done_head) || (0 != head)) && (0 == done_foot)) {
      tail = nr_rum_scan_html_for_foot(output, output_len);

      if (debug_autorum) {
        nrl_verbosedebug(NRL_AUTORUM, "autorum: tail=%p", tail);
      }

      if (0 != tail) {
        if (nrunlikely(tail < head)) {
          if (debug_autorum) {
            nrl_verbose(
                NRL_AUTORUM,
                "autorum: malformed HTML - </body> appears before <head>");
          }
          tail = 0;
        }
      }

      if (0 != tail) {
        rum_footer = (control_block->produce_footer)(txn, 1, 1);
        if (debug_autorum) {
          nrl_verbosedebug(NRL_AUTORUM, "autorum: footer=" NRP_FMT,
                           NRP_RUM(rum_footer ? rum_footer : "<NULL>"));
        }
        if (0 != rum_footer) {
          footer_len = nr_strlen(rum_footer);
          bytes_up_to_tail = tail - output;
          bytes_after_tail = (output_len - bytes_up_to_tail);
          final_len += footer_len;
        } else {
          tail = 0;
        }
      }
    }

    if (final_len != output_len) {
      char* final_out = (control_block->malloc_worker)(final_len + 1);

      *handled_output = final_out;
      *handled_output_len = final_len;

      /*
       * This does a series of memcpy's to insert the header and possibly the
       * footer in the right place. There are two ways we could have done this.
       * First by copying the whole buffer and then by inserting the strings
       * as appropriate, and this way. I chose this way, because it doesn't
       * need to deal with overlapping memory copies, which an insert would.
       * Overlapping memmove() is much slower than a few memcpy() calls.
       */
      if (debug_autorum) {
        nrl_verbosedebug(
            NRL_AUTORUM,
            "autorum: head=%p tail=%p bytes_up_to_head=%d header_len=%d "
            "bytes_after_head=%d bytes_up_to_tail=%d footer_len=%d "
            "bytes_after_tail=%d",
            head, tail, bytes_up_to_head, header_len, bytes_after_head,
            bytes_up_to_tail, footer_len, bytes_after_tail);
      }

      if (head) {
        nr_memcpy(final_out, output, bytes_up_to_head);
        final_out += bytes_up_to_head;
        nr_memcpy(final_out, rum_header, header_len);
        final_out += header_len;
        if (tail) {
          nr_memcpy(final_out, head, bytes_up_to_tail - bytes_up_to_head);
          final_out += (bytes_up_to_tail - bytes_up_to_head);
          nr_memcpy(final_out, rum_footer, footer_len);
          final_out += footer_len;
          nr_memcpy(final_out, tail, bytes_after_tail);
          final_out += bytes_after_tail;
        } else {
          nr_memcpy(final_out, head, bytes_after_head);
          final_out += bytes_after_head;
        }
      } else {
        nr_memcpy(final_out, output, bytes_up_to_tail);
        final_out += bytes_up_to_tail;
        nr_memcpy(final_out, rum_footer, footer_len);
        final_out += footer_len;
        nr_memcpy(final_out, tail, bytes_after_tail);
        final_out += bytes_after_tail;
      }
      *final_out = 0;
    }
    nr_free(rum_header);
    nr_free(rum_footer);
  } else {
    if (debug_autorum) {
      nrl_verbosedebug(NRL_AUTORUM, "autorum: short output_len=%zu from %s",
                       output_len, output);
    }
  }
}
