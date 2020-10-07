/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include "util_hash.h"
#include "util_hash_private.h"
#include "util_md5.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * txnName is hashed against a bit-rotated hash of the caller's path hash.
 *
 * The pseudocode for this hashing operations is as follows:
 *
 * TOHEXSTRING(
 *   XOR(
 *     ROL32(FROMHEXSTRING(referring_path_hash)),
 *     LOW32(MD5(CONCAT(primary_app_name, ';', txn_name)))
 *   )
 * )
 */
char* nr_hash_cat_path(const char* txn_name,
                       const char* primary_app_name,
                       const char* referring_path_hash) {
  char* app_txn = NULL;
  unsigned char md5[16];
  uint32_t md5_low;
  char* out = NULL;
  uint32_t refer = 0;
  uint32_t result;

  if ((NULL == txn_name) || (NULL == primary_app_name)) {
    return NULL;
  }

  /*
   * Firstly, we have to convert the referring path hash into an unsigned 32 bit
   * integer. If there's no referring path hash or it can't be converted, we use
   * the default value of 0.
   */
  if (NULL != referring_path_hash) {
    sscanf(referring_path_hash, "%" SCNx32, &refer);
  }

  /*
   * We then rotate the referring path hash.
   */
  refer = ((refer << 1) | (refer >> 31));

  /*
   * The next thing we have to do is concatenate the application name, a
   * semi-colon, and the transaction name, then take the MD5 hash of it, then
   * extract the lowest 32 bits of that.
   */
  app_txn = nr_formatf("%s;%s", primary_app_name, txn_name);
  if (NULL == app_txn) {
    goto end;
  }

  if (NR_FAILURE == nr_hash_md5(md5, app_txn, nr_strlen(app_txn))) {
    goto end;
  }
  md5_low = nr_hash_md5_low32(md5);

  /*
   * Finally, we xor the rotated referring path hash and the lowest 32 bits of
   * the MD5 hash derived from the application and transaction names, and then
   * output that to a hexadecimal string.
   */
  result = refer ^ md5_low;
  out = nr_formatf("%08x", result);

end:
  nr_free(app_txn);
  return out;
}

nr_status_t nr_hash_md5(unsigned char result[16], const char* input, int size) {
  nr_MD5_CTX ctx;

  if ((NULL == result) || (NULL == input) || (0 > size)) {
    return NR_FAILURE;
  }

  nr_MD5_Init(&ctx);
  nr_MD5_Update(&ctx, (const void*)input, (unsigned long)size);
  nr_MD5_Final(result, &ctx);

  return NR_SUCCESS;
}

uint32_t nr_hash_md5_low32(const unsigned char md5[16]) {
  uint32_t result;

  if (NULL == md5) {
    return 0;
  }

  result
      = ((md5[12] << 24) | (md5[13] << 16) | (md5[14] << 8) | (md5[15] << 0));

  return result;
}

uint32_t nr_mkhash(const char* str, int* len) {
  /*
   * This function currently implements MurmurHash3. This is loosely based on
   * the canonical C++ implementation, which was written by Austin Appleby and
   * is available under the MIT Licence. You can find it at
   * https://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp.
   */

  /*
   * These are constants defined by the MurmurHash3 algorithm.
   */
  static const uint32_t c1 = 0xcc9e2d51;
  static const uint32_t c2 = 0x1b873593;
  static const uint32_t r1 = 15;
  static const uint32_t r2 = 13;
  static const uint32_t m = 5;
  static const uint32_t n = 0xe6546b64;

  /*
   * A seed chosen entirely at random. Some MurmurHash3 implementations allow
   * the seed to be provided at runtime; we could theoretically generate a
   * random number on each run of the agent and daemon, but this seems
   * overkill.
   */
  static const uint32_t seed = 0x290848ab;
  uint32_t hash = seed;

  /*
   * Other variables used within the implementation.
   */
  const uint32_t* blocks;
  int i;
  uint32_t k1 = 0;
  int num_blocks;
  int str_len;
  const uint8_t* tail;

  /*
   * Check parameters and set *len if provided.
   */
  if ((NULL == str) || ('\0' == str[0])) {
    if (len) {
      *len = 0;
    }
    return 0;
  }

  if ((NULL == len) || (0 == *len)) {
    str_len = nr_strlen(str);
    if (len) {
      *len = str_len;
    }
  } else {
    str_len = *len;
  }

  /*
   * The bulk of the hash requires us to read the string data in four byte
   * chunks, so we'll cast the string to an array of uint32_t for easier
   * accessibility.
   */
  num_blocks = str_len / 4;
  blocks = (const uint32_t*)str;
  for (i = 0; i < num_blocks; i++) {
    uint32_t k = blocks[i];

    k *= c1;
    k = (k << r1) | (k >> (32 - r1));
    k *= c2;

    hash ^= k;
    hash = (hash << r2) | (hash >> (32 - r2));
    hash = hash * m + n;
  }

  /*
   * Handle remaining bytes. There is intentionally no default: case in the
   * switch statement, as the only other possible case is there being no
   * remaining bytes, in which case we do nothing.
   */
  tail = (const uint8_t*)(str + (num_blocks * 4));
  switch (str_len & 3) {
    case 3:
      k1 ^= tail[2] << 16;
      /* FALLTHROUGH */
    case 2:
      k1 ^= tail[1] << 8;
      /* FALLTHROUGH */
    case 1:
      k1 ^= tail[0];

      k1 *= c1;
      k1 = (k1 << r1) | (k1 >> (32 - r1));
      k1 *= c2;
      hash ^= k1;
  }

  /*
   * Final mixing with a couple more magic constants.
   */
  hash ^= str_len;
  hash ^= (hash >> 16);
  hash *= 0x85ebca6b;
  hash ^= (hash >> 13);
  hash *= 0xc2b2ae35;
  hash ^= (hash >> 16);

  return hash;
}
