//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package config

import (
	"reflect"
	"testing"
	"time"
)

var boolTests = []struct {
	want bool
	in   string
}{
	// true synonyms
	{want: true, in: "true"},
	{want: true, in: "TRUE"},
	{want: true, in: "yes"},
	{want: true, in: "YES"},
	{want: true, in: "on"},
	{want: true, in: "ON"},
	{want: true, in: "1"},
	{want: true, in: "t"},
	{want: true, in: "T"},
	{want: true, in: "y"},
	{want: true, in: "Y"},
	// false synonyms
	{want: false, in: "false"},
	{want: false, in: "FALSE"},
	{want: false, in: "no"},
	{want: false, in: "NO"},
	{want: false, in: "off"},
	{want: false, in: "OFF"},
	{want: false, in: "0"},
	{want: false, in: "f"},
	{want: false, in: "F"},
	{want: false, in: "n"},
	{want: false, in: "N"},
	{want: false, in: ""},
}

func TestUnmarshalBool(t *testing.T) {
	var got bool

	dest := reflect.ValueOf(&got).Elem()

	// happy path
	for _, tc := range boolTests {
		got = !tc.want

		err := unmarshalValue(dest, "keyword", []byte(tc.in))
		if err == nil {
			if got != tc.want {
				t.Errorf("unmarshalValue(%q) = %t, want %t", tc.in, got, tc.want)
			}
		} else {
			t.Errorf("unmarshalValue(%q) = %q, want %t", tc.in, err.Error(), tc.want)
		}
	}

	// error path
	got = true
	if err := unmarshalValue(dest, "keyword", []byte("not a bool")); err == nil {
		t.Error("unmarshalling an invalid boolean should return an error")
	}
	if !got {
		t.Error("unmarshalling an invalid boolean should not change dest")
	}
}

func TestUnmarshalInt(t *testing.T) {
	var err error

	// int8
	x8 := int8(0)
	dest8 := reflect.ValueOf(&x8).Elem()
	err = unmarshalValue(dest8, "keyword", []byte("42"))
	if err == nil && x8 != 42 {
		t.Errorf("unmarshalValue(%q) = %d, want %d", "42", x8, 42)
	} else if err != nil {
		t.Errorf("unmarshalValue(%q) = %q, want %d", "42", err.Error(), 42)
	}

	// int16
	x16 := int16(0)
	dest16 := reflect.ValueOf(&x16).Elem()
	err = unmarshalValue(dest16, "keyword", []byte("42"))
	if err == nil && x16 != 42 {
		t.Errorf("unmarshalValue(%q) = %d, want %d", "42", x16, 42)
	} else if err != nil {
		t.Errorf("unmarshalValue(%q) = %q, want %d", "42", err.Error(), 42)
	}

	// int32
	x32 := int32(0)
	dest32 := reflect.ValueOf(&x32).Elem()
	err = unmarshalValue(dest32, "keyword", []byte("42"))
	if err == nil && x32 != 42 {
		t.Errorf("unmarshalValue(%q) = %d, want %d", "42", x32, 42)
	} else if err != nil {
		t.Errorf("unmarshalValue(%q) = %q, want %d", "42", err.Error(), 42)
	}

	// int64
	x64 := int64(0)
	dest64 := reflect.ValueOf(&x64).Elem()
	err = unmarshalValue(dest64, "keyword", []byte("42"))
	if err == nil && x64 != 42 {
		t.Errorf("unmarshalValue(%q) = %d, want %d", "42", x64, 42)
	} else if err != nil {
		t.Errorf("unmarshalValue(%q) = %q, want %d", "42", err.Error(), 42)
	}

	// overflow
	x := int8(42)
	dest := reflect.ValueOf(&x).Elem()
	err = unmarshalValue(dest, "keyword", []byte("128"))
	if err == nil {
		t.Error("should return error on overflow")
	}
	if x != 42 {
		t.Error("should not modify dest on overflow")
	}

	// not a number
	x32 = 42
	err = unmarshalValue(dest32, "keyword", []byte("not an int"))
	if err == nil {
		t.Error("should return error on invalid input")
	}
	if x32 != 42 {
		t.Error("should not modify dest on invalid input")
	}
}

