/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <math.h>
#include <stdio.h>

#include "nr_app_harvest.h"
#include "nr_app_harvest_private.h"
#include "util_logging.h"

/*
 * The build pipeline for PHP 8.2 uses a Debian image that is built on a
 * version of glibc that contains an newer glibc version symbol for `pow()`
 * which is incompatible with older distributions, causing RPM installs to fail.
 *
 * To pin the version of `pow` to a version compatible with all NR PHP Agent
 * supported OSes, we utilize `asm()` and `.symver` to instruct the linker to
 * select an older version- in this case, 2.17.
 */
#if defined(__GLIBC__)
__asm__(".symver pow,pow@GLIBC_2.17");
#endif

void nr_app_harvest_init(nr_app_harvest_t* ah,
                         nrtime_t connect_timestamp,
                         nrtime_t harvest_frequency,
                         uint16_t sampling_target) {
  nr_app_harvest_private_init(ah, connect_timestamp, harvest_frequency,
                              sampling_target, nr_get_time());
}

bool nr_app_harvest_should_sample(nr_app_harvest_t* ah, nr_random_t* rnd) {
  return nr_app_harvest_private_should_sample(ah, rnd, nr_get_time());
}

nrtime_t nr_app_harvest_calculate_next_harvest_time(const nr_app_harvest_t* ah,
                                                    nrtime_t now) {
  uint64_t cycles;

  /* If the current time is before the connect timestamp, we don't really have
   * a sensible answer. Let's just say it'll be the connect timestamp, log a
   * message saying this is a bit odd, and go with it. */
  if (now < ah->connect_timestamp) {
    nrl_info(NRL_DAEMON,
             "cannot calculate next harvest given a connect timestamp in the "
             "future; possible clock skew? now=" NR_TIME_FMT
             " connect_timestamp=" NR_TIME_FMT,
             now, ah->connect_timestamp);
    return ah->connect_timestamp;
  }

  /* Similarly, if the harvest frequency is zero, then something's gone fairly
   * awry. As above, we'll just return the connect timestamp to avoid a
   * floating point exception. */
  if (0 == ah->frequency) {
    nrl_info(NRL_DAEMON, "harvest frequency is unexpectedly zero");
    return ah->connect_timestamp;
  }

  /* Otherwise, we calculate how many harvest cycles have occurred since
   * connection, add one, and we can multiply and add our way to the next
   * timestamp. The edge case here is if the current time is _exactly_ when a
   * harvest would have occurred: in that case, we'll return the timestamp for
   * the next harvest cycle, which is fine for the purposes of estimating
   * sampling. */
  cycles = (now - ah->connect_timestamp) / ah->frequency;
  return ah->connect_timestamp + (ah->frequency * (cycles + 1));
}

uint64_t nr_app_harvest_calculate_threshold(uint64_t target,
                                            uint64_t sampled_true_count) {
  uint64_t threshold;

  if ((0 == sampled_true_count) || (0 == target)
      || (sampled_true_count < target)) {
    return 0;
  }

  /* The spec provides the following (ruby) expression for the exponential
   * back-off strategy:
   *
   *   sampled = rand(decided_count) <
   *               (target ** (target / sampled_true_count) - target ** 0.5)
   *   or
   *
   *   sampled = rand(decided_count) < threshold
   *
   *   This function evaluates the threshold portion of the expression.
   */

  threshold = pow((double)target, (double)target / (double)sampled_true_count)
              - pow((double)target, 0.5);
  return threshold;
}

bool nr_app_harvest_compare_harvest_to_now(nrtime_t connect_timestamp,
                                           nrtime_t frequency,
                                           nrtime_t now) {
  return (now < (connect_timestamp + frequency));
}

bool nr_app_harvest_is_first(nr_app_harvest_t* ah, nrtime_t now) {
  if (NULL == ah) {
    return false;
  }
  return nr_app_harvest_compare_harvest_to_now(ah->connect_timestamp,
                                               ah->frequency, now);
}

