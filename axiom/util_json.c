/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdint.h>
#include <stdio.h>

#include "util_json.h"

int nr_json_escape(char* dest, const char* json) {
  char* ep;

  if (0 == json) {
    json = "";
  }

  ep = dest;
  if (0 == ep) {
    return 0;
  }

  *ep = '"';
  ep++;

  while (*json) {
    switch (*json) {
      case '"':
        *ep = '\\';
        ep++;
        *ep = '"';
        ep++;
        break;

      case '\n':
        *ep = '\\';
        ep++;
        *ep = 'n';
        ep++;
        break;

      case '\r':
        *ep = '\\';
        ep++;
        *ep = 'r';
        ep++;
        break;

      case '\f':
        *ep = '\\';
        ep++;
        *ep = 'f';
        ep++;
        break;

      case '\b':
        *ep = '\\';
        ep++;
        *ep = 'b';
        ep++;
        break;

      case '\t':
        *ep = '\\';
        ep++;
        *ep = 't';
        ep++;
        break;

      case '\\':
        *ep = '\\';
        ep++;
        *ep = '\\';
        ep++;
        break;

      case '/':
        *ep = '\\';
        ep++;
        *ep = '/';
        ep++;
        break;

      default: {
        unsigned const char* u_json = (unsigned const char*)json;
        /*
         * All leading bytes start with 0b11xxxxxx
         */
        if (0xc0 == (u_json[0] & 0xc0)) { /* & 0b1100000 == 0b110000 */
          /*
           * Putative start of UTF-8 string
           * See:
           *   http://en.wikipedia.org/wiki/UTF8
           *   http://en.wikipedia.org/wiki/Json#Data_portability_issues
           */
          int bits_in_code_point; /* total bits in the code point */
          int nbytes;             /* total length of the utf8 character */
          uint8_t lead_mask
              = 0x0; /* bits of payload in byte 0 of the utf8 character */
          uint32_t code_point = 0x0; /* the binary encoding of the code point */
          int fault = 0; /* set if there's a decode fault somewhere in the
                            subsidiary bytes */
          int i;

          if (0xc0 == (u_json[0] & 0xe0)) {
            bits_in_code_point = 11;
            nbytes = 2;
            lead_mask = 0x1f;
          } else if (0xe0 == (u_json[0] & 0xf0)) {
            bits_in_code_point = 16;
            nbytes = 3;
            lead_mask = 0xf;
          } else if (0xf0 == (u_json[0] & 0xf8)) {
            bits_in_code_point = 21;
            nbytes = 4;
            lead_mask = 0x7;
          } else if (0xf8 == (u_json[0] & 0xfc)) {
            /* NOTE: not fully implemented; no encoding possible with surrogate
             * pairs */
            bits_in_code_point = 26;
            nbytes = 5;
            lead_mask = 0x3;
          } else if (0xfc == (u_json[0] & 0xfe)) {
            /* NOTE: not fully implemented; no encoding possible with surrogate
             * pairs */
            bits_in_code_point = 31;
            nbytes = 6;
            lead_mask = 0x1;
          } else {
            goto fault;
          }

          /*
           * Start assembling the binary representation of the code_point,
           * checking that all of the continuation bytes match 0b10xxxxxx
           */
          code_point = u_json[0] & lead_mask;
          fault = 0;
          for (i = 1; i < nbytes; i++) {
            if (0x80 == (u_json[i] & 0xc0)) {
              code_point <<= 6;
              code_point |= (u_json[i] & 0x3f);
            } else {
              fault = 1;
              break;
            }
          }

          if (fault) {
            goto fault;
          } else {
            if (bits_in_code_point <= 16) {
              sprintf(ep, "\\u%04x", code_point & 0xffff);
              ep += (1 + 1 + 4); /* 1 byte for backslash, 1 for u, 4 for data */
            } else if (bits_in_code_point == 21) {
              /*
               * Build a surrogate pair
               * Example from wikipedia is U+1F602 is 1 1111 0110  0000 0010
               *   wikipedia has pair1 is \uD83D is 11011000 11010011
               *   wikipedia has pair2 is \uDE02 is 11011110 00000010
               * See
               * http://en.wikipedia.org/wiki/UTF-16#Code_points_U.2B10000_to_U.2B10FFFF
               */
              uint16_t surrogate_0;
              uint16_t surrogate_1;

              code_point -= 0x10000; /* leaves us a 20-bit number */
              surrogate_0 = 0xd800 + ((code_point >> 10) & ((1 << 10) - 1));
              surrogate_1 = 0xdc00 + ((code_point >> 0) & ((1 << 10) - 1));
              sprintf(ep, "\\u%04x\\u%04x", surrogate_0 & 0xffff,
                      surrogate_1 & 0xffff);
              ep += (1 + 1 + 4 + 1 + 1 + 4);
            } else {
              goto fault;
            }
          }
          json += nbytes - 1; /* we'll increment this again shortly */
          break;
        }

        if ((u_json[0] <= 0x1f) || (u_json[0] >= 0x7f)) {
          char tmp[4];

        fault:;
          /*
           * Behavior of the encoder when presented with illegal UTF-8 is
           * undefined. Here we handle unknown or mis-encoded characters as a 16
           * bit UTF-8 encoding, with the leading byte set to 0.
           */
          snprintf(tmp, sizeof(tmp), "%02x", (int)(u_json[0]));
          *ep = '\\';
          ep++;
          *ep = 'u';
          ep++;
          *ep = '0';
          ep++;
          *ep = '0';
          ep++;
          *ep = tmp[0];
          ep++;
          *ep = tmp[1];
          ep++;
        } else {
          *ep = *json;
          ep++;
        }
      } break;
    }
    json++;
  }

  *ep = '"';
  ep++;
  *ep = 0;

  return ep - dest;
}
