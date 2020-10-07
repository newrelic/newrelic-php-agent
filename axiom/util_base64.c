/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * COPYRIGHT AND PERMISSION NOTICE
 *
 * Copyright (c) 1996 - 2019, Daniel Stenberg, daniel@haxx.se, and many
 * contributors, see the THANKS file.
 *
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright
 * notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The terms are also available at http://curl.haxx.se/docs/copyright.html.
 */

#include "nr_axiom.h"

#include <stdio.h>
#include <stdlib.h>

#include "util_base64.h"
#include "util_memory.h"

static const char table64[]
    = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const char* nr_b64_get_table(void) {
  return table64;
}

char* nr_b64_encode(const char* data, int len, int* retlen) {
  unsigned char ibuf[3];
  unsigned char obuf[4];
  int i;
  char* output;
  char* outdata;
  const unsigned char* indata = (const unsigned char*)data;

  if ((len <= 0) || (0 == data)) {
    return 0;
  }

  output = (char*)nr_malloc(len * 4 / 3 + 4);
  if (0 == output) {
    return 0;
  }
  outdata = output;

  while (len > 0) {
    int isegments = 0;

    for (i = 0; i < 3; i++) {
      if (len > 0) {
        isegments++;
        ibuf[i] = (unsigned char)*indata;
        indata++;
        len--;
      } else {
        ibuf[i] = 0;
      }
    }

    obuf[0] = (unsigned char)((ibuf[0] & 0xfc) >> 2);
    obuf[1]
        = (unsigned char)(((ibuf[0] & 0x03) << 4) | ((ibuf[1] & 0xf0) >> 4));
    obuf[2]
        = (unsigned char)(((ibuf[1] & 0x0f) << 2) | ((ibuf[2] & 0xc0) >> 6));
    obuf[3] = (unsigned char)(ibuf[2] & 0x3f);

    switch (isegments) {
      case 1:
        snprintf(output, 5, "%c%c==", table64[obuf[0]], table64[obuf[1]]);
        break;

      case 2:
        snprintf(output, 5, "%c%c%c=", table64[obuf[0]], table64[obuf[1]],
                 table64[obuf[2]]);
        break;

      default:
        snprintf(output, 5, "%c%c%c%c", table64[obuf[0]], table64[obuf[1]],
                 table64[obuf[2]], table64[obuf[3]]);
        break;
    }
    output += 4;
  }

  *output = 0;
  i = (int)(output - outdata);

  if (0 != retlen) {
    *retlen = i;
  }

  outdata = (char*)nr_realloc(outdata, i + 1);
  return outdata;
}

static void decodeQuantum(unsigned char* dest, const char* src) {
  const char* s;
  const char* p;
  unsigned long i;
  unsigned long v;
  unsigned long x = 0;

  for (i = 0, s = src; i < 4; i++, s++) {
    v = 0;
    p = table64;
    while (*p && (*p != *s)) {
      v++;
      p++;
    }
    if (*p == *s) {
      x = (x << 6) + v;
    } else if (*s == '=') {
      x = (x << 6);
    }
  }

  dest[2] = x & 0xff;
  x >>= 8;
  dest[1] = x & 0xff;
  x >>= 8;
  dest[0] = x & 0xff;
}

char* nr_b64_decode(const char* src, int* retlen) {
  int length = 0;
  int equalsTerm = 0;
  int i;
  int numQuantums;
  unsigned char lastQuantum[3];
  int rawlen = 0;
  unsigned char* newstr;
  unsigned char* ret;

  if (0 == src) {
    return 0;
  }

  for (i = 0; src[i]; i++) {
    if (0 == nr_b64_is_valid_character(src[i])) {
      return 0;
    }
  }

  while ((src[length] != '=') && src[length])
    length++;
  /* A maximum of two = padding characters is allowed */
  if (src[length] == '=') {
    equalsTerm++;
    if (src[length + equalsTerm] == '=') {
      equalsTerm++;
    }
  }
  numQuantums = (length + equalsTerm) / 4;

  /* Don't allocate a buffer if the decoded length is 0 */
  if (0 == numQuantums) {
    if (0 != retlen) {
      *retlen = 0;
    }
    return 0;
  }

  rawlen = (numQuantums * 3) - equalsTerm;

  /*
   * The buffer must be large enough to make room for the last quantum (which
   * may be partially thrown out) and the zero terminator.
   */
  newstr = (unsigned char*)nr_malloc(rawlen + 4);
  ret = newstr;

  /*
   * Decode all but the last quantum (which may not decode to a multiple of
   * 3 bytes)
   */
  for (i = 0; i < numQuantums - 1; i++) {
    decodeQuantum(newstr, src);
    newstr += 3;
    src += 4;
  }

  /*
   * This final decode may actually read slightly past the end of the buffer
   * if the input string is missing pad bytes. This will almost always be
   * harmless.
   */
  decodeQuantum(lastQuantum, src);
  for (i = 0; i < 3 - equalsTerm; i++) {
    newstr[i] = lastQuantum[i];
  }

  newstr[i] = '\0'; /* zero terminate */

  if (0 != retlen) {
    *retlen = rawlen;
  }

  return (char*)ret;
}