void nr_app_harvest_private_init(nr_app_harvest_t* ah,
                                 nrtime_t connect_timestamp,
                                 nrtime_t harvest_frequency,
                                 uint16_t sampling_target,
                                 nrtime_t now) {
  nrtime_t prev_connect_timestamp;
  nrtime_t prev_frequency;

  if (NULL == ah) {
    return;
  }

  prev_connect_timestamp = ah->connect_timestamp;
  prev_frequency = ah->frequency;

  ah->connect_timestamp = connect_timestamp;
  ah->frequency = harvest_frequency;
  ah->target_transactions_per_cycle = sampling_target;

  nrl_debug(NRL_AGENT,
            "Adaptive sampling configuration. Connect: " NR_TIME_FMT
            " us. Frequency: " NR_TIME_FMT " us. Target: %d.",
            connect_timestamp, harvest_frequency, sampling_target);

  /* If the connect timestamp and/or harvest frequency changed, then the
   * previous data we had is now invalid, and we should reset it. */
  if (ah->connect_timestamp != prev_connect_timestamp
      || ah->frequency != prev_frequency) {
    ah->next_harvest = nr_app_harvest_calculate_next_harvest_time(ah, now);
    ah->threshold = 0.0;
    ah->prev_transactions_seen = 0;
    ah->transactions_seen = 0;
    ah->transactions_sampled = 0;
  }
}

bool nr_app_harvest_private_should_sample(nr_app_harvest_t* ah,
                                          nr_random_t* rnd,
                                          nrtime_t now) {
  if ((NULL == ah) || (NULL == rnd)) {
    return false;
  }

  /* If the time is at or after the next harvest, we need to roll the
   * transaction counters into a new harvest. */
  if (now >= ah->next_harvest) {
    ah->threshold = nr_app_harvest_calculate_threshold(
        ah->target_transactions_per_cycle, ah->transactions_sampled);

    /* To correctly determine the number of transactions seen in the previous
     * harvest, we need to determine whether we are in the immediately
     * subsequent harvest or not.
     *
     * We might be in a situation in which transactions were sampled during
     * harvest i, none were harvested in i+1, and now we are at i+2:
     *
     *    |-- harvest i --|-- harvest i+1 --|-- harvest i+2 --|               */
    if (now >= ah->next_harvest + ah->frequency) {
      ah->prev_transactions_seen = 0;
    } else {
      ah->prev_transactions_seen = ah->transactions_seen;
    }

    ah->transactions_seen = 0;
    ah->transactions_sampled = 0;
    ah->next_harvest = nr_app_harvest_calculate_next_harvest_time(ah, now);
  }

  /* This function implies that we've seen a transaction, so let's record that.
   */
  ah->transactions_seen++;

  /* If this is the first harvest, then the spec requires
   * us to sample the first n transactions, where n is the target number.
   * Figure that out and we can return early. */
  if (nr_app_harvest_is_first(ah, now)) {
    if (ah->transactions_sampled < ah->target_transactions_per_cycle) {
      ah->transactions_sampled++;
      return true;
    }
    return false;
  }
  /* We're still here! If we've not yet sampled the target number, we
   * determine whether this transaction should be sampled based on how many
   * transactions were sampled in the previous harvest cycle. */
  if (ah->transactions_sampled < ah->target_transactions_per_cycle) {
    if (nr_random_range(rnd, ah->prev_transactions_seen)
        < ah->target_transactions_per_cycle) {
      ah->transactions_sampled++;
      return true;
    }
    return false;
  } else {
    /* If we've already sampled enough transactions to hit the target, then we
     * need to adjust the target to make it exponentially harder and harder to
     * sample a transaction.
     */
    ah->threshold = nr_app_harvest_calculate_threshold(
        ah->target_transactions_per_cycle, ah->transactions_sampled);

    if (nr_random_range(rnd, ah->transactions_seen) < ah->threshold) {
      ah->transactions_sampled++;
      return true;
    }
  }

  return false;
}
