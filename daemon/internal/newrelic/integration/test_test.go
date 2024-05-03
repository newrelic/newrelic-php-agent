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

func TestMergeEnv(t *testing.T) {
	os_k := "OS_KEY"
	os_v := "os_value"
	test_k := "TEST_KEY"
	test_v := "test_value"
	os_env := map[string]string{
		os_k: os_v,
	}
	test_same_env := map[string]string{
		os_k: test_v,
	}
	test_diff_env := map[string]string{
		test_k: test_v,
	}
	tests := []struct {
		name string
		env  map[string]string
		want string
	}{
		{name: "TestNoEnv", env: nil, want: os_v},
		{name: "TestWithSameEnv", env: test_same_env, want: test_v},
		{name: "TestWithDiffEnv", env: test_diff_env, want: os_v},
	}
	for i, tt := range tests {
		test := makeTestWithEnv(tt.name, tt.env)
		test.mergeEnv(os_env)
		got := test.Env[os_k]
		if got != tt.want {
			t.Errorf("%d. %s - got: %s; want %s", i, test.Name, got, tt.want)
		}
	}
}
