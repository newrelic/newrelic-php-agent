//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package sysinfo

import (
	"errors"
)

var (
	ErrFeatureUnsupported = errors.New("That feature is not supported on this platform")
	ErrIdentifierNotFound = errors.New("The requested identifier could not be found")
)
