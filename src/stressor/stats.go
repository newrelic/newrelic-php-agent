//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import (
	"runtime"
	"sort"
	"time"
)

type MemStats struct {
	NumAlloc uint64        // number of allocations
	SumAlloc uint64        // number of bytes allocated
	MaxAlloc uint64        // max bytes in use
	NumGC    uint64        // total GCs performed
	SumGC    time.Duration // total time spent in STW phase
	Pauses   *GCFreq       // GC pause frequency counts
}

// MaxGC returns the maximum recorded GC pause duration.
func (m *MemStats) MaxGC() time.Duration {
	if m.Pauses != nil {
		return m.Pauses.MaxDuration()
	}
	return 0
}

// GCFreq counts the frequency of GC pauses in one millisecond buckets.
type GCFreq struct {
	buckets map[time.Duration]uint32
	// End time of the last recorded GC. This is needed to ensure
	// GC pause times are not double counted.
	pauseEnd uint64
}

// MaxDuration returns the maximum recorded GC pause duration.
func (f *GCFreq) MaxDuration() time.Duration {
	var max time.Duration
	for d := range f.buckets {
		if d > max {
			max = d
		}
	}
	return max
}

func (f *GCFreq) Count() uint32 {
	var count uint32
	for _, n := range f.buckets {
		count += n
	}
	return count
}

type durationSlice []time.Duration

func (s durationSlice) Len() int           { return len(s) }
func (s durationSlice) Less(i, j int) bool { return s[i] < s[j] }
func (s durationSlice) Swap(i, j int)      { s[i], s[j] = s[j], s[i] }

func (f *GCFreq) Do(fn func(time.Duration, uint32)) {
	keys := make(durationSlice, 0, len(f.buckets))
	for bin := range f.buckets {
		keys = append(keys, bin)
	}
	sort.Sort(keys)

	for _, bin := range keys {
		fn(bin, f.buckets[bin])
	}
}

// Record records the pause durations from stats.
func (f *GCFreq) Record(stats *runtime.MemStats) {
	if f.buckets == nil {
		f.buckets = make(map[time.Duration]uint32)
	}

	for i := 0; i < 256; i++ {
		if stats.PauseEnd[i] > f.pauseEnd {
			pause := time.Duration(stats.PauseNs[i])
			f.buckets[pause-(pause%time.Millisecond)]++
		}
	}

	f.pauseEnd = stats.PauseEnd[(stats.NumGC+255)&255]
}

// Reset resets the frequency counters to zero.
func (f *GCFreq) Reset(stats *runtime.MemStats) {
	f.buckets = make(map[time.Duration]uint32)
	if stats != nil && stats.NumGC > 0 {
		f.pauseEnd = stats.PauseEnd[(stats.NumGC+255)&255]
	}
}
