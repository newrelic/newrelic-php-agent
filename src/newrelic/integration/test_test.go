//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"bytes"
	"testing"

	"newrelic.com/daemon/newrelic/sysinfo"
)

func TestScrubHost(t *testing.T) {
	host, err := sysinfo.Hostname()
	if err != nil {
		t.Fatal(err)
	}

	tests := []struct {
		in   []byte
		want []byte
	}{
		{
			in:   []byte("Datastore/instance/Memcached/" + host + "/11211"),
			want: []byte("Datastore/instance/Memcached/__HOST__/11211"),
		},
		{
			in:   []byte(`"host":"` + host + `"`),
			want: []byte(`"host":"__HOST__"`),
		},
		{
			in:   []byte(`"host":"aaa` + host + `"`),
			want: []byte(`"host":"aaa` + host + `"`),
		},
		{
			in:   []byte(`"host":"` + host + `bbb"`),
			want: []byte(`"host":"` + host + `bbb"`),
		},
		{
			in:   []byte(`"host":"999` + host + `"`),
			want: []byte(`"host":"999` + host + `"`),
		},
		{
			in:   []byte(`"host":"` + host + `999"`),
			want: []byte(`"host":"` + host + `999"`),
		},
		{
			in:   []byte(`"host":"___` + host + `"`),
			want: []byte(`"host":"___` + host + `"`),
		},
		{
			in:   []byte(`"host":"` + host + `___"`),
			want: []byte(`"host":"` + host + `___"`),
		},
	}

	for i, tt := range tests {
		if got := ScrubHost(tt.in); !bytes.Equal(got, tt.want) {
			t.Errorf("%d. ScrubHost(%q) = %q; got %q", i, tt.in, tt.want, got)
		}
	}
}
