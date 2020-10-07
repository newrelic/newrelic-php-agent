//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"strings"
)

// Implements a string flag that allows us to (optionally) load its value
// from a file.  If the first character of the type is a "@" then we'll
// trim that @, and use the remaining value as the file to load.
type FlagStringOrFile string

func (f *FlagStringOrFile) String() string {
	return string(*f)
}

func (f *FlagStringOrFile) Set(flagValue string) error {

	// does argument start with @?  If not just treat as string
	if "" == flagValue || '@' != flagValue[0] {
		*f = FlagStringOrFile(flagValue)
		return nil
	}

	fileName := flagValue[1:]
	contents, err := ioutil.ReadFile(fileName)

	if nil != err {
		fmt.Fprintf(os.Stderr, "unable read file [%v], Error: [%v]\n", flagValue[1:], err)
		return err
	}

	*f = FlagStringOrFile(strings.TrimSpace(string(contents)))
	return nil
}
