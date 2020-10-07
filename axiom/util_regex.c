/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "util_logging.h"
#include "util_memory.h"
#include "util_regex.h"
#include "util_regex_private.h"

/*
 * Purpose : Helper function to translate NR_REGEX constants into their PCRE
 *           equivalents.
 *
 * Params  : 1. A pointer to the current PCRE options that are set. This
 *              bitfield will be updated if the NR_REGEX option is set.
 *           2. The NR_REGEX options that were set.
 *           3. The PCRE option to set if the NR_REGEX option is set.
 *           4. The NR_REGEX option to check.
 */
static inline void nr_regex_handle_option(int* pcre_option_set,
                                          int nr_option_set,
                                          int pcre_option,
                                          int nr_option) {
  if (nr_option_set & nr_option) {
    *pcre_option_set |= pcre_option;
  }
}

nr_regex_t* nr_regex_create(const char* pattern, int options, int do_study) {
  const char* err = NULL;
  int erroffset = 0;
  int pcre_options = 0;
  nr_regex_t* regex;

  if (NULL == pattern) {
    return NULL;
  }

  regex = (nr_regex_t*)nr_zalloc(sizeof(nr_regex_t));

  /*
   * Transform NR_REGEX_* options into PCRE ones.
   */
  nr_regex_handle_option(&pcre_options, options, PCRE_ANCHORED,
                         NR_REGEX_ANCHORED);
  nr_regex_handle_option(&pcre_options, options, PCRE_CASELESS,
                         NR_REGEX_CASELESS);
  nr_regex_handle_option(&pcre_options, options, PCRE_DOLLAR_ENDONLY,
                         NR_REGEX_DOLLAR_ENDONLY);
  nr_regex_handle_option(&pcre_options, options, PCRE_DOTALL, NR_REGEX_DOTALL);
  nr_regex_handle_option(&pcre_options, options, PCRE_MULTILINE,
                         NR_REGEX_MULTILINE);

  /*
   * Compile the regular expression.
   */
  regex->code = pcre_compile(pattern, pcre_options, &err, &erroffset, NULL);
  if (NULL == regex->code) {
    nrl_verbosedebug(NRL_MISC, "%s: regex compilation error %s at offset %d",
                     __func__, err, erroffset);

    nr_regex_destroy(&regex);
    return NULL;
  }

  /*
   * Study it, if asked.
   */
  if (do_study) {
    err = NULL;
    regex->extra = pcre_study(regex->code, 0, &err);
    if ((NULL == regex->extra) && (NULL != err)) {
      nrl_verbosedebug(NRL_MISC, "%s: regex study error %s", __func__, err);

      nr_regex_destroy(&regex);
      return NULL;
    }
  }

  regex->capture_count = nr_regex_capture_count(regex);
  if (-1 == regex->capture_count) {
    nr_regex_destroy(&regex);
    return NULL;
  }

  return regex;
}

void nr_regex_destroy(nr_regex_t** regex_ptr) {
  nr_regex_t* regex;

  if ((NULL == regex_ptr) || (NULL == *regex_ptr)) {
    return;
  }

  regex = *regex_ptr;

  pcre_free_study(regex->extra);
  pcre_free(regex->code);
  nr_memset((void*)regex, 0, sizeof(nr_regex_t));
  nr_realfree((void**)regex_ptr);
}

nr_status_t nr_regex_match(const nr_regex_t* regex,
                           const char* str,
                           int str_len) {
  int rc;

  if ((NULL == regex) || (NULL == str) || (str_len < 0)) {
    return NR_FAILURE;
  }

  rc = pcre_exec(regex->code, regex->extra, str, str_len, 0, 0, NULL, 0);
  if (rc >= 0) {
    return NR_SUCCESS;
  } else if (PCRE_ERROR_NOMATCH == rc) {
    return NR_FAILURE;
  }

  nrl_verbosedebug(
      NRL_MISC, "%s: pcre_exec returned %d; expected >=0 or PCRE_ERROR_NOMATCH",
      __func__, rc);
  return NR_FAILURE;
}

nr_regex_substrings_t* nr_regex_match_capture(const nr_regex_t* regex,
                                              const char* str,
                                              int str_len) {
  int rc;
  nr_regex_substrings_t* ss;

  if ((NULL == regex) || (NULL == str) || (str_len < 0)) {
    return NULL;
  }

  ss = nr_regex_substrings_create(regex->code, regex->capture_count);
  if (NULL == ss) {
    return NULL;
  }

  rc = pcre_exec(regex->code, regex->extra, str, str_len, 0, 0, ss->ovector,
                 ss->ovector_size);
  if (rc < 0) {
    if (PCRE_ERROR_NOMATCH != rc) {
      nrl_verbosedebug(
          NRL_MISC,
          "%s: pcre_exec returned %d; expected >0 or PCRE_ERROR_NOMATCH",
          __func__, rc);
    }

    nr_regex_substrings_destroy(&ss);
    return NULL;
  } else if (0 == rc) {
    nrl_verbosedebug(NRL_MISC,
                     "%s: pcre_exec returned 0 (too many matches); expected >0 "
                     "or PCRE_ERROR_NOMATCH",
                     __func__);

    /*
     * We want this case to blow up: although we know the regular expression
     * matched, the ovector values are useless.
     */

    nr_regex_substrings_destroy(&ss);
    return NULL;
  }

  ss->capture_count = rc - 1;
  ss->subject = nr_strndup(str, str_len);

  return ss;
}

