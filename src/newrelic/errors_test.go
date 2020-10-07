//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"testing"
	"time"
)

func TestBasicErrorUse(t *testing.T) {
	h := NewErrorHeap(10)

	h.AddError(1, []byte(`{"x":1}`))
	h.AddError(2, []byte(`{"x":2}`))

	id := AgentRunID(`12345`)
	json, err := h.Data(id, time.Now())
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != "[\"12345\",[{\"x\":1},{\"x\":2}]]" {
		t.Error(string(json))
	}
}

func TestEmptyErrors(t *testing.T) {
	h := NewErrorHeap(10)
	id := AgentRunID(`12345`)
	json, err := h.Data(id, time.Now())
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",[]]` {
		t.Error(string(json))
	}
}

func TestErrorReplacement(t *testing.T) {
	h := NewErrorHeap(3)

	h.AddError(6, []byte(`{"x":6}`))
	h.AddError(5, []byte(`{"x":5}`))
	h.AddError(7, []byte(`{"x":7}`))
	h.AddError(4, []byte(`{"x":4}`))
	h.AddError(1, []byte(`{"x":1}`))
	h.AddError(8, []byte(`{"x":8}`))

	id := AgentRunID(`12345`)
	json, err := h.Data(id, time.Now())
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != "[\"12345\",[{\"x\":6},{\"x\":7},{\"x\":8}]]" {
		t.Error(string(json))
	}
}

func TestErrorsAudit(t *testing.T) {
	h := NewErrorHeap(10)
	h.AddError(1, []byte(`{"x":1}`))
	id := AgentRunID(`12345`)
	audit, err := h.Audit(id, time.Now())
	if nil != err {
		t.Fatal(err)
	}
	if nil != audit {
		t.Fatal(audit)
	}
}

func TestErrorsDataCopy(t *testing.T) {
	h := NewErrorHeap(10)
	data := []byte(`{"x":1}`)
	h.AddError(1, data)
	data[1] = 'X'

	id := AgentRunID(`12345`)
	json, err := h.Data(id, time.Now())
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != "[\"12345\",[{\"x\":1}]]" {
		t.Error(string(json))
	}
}
