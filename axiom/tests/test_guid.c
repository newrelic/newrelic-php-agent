/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_guid.h"
#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

static void test_create(void) {
  char* guid;
  nr_random_t* rnd = nr_random_create();

  nr_random_seed(rnd, 345345);

  guid = nr_guid_create(NULL);
  tlib_pass_if_str_equal("NULL random", guid, "0000000000000000");
  nr_free(guid);

  guid = nr_guid_create(rnd);
  tlib_pass_if_str_equal("guid creation", guid, "4559ffcf1824a8c6");
  nr_free(guid);

  guid = nr_guid_create(rnd);
  tlib_pass_if_str_equal("repeat guid creation", guid, "a53d06ce125d16df");
  nr_free(guid);

  nr_random_destroy(&rnd);
}

static void test_create_uses_all_hex_digits(void) {
  const char* hex_digits = "0123456789abcdef";
  const int guid_count = 1000;
  int counts[16] = {0};
  int i;
  int j;
  nr_random_t* rnd = nr_random_create();

  nr_random_seed(rnd, 345345);

  /*
   * The seed is fixed, so this test is deterministic. 1000 GUIDs give 16000
   * characters, about 1000 per digit if the digits are uniformly distributed,
   * so a digit that is missing entirely means it can never be generated.
   */
  for (i = 0; i < guid_count; i++) {
    char* guid = nr_guid_create(rnd);
    int bad = -1;

    tlib_pass_if_true("guid length", NR_GUID_SIZE == nr_strlen(guid),
                      "guid=%s length=%d", guid, nr_strlen(guid));
    for (j = 0; j < NR_GUID_SIZE; j++) {
      const char* pos = nr_strchr(hex_digits, guid[j]);

      if (NULL == pos || '\0' == guid[j]) {
        bad = j;
        break;
      }
      counts[pos - hex_digits]++;
    }
    tlib_pass_if_true("only lowercase hex digits", -1 == bad,
                      "guid=%s index=%d char=0x%02x", guid, bad,
                      -1 == bad ? 0u : (unsigned)(unsigned char)guid[bad]);
    nr_free(guid);
  }

  for (i = 0; i < 16; i++) {
    tlib_pass_if_true("every hex digit is generated", counts[i] > 0,
                      "digit=%c count=%d", hex_digits[i], counts[i]);
  }

  nr_random_destroy(&rnd);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_create();
  test_create_uses_all_hex_digits();
}
