//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package ratelimit

import (
	"errors"
	"math/big"
	"sync"
	"time"
)

// A Bucket represents a simple token bucket based rate limiter. Token
// buckets act as a reservoir with a fixed capacity. In order to perform a
// rate limited operation, a token must be acquired from the bucket. If
// sufficient tokens are available, the operation preceeds immediately
// and those tokens are removed from the bucket. If sufficient tokens
// are not available the operation is blocked.
//
// See: https://en.wikipedia.org/wiki/Token_bucket
type Bucket struct {
	mu        sync.Mutex // protects the following fields
	avail     uint64     // available tokens
	timestamp time.Time  // the last time tokens were added

	// The remaining fields are not protected by the mutex. Their values
	// do not change during the lifetime of the Bucket.

	capacity uint64 // maximum capacity of the token bucket

	// The refill rate is expressed as a rational number.
	tokens  uint64
	quantum time.Duration
}

// NewBucket returns a new token bucket with average rate mean/d.Seconds()
// ops/sec and maximum rate max/d.Seconds() ops/sec.
//
// For constant throughput, set mean == max.
func NewBucket(mean, max uint64, d time.Duration) *Bucket {
	return &Bucket{
		avail:     mean, // start with one quantum's worth of tokens
		timestamp: time.Now(),
		capacity:  max,
		tokens:    mean,
		quantum:   d,
	}
}

// Take acquires a single token from the Bucket. If no tokens are
// available, Take blocks until the Bucket is refilled.
func (b *Bucket) Take() {
	var minWait time.Duration

	for {
		b.mu.Lock()

		if b.avail == 0 {
			minWait = b.refill()
		}

		if b.avail > 0 {
			b.avail--
			b.mu.Unlock()
			return
		}

		b.mu.Unlock()

		// Insufficient time has elapsed for one token to be added,
		// sleep until at least that much time has elapsed.
		time.Sleep(minWait)
	}
}

// refill adds more tokens to the Bucket and returns the time remaining
// until the next refill.
func (b *Bucket) refill() time.Duration {
	now := time.Now()
	elapsed := now.Sub(b.timestamp)

	if elapsed < b.quantum {
		// Insufficient time has elapsed to add more tokens. Return the time
		// remaining until tokens can be added so the caller can wait.
		return b.quantum - elapsed
	}

	n := b.tokens * uint64(elapsed/b.quantum)

	if n < b.capacity-b.avail {
		b.avail += n
	} else {
		b.avail = b.capacity
	}

	// Round the timestamp down to the nearest whole quantum, so that
	// unaccounted time will be included in the next refill.
	if remainder := elapsed % b.quantum; remainder > 0 {
		b.timestamp = now.Add(-remainder)
	} else {
		b.timestamp = now
	}

	return b.quantum
}

// rpmToRate converts requests per minute into an equivalent token bucket
// refill rate expressed as a rational number.
func rpmToRate(rpm int) (n uint64, d time.Duration) {
	// Estimate how many tokens to add and at what interval to approximate
	// the requested RPM target.
	rate := big.NewRat(int64(rpm), 60000 /* milliseconds */)
	n = rate.Num().Uint64()
	d = time.Duration(rate.Denom().Uint64()) * time.Millisecond
	return
}

// ConstantRPM returns a new token bucket targeting n requests per minute.
func ConstantRPM(n int) *Bucket {
	if n <= 0 {
		panic(errors.New("ratelimit: non-positive RPM given"))
	}

	tokens, quantum := rpmToRate(n)
	return NewBucket(tokens, tokens, quantum)
}
