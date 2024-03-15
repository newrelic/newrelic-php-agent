//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package valgrind

import (
	"io"
	"reflect"
	"testing"
)

func TestParseEmpty(t *testing.T) {
	_, err := ParseXML([]byte("<valgrindoutput />"))
	if err != nil {
		t.Error(err)
	}
}

func TestParseErrorCounts(t *testing.T) {
	input := []byte(`
    <valgrindoutput>
      <errorcounts>
        <pair>
          <count>2</count>
          <unique>abc</unique>
        </pair>
        <pair>
          <count>3</count>
          <name>def</name>
        </pair>
      </errorcounts>
    </valgrindoutput>`)

	report, err := ParseXML(input)
	if err != nil {
		t.Error(err)
		return
	}

	want := map[string]int{"abc": 2, "def": 3}

	if !reflect.DeepEqual(report.ErrorCounts, want) {
		t.Errorf("got %#v, want %#v", report.ErrorCounts, want)
	}
}

func TestParseEmptyErrorCounts(t *testing.T) {
	input := []byte(`<valgrindoutput><errorcounts></errorcounts></valgrindoutput>`)

	report, err := ParseXML(input)
	if err != nil {
		t.Error(err)
	} else if len(report.ErrorCounts) != 0 {
		t.Errorf("got %#v, want map[string]int{}", report)
	}
}

// On Mac OS X Yosemite, valgrind seems to consistently output partial XML.
// When that happens, xml.Unmarshal returns an EOF error, which is misleading.
// Test that we handle this by converting EOF errors into a more useful
// description of the problem.
func TestParseIncomplete(t *testing.T) {
	input := []byte(`
    <valgrindoutput>
      <protocolversion>4</protocolversion>
      <protocoltool>memcheck</protocoltool>

      <pid>59215</pid>
      <ppid>59214</ppid>
      <tool>memcheck</tool>`)

	report, err := ParseXML(input)

	if report != nil {
		t.Errorf("want report = %v, got %#v", nil, report)
	}

	if err == nil || err == io.EOF {
		t.Errorf("want a useful error, got %v", err)
	}
}
