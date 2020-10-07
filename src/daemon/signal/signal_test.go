//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package signal

import (
	"testing"
	"time"
)

func TestListenForWorkerSignal(t *testing.T) {
	if err := IsolateProcessGroup(); err != nil {
		t.Fatal(err)
	}

	c := ListenForWorker(1 * time.Hour)

	if err := SendReady(); err != nil {
		t.Error(err)
	}

	if state := <-c; state != WorkerReady {
		t.Error(state)
	}
}

func TestListenForWorkerTimeout(t *testing.T) {
	c := ListenForWorker(1 * time.Nanosecond)
	if state := <-c; state != WorkerTimeout {
		t.Error(state)
	}
}
