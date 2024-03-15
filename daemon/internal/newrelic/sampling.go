//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

// The priority sampling algorithm allows for uniform sampling of transactions,
// while also sampling coherent collections of transaction events, transaction
// error events, error traces and so forth for Distributed Tracing support.

// The SamplingPriority type is used by the Daemon wherever it must make any
// decisions on what data to replace when pool limits are reached during a
// single harvest.
type SamplingPriority float64

func (x SamplingPriority) IsLowerPriority(y SamplingPriority) bool {
	return x < y
}
