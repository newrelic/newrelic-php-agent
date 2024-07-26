//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

// Package config implements configuration file parsing.
package config

import (
	"bufio"
	"bytes"
	"errors"
	"fmt"
	"io"
	"os"
	"reflect"
	"strings"
	"unicode"
)

// A Decoder represents a configuration parser reading a particular
// input stream. The parser assumes that its input is encoded in UTF-8.
type Decoder struct {
	r       *bufio.Reader // input stream
	fields  typeInfo      // maps keywords to their destination
	keyword string        // current keyword
	token   bytes.Buffer  // current token
}

// typeInfo maps keywords to their corresponding field. Fields are
// identified by their index sequence, which is stored in the same
// format expected by Type.FieldByIndex.
type typeInfo map[string][]int

// A stateFn is a transition function for configuration parsing.
type stateFn func(*Decoder, reflect.Value) (next stateFn, err error)

// ParseFile parses the configuration in the file specified by name and
// stores the result in the value pointed to by v.
func ParseFile(name string, v interface{}) error {
	f, err := os.Open(name)
	if err != nil {
		return err
	}
	defer f.Close()

	d := Decoder{r: bufio.NewReader(f)}
	return d.Decode(v)
}

// ParseString parses the configuration in s and stores the result in
// the value pointed to by v.
func ParseString(s string, v interface{}) error {
	d := Decoder{r: bufio.NewReader(strings.NewReader(s))}
	return d.Decode(v)
}

// NewDecoder creates a new configuration parser reading from r.
func NewDecoder(r io.Reader) *Decoder {
	return &Decoder{r: bufio.NewReader(r)}
}

// Decode parses the configuration and stores the result in the
// value pointed to by v.
//
// Decode implements the following PEG.
//
//	INI     = ((KEYWORD WS* '=' WS* VALUE) / COMMENT / WS)*
//	KEYWORD = ALPHA (ALPHA / NUMERIC / '_' / '.')*
//	VALUE   = QUOTED / DQUOTED / RAW
//	QUOTED  = '\'' .* '\''
//	DQUOTED = '"' .* '"'
//	RAW     = .* EOL
//	COMMENT = ('#' / ';') .* EOL
func (d *Decoder) Decode(v interface{}) (err error) {
	val := reflect.ValueOf(v)
	if val.Kind() != reflect.Ptr {
		return errors.New("non-pointer passed to Decode")
	}
	val = val.Elem()

	d.keyword = ""
	d.token.Reset()
	d.fields = getTypeInfo(val.Type())

	for state := lexInitial; state != nil && err == nil; {
		state, err = state(d, val)
	}

	if err == io.EOF {
		return nil
	}
	return err
}

// lexInitial is the entry point for configuration parsing.
func lexInitial(d *Decoder, v reflect.Value) (next stateFn, err error) {
	var ch rune

	for {
		ch, err = d.next()
		if err != nil {
			return nil, err
		}

		if unicode.IsSpace(ch) {
			continue
		}

		switch {
		case ch == '#' || ch == ';':
			return lexComment, nil
		case unicode.IsLetter(ch):
			d.token.WriteRune(ch)
			return lexKeyword, nil
		default:
			return nil, fmt.Errorf(
				"config: syntax error, expected keyword or comment, got %q", ch)
		}
	}
}

// lexKeyword parses a dotted, alpha-numeric identifier. When this
// function is called, the first character in the keyword has already
// been consumed.
func lexKeyword(d *Decoder, v reflect.Value) (next stateFn, err error) {
	var ch rune

	for {
		ch, err = d.next()
		if err != nil {
			return nil, err
		}

		switch {
		case isAlnum(ch) || ch == '.':
			d.token.WriteRune(ch)
		case unicode.IsSpace(ch):
			d.keyword = d.token.String()
			d.token.Reset()
			return lexDelimiter, nil
		case ch == '=':
			d.keyword = d.token.String()
			d.token.Reset()
			return lexValue, nil
		default:
			return nil, fmt.Errorf(
				"invalid character %q following keyword: %s", ch,
				d.token.String())
		}
	}
}

func isAlnum(r rune) bool {
	return r == '_' || unicode.IsLetter(r) || unicode.IsNumber(r)
}

// lexDelimiter parses the delimiter between keyword and value. Leading
// whitespace is skipped.
func lexDelimiter(d *Decoder, v reflect.Value) (next stateFn, err error) {
	var ch rune

	for {
		ch, err = d.next()
		if err != nil {
			if err == io.EOF {
				return nil, fmt.Errorf("expected delimiter after keyword %q",
					d.keyword)
			}
			return nil, err
		}

		if unicode.IsSpace(ch) {
			continue
		}

		if ch == '=' {
			return lexValue, nil
		}

		return nil, fmt.Errorf("expected delimiter after keyword '%s', got %q",
			d.keyword, ch)
	}
}

// lexValue parses the value following a keyword plus delimiter.
func lexValue(d *Decoder, v reflect.Value) (next stateFn, err error) {
	var ch rune

	// skip leading space, stopping at the first non-space character or EOL
	for {
		ch, err = d.next()
		if err != nil {
			if err != io.EOF {
				return nil, err
			}

			err = d.processKeyword(v)
			if err != nil {
				return nil, err
			}
			return nil, nil
		}

		if !unicode.IsSpace(ch) {
			break
		}

		if ch == '\n' {
			// remainder of the line was blank
			err = d.processKeyword(v)
			if err != nil {
				return nil, err
			}
			return lexInitial, nil
		}
	}

	switch ch {
	case '\'':
		return lexSingleQuoteString, nil
	case '"':
		return lexDoubleQuoteString, nil
	default:
		d.token.WriteRune(ch)
		return lexRawString, nil
	}
}

