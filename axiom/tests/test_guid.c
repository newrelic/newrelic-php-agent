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

/*
 * Generate GUIDs until every hex digit has been seen, stopping at the first
 * invalid character or after max_guids GUIDs. Uniform digits are all seen
 * within a few GUIDs, so a digit still missing after max_guids can't be
 * generated. The seed is fixed, so the test is deterministic, and it always
 * makes the same two checks, whether they pass or fail.
 */
static void test_create_uses_all_hex_digits(void) {
  const char* hex_digits = "0123456789abcdef";
  const int max_guids = 1000;
  bool seen[16] = {false};
  int seen_count = 0;
  int guids = 0;
  bool found_bad = false;
  char bad_guid[NR_GUID_SIZE + 1] = {0};
  int bad_index = -1;
  unsigned bad_char = 0;
  char missing[16 + 1] = {0};
  int missing_count = 0;
  int i;
  nr_random_t* rnd = nr_random_create();

  nr_random_seed(rnd, 345345);

  while (16 > seen_count && max_guids > guids && !found_bad) {
    char* guid = nr_guid_create(rnd);

    guids++;
    for (i = 0; i < NR_GUID_SIZE; i++) {
      /* strchr matches the terminator, so a short GUID needs the '\0' test. */
      const char* pos = nr_strchr(hex_digits, guid[i]);

      if (NULL == pos || '\0' == guid[i]) {
        found_bad = true;
        nr_strlcpy(bad_guid, guid, sizeof(bad_guid));
        bad_index = i;
        bad_char = (unsigned char)guid[i];
        break;
      }
      if (!seen[pos - hex_digits]) {
        seen[pos - hex_digits] = true;
        seen_count++;
      }
    }
    nr_free(guid);
  }

  for (i = 0; i < 16; i++) {
    if (!seen[i]) {
      missing[missing_count++] = hex_digits[i];
    }
  }

  tlib_pass_if_true("guids contain only lowercase hex digits", !found_bad,
                    "guid=%s index=%d char=0x%02x", bad_guid, bad_index,
                    bad_char);
  tlib_pass_if_true("every hex digit is generated", 16 == seen_count,
                    "missing=%s after %d guids", missing, guids);

  nr_random_destroy(&rnd);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_create();
  test_create_uses_all_hex_digits();
}
