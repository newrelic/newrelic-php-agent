//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"bytes"
	"maps"
	"testing"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/sysinfo"
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

func TestSubsEnvVar(t *testing.T) {
	k := "TEST_KEY"
	v := "test_value"
	test := NewTest("SubsEnvVar")
	test.Env[k] = v

	tests := []struct {
		in   []byte
		want []byte
	}{
		{
			in:   []byte("External/ENV[" + k + "]/all"),
			want: []byte("External/" + v + "/all"),
		},
		{
			in:   []byte("External/ENV[NON_EXISTING_KEY]/all"),
			want: []byte("External//all"),
		},
	}

	for i, tt := range tests {
		if got := test.subEnvVars(tt.in); !bytes.Equal(got, tt.want) {
			t.Errorf("%d. Test.subsEnvVar(%q) = %q; want %q", i, tt.in, got, tt.want)
		}
	}
}

func makeTestWithEnv(name string, e map[string]string) *Test {
	t := NewTest(name)
	if nil != e {
		maps.Copy(t.Env, e)
	}
	return t
}

func TestMerge(t *testing.T) {
	m1_k := "OS_KEY"
	m1_v := "os_value"
	m2_k := "TEST_KEY"
	m2_v := "test_value"
	m1 := map[string]string{
		m1_k: m1_v,
	}
	m2 := map[string]string{
		m2_k: m2_v,
	}
	same_key_diff_val := map[string]string{
		m1_k: m2_v,
	}
	tests := []struct {
		name string
		m    map[string]string
		k    string
		want string
	}{
		{name: "MergeEmptyMap", m: nil, k: m1_k, want: m1_v},
		{name: "MergeDiffMaps", m: m2, k: m1_k, want: m1_v},
		{name: "MergeMapWithSameKeys", m: same_key_diff_val, k: m1_k, want: m2_v},
	}
	for i, tt := range tests {
		mm := merge(m1, tt.m)
		got := mm[tt.k]
		if got != tt.want {
			t.Errorf("%d. %s - got: %s; want %s", i, tt.name, got, tt.want)
		}
	}
}
