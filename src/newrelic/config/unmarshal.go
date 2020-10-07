//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package config

import (
	"bytes"
	"encoding"
	"fmt"
	"reflect"
	"strconv"
)

func unmarshalValue(dest reflect.Value, keyword string, value []byte) error {
	if tu := asTextUnmarshaler(dest); tu != nil {
		return tu.UnmarshalText(value)
	}

	if len(value) == 0 {
		dest.Set(reflect.Zero(dest.Type()))
		return nil
	}

	switch dest.Kind() {
	case reflect.Bool:
		return unmarshalBool(dest, keyword, value)
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		return unmarshalInt(dest, keyword, value)
	case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64, reflect.Uintptr:
		return unmarshalUint(dest, keyword, value)
	case reflect.Float32, reflect.Float64:
		return unmarshalFloat(dest, keyword, value)
	case reflect.String:
		dest.SetString(string(value))
	default:
		return cannotConvertToType(value, dest.Type())
	}

	return nil
}

// asTextUnmarshaler checks whether v implements the TextUnmarshaler interface.
func asTextUnmarshaler(v reflect.Value) encoding.TextUnmarshaler {
	// Get a pointer to v so that methods with pointer receivers will be included below.
	if v.Kind() != reflect.Ptr && v.Type().Name() != "" && v.CanAddr() {
		v = v.Addr()
	}

	if v.Type().NumMethod() > 0 {
		if u, ok := v.Interface().(encoding.TextUnmarshaler); ok {
			return u
		}
	}
	return nil
}

func unmarshalBool(dest reflect.Value, keyword string, value []byte) error {
	var x bool
	var err error

	switch s := string(bytes.ToLower(value)); s {
	case "y", "yes", "on":
		x = true
	case "n", "no", "off":
		x = false
	default:
		x, err = strconv.ParseBool(s)
	}

	if err != nil {
		return fmt.Errorf("config: %s expects a boolean value", keyword)
	}

	dest.SetBool(x)
	return nil
}

func unmarshalInt(dest reflect.Value, keyword string, value []byte) error {
	x, err := strconv.ParseInt(string(value), 0, 64)

	if err != nil {
		return fmt.Errorf("config: %s expects an integer value", keyword)
	}

	if dest.OverflowInt(x) {
		return cannotConvertToType(value, dest.Type())
	}

	dest.SetInt(x)
	return nil
}

func unmarshalUint(dest reflect.Value, keyword string, value []byte) error {
	x, err := strconv.ParseUint(string(value), 0, 64)

	if err != nil {
		return fmt.Errorf("config: %s expects a non-negative integer value", keyword)
	}

	if dest.OverflowUint(x) {
		return cannotConvertToType(value, dest.Type())
	}

	dest.SetUint(x)
	return nil
}

func unmarshalFloat(dest reflect.Value, keyword string, value []byte) error {
	x, err := strconv.ParseFloat(string(value), dest.Type().Bits())

	if err != nil {
		return fmt.Errorf("config: %s expects a numeric value", keyword)
	}

	if dest.OverflowFloat(x) {
		return cannotConvertToType(value, dest.Type())
	}

	dest.SetFloat(x)
	return nil
}

func cannotConvertToType(value []byte, t reflect.Type) error {
	return fmt.Errorf("config: cannot convert %.50q to type %v", value, t)
}
