//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package valgrind

import (
	"encoding/xml"
	"io"
	"strconv"
)

type outputElement struct {
	XMLName     xml.Name        `xml:"valgrindoutput"`
	Pid         int             `xml:"pid"`
	Ppid        int             `xml:"ppid"`
	Tool        string          `xml:"tool"`
	Errors      []*errorElement `xml:"error"`
	ErrorCounts []*pairElement  `xml:"errorcounts>pair"`
	SuppCounts  []*pairElement  `xml:"suppcounts>pair"`
}

type errorElement struct {
	Unique  string          `xml:"unique"`
	Kind    string          `xml:"kind"`
	What    string          `xml:"what"`
	Xwhat   string          `xml:"xwhat>text"`
	AuxWhat []string        `xml:"auxwhat"`
	Stacks  []*stackElement `xml:"stack"`
}

type stackElement struct {
	Frames []*frameElement `xml:"frame"`
}

type frameElement struct {
	IP       string `xml:"ip"`
	Object   string `xml:"obj"`
	Function string `xml:"fn"`
	Dir      string `xml:"dir"`
	File     string `xml:"file"`
	Line     int    `xml:"line"`
}

type pairElement struct {
	Name   string `xml:"name"`
	Unique string `xml:"unique"`
	Count  int    `xml:"count"`
}

// ParseXML parses XML-formatted Valgrind output.
func ParseXML(data []byte) (*Report, error) {
	var doc outputElement

	if len(data) == 0 {
		return nil, xml.UnmarshalError("valgrind: commentary is empty")
	}

	if err := xml.Unmarshal(data, &doc); err != nil {
		if err == io.EOF {
			// xml.Unmarshal returns EOF for an incomplete document.
			return nil, xml.UnmarshalError("valgrind: commentary is incomplete")
		}
		return nil, err
	}
	return newReportXML(&doc), nil
}

// newReportXML returns a new Report based on the contents of a
// XML-formatted valgrind report.
func newReportXML(output *outputElement) *Report {
	report := &Report{
		Pid:  output.Pid,
		Ppid: output.Ppid,
		Tool: output.Tool,
	}

	for _, node := range output.Errors {
		report.Errors = append(report.Errors, newErrorInfoXML(node))
	}

	if len(output.ErrorCounts) > 0 {
		report.ErrorCounts = make(map[string]int)

		for _, x := range output.ErrorCounts {
			// Name and Unique are mutually exclusive, so we expect a well-formed
			// document to provide exactly one of them for each error count.
			if x.Name != "" {
				report.ErrorCounts[x.Name] = x.Count
			} else {
				report.ErrorCounts[x.Unique] = x.Count
			}
		}
	}

	return report
}

// newErrorInfoXML returns a new ErrorInfo based on the contents of
// an error element in a valgrind XML document.
func newErrorInfoXML(elt *errorElement) *ErrorInfo {
	info := &ErrorInfo{
		Unique: elt.Unique,
		Kind:   elt.Kind,
		What:   elt.What,
	}

	// what and xwhat elements are mutually exclusive.
	if len(elt.Xwhat) > 0 {
		info.What = elt.Xwhat
	}

	if stacks := elt.Stacks; len(stacks) > 0 {
		// stacks[0] is the code path leading up to the error
		info.Stack = newStackTraceXML(stacks[0])

		// The remaining stacks provide additional information regarding the error.
		stacks := stacks[1:]

		for i, auxWhat := range elt.AuxWhat {
			auxInfo := &AuxInfo{What: auxWhat}

			// In well-formed output, we expect len(elt.AuxWhat) == len(stacks).
			if i < len(stacks) {
				auxInfo.Stack = newStackTraceXML(stacks[i])
			}

			info.Aux = append(info.Aux, auxInfo)
		}
	}

	return info
}

// newStackTraceXML returns a new slice of StackFrames based on the contents
// of a stack element in a valgrind XML document.
func newStackTraceXML(elt *stackElement) []*StackFrame {
	if elt == nil {
		return nil
	}

	frames := make([]*StackFrame, 0, len(elt.Frames))
	for _, eltFrame := range elt.Frames {
		frames = append(frames, newStackFrameXML(eltFrame))
	}
	return frames
}

// newStackFrameXML returns a new StackFrame based on the contents of a
// frame element in a valgrind XML document.
func newStackFrameXML(elt *frameElement) *StackFrame {
	var location string

	if elt.File != "" {
		location = elt.File + ":" + strconv.Itoa(elt.Line)
	}

	return &StackFrame{
		IP:       elt.IP,
		Object:   elt.Object,
		Function: elt.Function,
		Location: location,
	}
}