// lexSingleQuoteString parses the remainder of a single quoted string.
// When this function is called, the opening quote has already been
// consumed.
func lexSingleQuoteString(d *Decoder, v reflect.Value) (next stateFn,
	err error) {
	value, err := d.r.ReadBytes('\'')
	if err != nil {
		if err != io.EOF {
			return nil, err
		}
		return nil, fmt.Errorf("unexpected EOF: %q is missing a closing quote",
			d.keyword)
	}

	d.token.Write(value[:len(value)-1])
	err = d.processKeyword(v)
	if err != nil {
		return nil, err
	}
	return lexInitial, nil
}

var unescapeReplacer = strings.NewReplacer(
	"\\b", "\u0008",
	"\\t", "\u0009",
	"\\n", "\u000A",
	"\\v", "\u000B",
	"\\f", "\u000C",
	"\\r", "\u000D",
	"\\\"", "\u0022",
	"\\\\", "\u005C",
)

// lexDoubleQuoteString parses the remainder of a double quoted string.
// When this function is called, the opening quote has already been
// consumed.
func lexDoubleQuoteString(d *Decoder, v reflect.Value) (
	next stateFn, err error) {
	value, err := d.r.ReadBytes('"')
	if err != nil {
		if err != io.EOF {
			return nil, err
		}
		return nil, fmt.Errorf("unexpected EOF: %q is missing a closing quote",
			d.keyword)
	}

	d.token.Write(value[:len(value)-1])
	s := d.token.String()
	d.token.Reset()
	unescapeReplacer.WriteString(&d.token, s)

	err = d.processKeyword(v)
	if err != nil {
		return nil, err
	}
	return lexInitial, nil
}

func stripTrailingComment(line []byte) []byte {
	if i := bytes.IndexAny(line, "#;"); i != -1 {
		return line[:i]
	}
	return line
}

func trimRight(s []byte) []byte {
	return bytes.TrimRightFunc(s, unicode.IsSpace)
}

// lexRawString parses the remainder of an unquoted value. Unquoted
// values terminate at end of line, and do not include trailing whitespace.
func lexRawString(d *Decoder, v reflect.Value) (next stateFn, err error) {
	value, err := d.r.ReadBytes('\n')
	if err != nil && err != io.EOF {
		return nil, err
	}

	value = stripTrailingComment(value)
	value = trimRight(value)
	d.token.Write(value)

	if pkErr := d.processKeyword(v); pkErr != nil {
		return nil, pkErr
	}

	if err == io.EOF {
		return nil, nil
	}
	return lexInitial, nil
}

// lexComment parses the remainder of a comment. When this function is called,
// the comment start character has already been read. Comments extend until
// EOL or EOF, whichever comes first. The contents of the comment are ignored.
func lexComment(d *Decoder, v reflect.Value) (next stateFn, err error) {
	var isPrefix bool

	for {
		_, isPrefix, err = d.r.ReadLine()
		if err != nil {
			return nil, err
		}

		if !isPrefix {
			return lexInitial, nil
		}
	}
}

// next reads and returns the next character from the input stream.
func (d *Decoder) next() (r rune, err error) {
	r, _, err = d.r.ReadRune()
	return r, err
}

// processKeyword stores the value for the current keyword.
func (d *Decoder) processKeyword(v reflect.Value) error {
	defer func() {
		d.keyword = ""
		d.token.Reset()
	}()

	if idx, ok := d.fields[d.keyword]; ok {
		return unmarshalValue(v.FieldByIndex(idx), d.keyword, d.token.Bytes())
	}

	// no match found, ignore keyword
	return nil
}

// getTypeInfo returns a mapping of the keywords for all marshalable fields
// reachable from t to their index sequence.
func getTypeInfo(t reflect.Type) typeInfo {
	info := make(typeInfo)
	for i, n := 0, t.NumField(); i < n; i++ {
		field := t.Field(i)
		tag := field.Tag.Get("config")

		// skip unexported and ignored fields
		if field.PkgPath != "" || tag == "-" {
			continue
		}

		ft := field.Type
		if ft.Kind() == reflect.Ptr {
			ft = t.Elem()
		}

		if ft.Kind() == reflect.Struct {
			for keyword, idx := range getTypeInfo(ft) {
				info[keyword] = append([]int{i}, idx...)
			}
		} else if tag != "" {
			info[tag] = field.Index
		} else {
			info[field.Name] = field.Index
		}
	}

	return info
}

// FlagParserShim is a flag.Value that unmarshals flag values using
// ParseString()
type FlagParserShim struct {
	v interface{}
}

// NewFlagParserShim creates a FlagParserShim from a pointer.
func NewFlagParserShim(v interface{}) *FlagParserShim {
	return &FlagParserShim{v}
}

// Set parses a configuration setting value.
func (cv *FlagParserShim) Set(s string) error {
	return ParseString(s, cv.v)
}

// String always returns an empty string. It's implemented to satisfy the
// flag.Value interface.
func (cv *FlagParserShim) String() string {
	return ""
}
