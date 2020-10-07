/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_base64.h"
#include "util_memory.h"
#include "util_obfuscate.h"
#include "util_strings.h"

char* nr_obfuscate(const char* str, const char* key, int keylen) {
  int slen;
  int klen;
  unsigned char* xored;
  int i;
  int encoded_length;
  char* encoded;

  if ((0 == str) || (0 == str[0]) || (0 == key) || (0 == key[0])
      || (keylen < 0)) {
    return 0;
  }

  if (0 == keylen) {
    klen = nr_strlen(key);
  } else {
    klen = keylen;
  }

  slen = nr_strlen(str);
  xored = (unsigned char*)nr_alloca(slen + 1);

  for (i = 0; i < slen; i++) {
    int key_index = i % klen;
    unsigned char c = (unsigned char)str[i] ^ (unsigned char)key[key_index];

    xored[i] = c;
  }

  xored[slen] = '\0';
  encoded = (char*)nr_b64_encode((const char*)xored, slen, &encoded_length);
  return encoded;
}

char* nr_deobfuscate(const char* str, const char* key, int keylen) {
  int i;
  int klen;
  int slen = 0;
  char* decoded = 0;

  if ((0 == str) || (0 == str[0]) || (0 == key) || (0 == key[0])
      || (keylen < 0)) {
    return 0;
  }

  decoded = nr_b64_decode(str, &slen);
  if ((0 == decoded) || (slen <= 0)) {
    nr_free(decoded);
    return 0;
  }

  if (0 == keylen) {
    klen = nr_strlen(key);
  } else {
    klen = keylen;
  }

  for (i = 0; i < slen; i++) {
    int key_index = i % klen;

    decoded[i] = (unsigned char)decoded[i] ^ (unsigned char)key[key_index];
  }

  return decoded;
}
