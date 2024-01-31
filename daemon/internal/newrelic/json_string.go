//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

type JSONString []byte

func (js JSONString) MarshalJSON() ([]byte, error) {
	if nil == js {
		return []byte("null"), nil
	}
	return js, nil
}

func (js *JSONString) UnmarshalJSON(data []byte) error {
	if nil == data {
		*js = nil
	} else {
		cpy := make([]byte, len(data))
		copy(cpy, data)
		*js = cpy
	}
	return nil
}
