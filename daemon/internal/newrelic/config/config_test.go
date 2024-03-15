//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package config

import "testing"

func TestTags(t *testing.T) {
	var cfg struct {
		A string `config:"a"`
		B string `config:"-"`
	}

	in := "a=foo\nB=bar"

	if err := ParseString(in, &cfg); err != nil {
		t.Fatalf("ParseString(%q) = %q", in, err.Error())
	}
	if cfg.A != "foo" {
		t.Errorf("ParseString(%q) = %q, want %q", in, cfg.A, "foo")
	}
	if cfg.B != "" {
		t.Errorf("ParseString(%q) = %q, want %q", in, cfg.B, "")
	}
}

var keywordTests = []struct {
	want, in string
}{
	// raw
	{want: "sample text", in: "a.b.c=sample text"},
	{want: "sample text", in: "a.b.c =sample text"},
	{want: "sample text", in: "a.b.c= sample text"},
	{want: "sample text", in: "a.b.c = sample text"},
	{want: "", in: "a.b.c="},
	{want: "", in: "a.b.c = "},
	{want: "", in: "a.b.c = \n"},
	// single quoted
	{want: "sample text", in: "a.b.c='sample text'"},
	{want: "sample text", in: "a.b.c ='sample text'"},
	{want: "sample text", in: "a.b.c= 'sample text'"},
	// double quoted
	{want: "sample text", in: `a.b.c="sample text"`},
	{want: "sample text", in: `a.b.c ="sample text"`},
	{want: "sample text", in: `a.b.c= "sample text"`},
	// multi-line
	{want: "sample\ntext", in: "a.b.c = 'sample\ntext'"},
	{want: "sample\ntext", in: "a.b.c = \"sample\ntext\""},
	// escape sequences
	{want: "\b\t\n\v\f\r\\", in: `a.b.c = "\b\t\n\v\f\r\"`},
}

func TestKeywords(t *testing.T) {
	var cfg struct {
		S string `config:"a.b.c"`
	}

	for _, tc := range keywordTests {
		cfg.S = "wrong answer"

		if err := ParseString(tc.in, &cfg); err == nil && cfg.S != tc.want {
			t.Errorf("ParseString(%q) = %q, want %q", tc.in, cfg.S, tc.want)
		} else if err != nil {
			t.Errorf("ParseString(%q) = %q, want %q", tc.in, err.Error(), tc.want)
		}
	}

	// invalid keywords
	cfg.S = "wrong answer"
	if err := ParseString("a.b+c = sample text", &cfg); err == nil {
		t.Errorf("expected syntax error, got %q", cfg.S)
	}

	cfg.S = "wrong answer"
	if err := ParseString("+a.b.c = sample text", &cfg); err == nil {
		t.Errorf("expected syntax error, got %q", cfg.S)
	}

	// unterminated strings
	cfg.S = "wrong answer"
	if err := ParseString("a.b.c = 'sample text", &cfg); err == nil {
		t.Errorf("expected syntax error, got %q", cfg.S)
	}

	cfg.S = "wrong answer"
	if err := ParseString(`a.b.c = "sample text`, &cfg); err == nil {
		t.Errorf("expected syntax error, got %q", cfg.S)
	}
}

func TestComments(t *testing.T) {
	var tests = []struct {
		want, input string
	}{
		// hash style comments
		{"non-comment text", "\n# one\nS = non-comment text# two\n# three"},
		// semicolon style comments
		{"non-comment text", "\n; one\nS = non-comment text; two\n; three"},
		// mixed and embedded comments
		{"non-comment text", "; one\nS = non-comment text; two # three"},
		{"non-comment text", "# one\nS = non-comment text# two ; three"},
	}

	for i := range tests {
		var cfg struct{ S string }

		in := tests[i].input
		want := tests[i].want
		err := ParseString(in, &cfg)

		if err == nil && cfg.S != want {
			t.Errorf("ParseString(%q) = %q, want %q", in, cfg.S, want)
		} else if err != nil {
			t.Errorf("ParseString(%q) = %q, want %q", in, err.Error(), want)
		}
	}
}

func TestMissingDelimiter(t *testing.T) {
	var cfg struct{ S string }

	in := "S  "
	if err := ParseString(in, &cfg); err == nil {
		t.Errorf("ParseString(%q) = nil, want error", in)
	}
}

func TestInvalidDelimiter(t *testing.T) {
	var cfg struct{ S string }

	in := "S :   "
	if err := ParseString(in, &cfg); err == nil {
		t.Errorf("ParseString(%q) = nil, want error", in)
	}
}