int nr_regex_capture_count(const nr_regex_t* regex) {
  int count;
  int retval;

  if (NULL == regex) {
    return -1;
  }

  retval = pcre_fullinfo(regex->code, regex->extra, PCRE_INFO_CAPTURECOUNT,
                         (void*)&count);
  if (0 != retval) {
    nrl_verbosedebug(NRL_MISC, "%s: pcre_fullinfo returned %d; expected 0",
                     __func__, retval);
    return -1;
  }

  return count;
}

nr_regex_substrings_t* nr_regex_substrings_create(pcre* code, int count) {
  nr_regex_substrings_t* ss;

  if (count < 0 || NULL == code) {
    return NULL;
  }

  ss = (nr_regex_substrings_t*)nr_malloc(sizeof(nr_regex_substrings_t));
  ss->code = code;
  ss->subject = NULL;
  ss->capture_count = count;
  ss->ovector_size = (count + 1) * 3;
  ss->ovector = (int*)nr_calloc(ss->ovector_size, sizeof(int));

  return ss;
}

void nr_regex_substrings_destroy(nr_regex_substrings_t** ss_ptr) {
  nr_regex_substrings_t* ss;

  if ((NULL == ss_ptr) || (NULL == *ss_ptr)) {
    return;
  }

  ss = *ss_ptr;
  nr_free(ss->subject);
  nr_free(ss->ovector);
  nr_realfree((void**)ss_ptr);
}

int nr_regex_substrings_count(const nr_regex_substrings_t* ss) {
  if (NULL == ss) {
    return -1;
  }

  return ss->capture_count;
}

char* nr_regex_substrings_get(const nr_regex_substrings_t* ss, int index) {
  int offsets[2];

  if (NR_FAILURE == nr_regex_substrings_get_offsets(ss, index, offsets)) {
    return NULL;
  }

  /*
   * Although PCRE provides a pcre_copy_substring function, we'll use
   * nr_strndup so that we definitely use our own allocator and the returned
   * string is OK to nr_free.
   */
  return nr_strndup(ss->subject + offsets[0], offsets[1] - offsets[0]);
}

char* nr_regex_substrings_get_named(const nr_regex_substrings_t* ss,
                                    const char* name) {
  int offset;

  if (NULL == ss || NULL == name) {
    return NULL;
  }

  offset = pcre_get_stringnumber(ss->code, name);
  if (PCRE_ERROR_NOSUBSTRING == offset) {
    return NULL;
  }

  return nr_regex_substrings_get(ss, offset);
}

nr_status_t nr_regex_substrings_get_offsets(const nr_regex_substrings_t* ss,
                                            int index,
                                            int offsets[2]) {
  if ((NULL == ss) || (index < 0) || (index > ss->capture_count)
      || (NULL == offsets)) {
    return NR_FAILURE;
  }

  offsets[0] = ss->ovector[index * 2];
  offsets[1] = ss->ovector[index * 2 + 1];

  return NR_SUCCESS;
}

const char* nr_regex_pcre_version(void) {
  return pcre_version();
}

char* nr_regex_quote(const char* str, size_t str_len, size_t* quoted_len) {
  nrbuf_t* buf = nr_buffer_create(str_len, 0);
  char* quoted = NULL;
  int qlen = 0;

  if (NULL == str) {
    goto end;
  }

  nr_regex_add_quoted_to_buffer(buf, str, str_len);
  nr_buffer_add(buf, NR_PSTR("\0"));

  qlen = nr_buffer_len(buf);
  if (qlen < 0) {
    goto end;
  }

  quoted = nr_strndup(nr_buffer_cptr(buf), qlen);

  if (quoted_len) {
    *quoted_len = (size_t)(qlen - 1);
  }

end:
  nr_buffer_destroy(&buf);

  return quoted;
}

void nr_regex_add_quoted_to_buffer(nrbuf_t* buf,
                                   const char* str,
                                   size_t str_len) {
  size_t i;

  if ((NULL == buf) || (NULL == str)) {
    return;
  }

  /*
   * This is (loosely) adapted from PHP's preg_quote function.
   */
  for (i = 0; i < str_len; i++) {
    switch (str[i]) {
      case '\0':
        nr_buffer_add(buf, NR_PSTR("\\000"));
        break;

      case '.':
      case '\\':
      case '+':
      case '*':
      case '?':
      case '[':
      case '^':
      case ']':
      case '$':
      case '(':
      case ')':
      case '{':
      case '}':
      case '=':
      case '!':
      case '>':
      case '<':
      case '|':
      case ':':
      case '-':
        nr_buffer_add(buf, NR_PSTR("\\"));
        /* FALLTHROUGH */

      default:
        nr_buffer_add(buf, &str[i], 1);
        break;
    }
  }
}