func TestUnmarshalUint(t *testing.T) {
	var err error

	// uint8
	x8 := uint8(0)
	dest8 := reflect.ValueOf(&x8).Elem()
	err = unmarshalValue(dest8, "keyword", []byte("42"))
	if err == nil && x8 != 42 {
		t.Errorf("unmarshalValue(%q) = %d, want %d", "42", x8, 42)
	} else if err != nil {
		t.Errorf("unmarshalValue(%q) = %q, want %d", "42", err.Error(), 42)
	}

	// uint16
	x16 := uint16(0)
	dest16 := reflect.ValueOf(&x16).Elem()
	err = unmarshalValue(dest16, "keyword", []byte("42"))
	if err == nil && x16 != 42 {
		t.Errorf("unmarshalValue(%q) = %d, want %d", "42", x16, 42)
	} else if err != nil {
		t.Errorf("unmarshalValue(%q) = %q, want %d", "42", err.Error(), 42)
	}

	// uint32
	x32 := uint32(0)
	dest32 := reflect.ValueOf(&x32).Elem()
	err = unmarshalValue(dest32, "keyword", []byte("42"))
	if err == nil && x32 != 42 {
		t.Errorf("unmarshalValue(%q) = %d, want %d", "42", x32, 42)
	} else if err != nil {
		t.Errorf("unmarshalValue(%q) = %q, want %d", "42", err.Error(), 42)
	}

	// uint64
	x64 := uint64(0)
	dest64 := reflect.ValueOf(&x64).Elem()
	err = unmarshalValue(dest64, "keyword", []byte("42"))
	if err == nil && x64 != 42 {
		t.Errorf("unmarshalValue(%q) = %d, want %d", "42", x64, 42)
	} else if err != nil {
		t.Errorf("unmarshalValue(%q) = %q, want %d", "42", err.Error(), 42)
	}

	// overflow
	x8 = 42
	err = unmarshalValue(dest8, "keyword", []byte("256"))
	if err == nil {
		t.Error("should return error on overflow")
	}
	if x8 != 42 {
		t.Error("should not modify dest on overflow")
	}

	// not an unsigned integer
	x32 = 42
	err = unmarshalValue(dest32, "keyword", []byte("not a uint"))
	if err == nil {
		t.Error("should return error on invalid input")
	}
	if x32 != 42 {
		t.Error("should not modify dest on invalid input")
	}
}

func TestUnmarshalFloat(t *testing.T) {
	var err error

	// float32
	f32 := float32(0)
	dest32 := reflect.ValueOf(&f32).Elem()

	err = unmarshalValue(dest32, "keyword", []byte("42.0"))
	if err == nil && f32 != 42.0 {
		t.Errorf("unmarshalValue(%q) = %g, want %g", "42.0", f32, 42.0)
	} else if err != nil {
		t.Errorf("unmarshalValue(%q) = %q, want %g", "42.0", err.Error(), 42.0)
	}

	// float64
	f64 := float64(0)
	dest64 := reflect.ValueOf(&f64).Elem()

	err = unmarshalValue(dest64, "keyword", []byte("42.0"))
	if err == nil && f64 != 42.0 {
		t.Errorf("unmarshalValue(%q) = %g, want %g", "42.0", f64, 42.0)
	} else if err != nil {
		t.Errorf("unmarshalValue(%q) = %q, want %g", "42.0", err.Error(), 42.0)
	}

	// overflow
	f32 = float32(42)

	err = unmarshalValue(dest32, "keyword", []byte("3.5e+38"))
	if err == nil {
		t.Error("should return error on overflow")
	}
	if f32 != 42.0 {
		t.Errorf("should not modify dest on overflow")
	}

	// not a valid number
	f64 = 42.0

	err = unmarshalValue(dest64, "keyword", []byte("not a float"))
	if err == nil {
		t.Error("unmarshalling an invalid float should return an error")
	}
	if f64 != 42.0 {
		t.Error("unmarshalling an invalid float should not change dest")
	}
}

func TestUnmarshalTimeout(t *testing.T) {
	var x Timeout

	want := Timeout(60 * time.Second)
	input := want.String()

	err := x.UnmarshalText([]byte(input))
	if err == nil && x != want {
		t.Errorf("UnmarshalText(%q) = %v, want %v", input, x, want)
	} else if err != nil {
		t.Errorf("UnmarshalText(%q) = %q, want %v", input, err.Error(), want)
	}
}

func TestUnmarshalTimeoutNoUnits(t *testing.T) {
	var x Timeout

	want := Timeout(500 * time.Millisecond)
	input := "500"

	err := x.UnmarshalText([]byte(input))
	if err == nil && x != want {
		t.Errorf("UnmarshalText(%q) = %v, want %v", input, x, want)
	} else if err != nil {
		t.Errorf("UnmarshalText(%q) = %q, want %v", input, err.Error(), want)
	}
}

func TestTextUnmarshaler(t *testing.T) {
	var x time.Time

	want := time.Date(2006, time.January, 2, 15, 04, 56, 05, time.UTC)
	input := want.Format(time.RFC3339)

	dest := reflect.ValueOf(&x).Elem()
	err := unmarshalValue(dest, "keyword", []byte(input))

	if err == nil && x.Equal(want) {
		t.Errorf("unmarshalValue(%q) = %v, want %v", input, x, want)
	} else if err != nil {
		t.Errorf("unmarshalValue(%q) = %q, want %v", input, err.Error(), want)
	}
}
