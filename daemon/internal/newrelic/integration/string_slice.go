package integration

import "slices"

// check if string is contained in an array of strings
// for legacy Go as it would be preferable to use the slices
func StringSliceContains(slice []string, text string) bool {
	return slices.Contains(slice, text)
}
