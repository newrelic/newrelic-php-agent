//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
	"testing"
)

type CompressEncodeTestcase struct {
	decoded string
}

var testcases = [...]CompressEncodeTestcase{
	{decoded: "compress me"},
	{
		decoded: "zipzipzipzipzipzipzipzipzipzipzipzipzipzipzipzipzipzipzip" +
			"zipzipzipzipzipzipzipzipzipzipzipzipzipzipzipzipzipzipzip"},
}

func TestCompressEncode(t *testing.T) {
	for _, tc := range testcases {
		encoded, err := CompressEncode([]byte(tc.decoded))
		if nil != err {
			t.Fatal(err)
		}
		decoded, err := UncompressDecode(encoded)
		if nil != err {
			t.Fatal(err)
		}
		if string(decoded) != tc.decoded {
			t.Fatalf("expected=%s got=%s", tc.decoded, string(decoded))
		}
	}
}
