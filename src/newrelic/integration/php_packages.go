//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"slices"
	"sort"
	"strings"
)

// PHP packages collection is a JSON string containing the detected PHP packages
// using a tool like composer
type PhpPackage struct {
	Name    string
	Version string
}
type PhpPackagesCollection struct {
	config   PhpPackagesConfiguration
	packages []PhpPackage
}

// PHP packages config describes how to collect the JSON for the packages installed
// for the current test case
type PhpPackagesConfiguration struct {
	path                string
	command             string
	supported_list_file string
	expected_packages   []string
	package_name_only   []string
}

// composer package JSON
type ComposerPackage struct {
	Name        string
	Version     string
	Description string
}

// composer entire JSON
type ComposerJSON struct {
	Installed []ComposerPackage
}

//
// Given a JSON harvest payload, extract the PHP packages
//
// Params 1 : JSON byte string containing update_loaded_modules endpoint data
//
// Returns :  []PhpPackage with extracted package info, sorted by package name
//            nil upon error processing JSON
func GetPhpPackagesFromData(data []byte) ([]PhpPackage, error) {
	var pkgs []PhpPackage
	var x []interface{}

	// extract string "Jars" and array of Php Packages JSON
	err := json.Unmarshal(data, &x)
	if nil != err {
		return nil, err
	}
	if 2 != len(x) {
		return nil, fmt.Errorf(("Expectd top level array of len 2"))
	}
	str, ok := x[0].(string)
	if !ok || "Jars" != str {
		return nil, fmt.Errorf(("Expected \"Jars\" string at top level"))
	}

	// walk through array of Php packages JSON
	v2, _ := x[1].([]interface{})
	for _, e1 := range v2 {
		v3, _ := e1.([]interface{})
		if 3 != len(v3) {
			return nil, fmt.Errorf("Expected php packages json to have 3 values, has %d : %+v", len(v3), v3)
		}
		pkgs = append(pkgs, PhpPackage{v3[0].(string), v3[1].(string)})
	}

	// sort by package name to aid comparision later
	sort.Slice(pkgs, func(i, j int) bool {
		return pkgs[i].Name < pkgs[j].Name
	})

	return pkgs, nil
}

// convert PhpPackage to collector JSON representation
func (pkg *PhpPackage) CollectorJSON() ([]byte, error) {
	buf := &bytes.Buffer{}

	buf.WriteString(`["`)
	buf.WriteString(pkg.Name)
	buf.WriteString(`","`)
	buf.WriteString(pkg.Version)
	buf.WriteString(`",{}]`)

	return buf.Bytes(), nil
}

// Create a package collection - requires config data from the EXPECT_PHP_PACKAGES stanza
func NewPhpPackagesCollection(path string, config []byte) (*PhpPackagesCollection, error) {

	params := make(map[string]string)

	for _, line := range bytes.Split(config, []byte("\n")) {
		trimmed := bytes.TrimSpace(line)
		if len(trimmed) == 0 {
			continue
		}

		var key, value string

		kv := bytes.SplitN(trimmed, []byte("="), 2)
		key = string(bytes.TrimSpace(kv[0]))
		if len(kv) == 2 {
			value = string(bytes.TrimSpace(kv[1]))
		}

		if key == "" {
			return nil, fmt.Errorf("invalid config format '%s'", string(config))
		}

		params[key] = value
	}

	// verify command and supported_list are defined
	var supported_list_file string
	var expected_packages string
	var package_name_only string
	var expected_packages_arr []string
	var package_name_only_arr []string
	var command_ok, supported_ok, expected_ok bool
	var ok bool
	var err error

	command, command_ok := params["command"]

	// either expect a "supported_packages" key which specifies a file listing all possible packages agent
	// can detect and this is used to filter the auto-discovered packages (by integration_runner using "command")
	supported_list_file, supported_ok = params["supported_packages"]

	// or "expected_packages" which is specifies a fixed list of packages we expect to show up in this test
	expected_packages, expected_ok = params["expected_packages"]
	if expected_ok {
		expected_packages_arr, err = ParsePackagesList(expected_packages)
		if nil != err {
			return nil, fmt.Errorf("Error parsing expected_packages list %s\n", err.Error())
		}
	}

	if supported_ok && expected_ok {
		return nil, fmt.Errorf("Improper EXPECT_PHP_PACKAGES config - cannot specify 'supported_packages' and 'expected packages' - got %+v", params)
	}

	if !supported_ok && !expected_ok {
		return nil, fmt.Errorf("Improper EXPECT_PHP_PACKAGES config - must specify 'supported_packages' or 'expected packages' - got %+v", params)
	}

	if supported_ok && !command_ok {
		return nil, fmt.Errorf("Improper EXPECT_PHP_PACKAGES config - must specify 'command' option with `supported_packages` - got %+v", params)
	}

	// optional option to specify which packages will only have a name because agent cannot determine the version
	package_name_only, ok = params["package_name_only"]
	if ok {
		package_name_only_arr, err = ParsePackagesList(package_name_only)
		if nil != err {
			return nil, fmt.Errorf("Error parsing package_name_only list %s\n", err.Error())
		}
	}

	p := &PhpPackagesCollection{
		config: PhpPackagesConfiguration{
			command:             command,
			path:                path,
			supported_list_file: supported_list_file,
			expected_packages:   expected_packages_arr,
			package_name_only:   package_name_only_arr},
	}

	return p, nil
}

func LoadSupportedPackagesList(path, supported_list_file string) ([]string, error) {

	jsonFile, err := os.Open(filepath.Dir(path) + "/" + supported_list_file)
	if err != nil {
		return nil, fmt.Errorf("error opening supported list %s!", err.Error())
	} else {
		defer jsonFile.Close()
	}

	supported_json, err := ioutil.ReadAll(jsonFile)
	if err != nil {
		return nil, fmt.Errorf("Error reading supported list %s!", err.Error())
	}

	var supported []string

	err = json.Unmarshal([]byte(supported_json), &supported)
	if nil != err {
		return nil, fmt.Errorf("Error unmarshalling supported list %s\n", err.Error())
	}

	return supported, nil
}

// expect string containing comma separated list of package names
// returns an array of strings with all leading/trailing whitespace removed
func ParsePackagesList(expected_packages string) ([]string, error) {
	tmp := strings.Replace(expected_packages, " ", "", -1)
	return strings.Split(tmp, ","), nil
}

// Detects installed PHP packages
//
// Returns :  []PhpPackage with extracted package info, sorted by package name
//            nil upon error processing JSON
//
// Notes   :  Currently only supports an application created with composer
//
func (pkgs *PhpPackagesCollection) GatherInstalledPackages() ([]PhpPackage, error) {

	var err error

	if nil == pkgs {
		return nil, fmt.Errorf("GatherInstallPackages(): pkgs is nil")
	}

	var supported []string

	// get list of packages we expected the agent to detect
	// this can be one of 2 scenarios:
	//  1) test case used the "supported_packages" option which gives a JSON file which
	//     lists all the packages the agent can detect
	//  2) test case used the "expected_packages" options which provides a comma separated
	//     list of packages we expect the agent to detect
	//
	//  Option #1 is preferable as it provides the most comprehensive view of what the agent can do.
	//
	//  Option #2 is needed because some test cases do not exercise all the packages which are 
	//  installed and so the agent will not detect everything for that test case run which it could
	//  theorectically detect if the test case used all the available packages installed.
	//
	//  Once the list of packages the agent is expected to detect is created it is used to filter
	//  down the package list returned by running the "command" (usually composer) option for the
	//  test case provided.
	if 0 < len(pkgs.config.supported_list_file) {
		supported, err = LoadSupportedPackagesList(pkgs.config.path, pkgs.config.supported_list_file)
		if nil != err {
			return nil, err
		}
	} else if 0 < len(pkgs.config.expected_packages) {
		supported = pkgs.config.expected_packages
	} else {
		return nil, fmt.Errorf("Error determining expected packages - supported_list_file and expected_packages are both empty")
	}

	splitCmd := strings.Split(pkgs.config.command, " ")
	cmd := exec.Command(splitCmd[0], splitCmd[1:]...)
	cmd.Dir = filepath.Dir(pkgs.config.path)
	cmd.Stderr = os.Stderr

	var out []byte

	out, err = cmd.Output()
	if nil != err {
		fmt.Printf("Error from output command %s\n", err.Error())
	}

	if "composer" == splitCmd[0] {
		detected := ComposerJSON{}
		json.Unmarshal([]byte(out), &detected)
		for _, v := range detected.Installed {
			//fmt.Printf("composer detected %s %s\n", v.Name, v.Version)
			if slices.Contains(supported, v.Name) {
				var version string

				// remove any 'v' from front of version string
				if 0 < len(v.Version) && string(v.Version[0]) == "v" {
					version = v.Version[1:]
				} else {
					version = v.Version
				}
				pkgs.packages = append(pkgs.packages, PhpPackage{v.Name, version})
				//fmt.Printf("   -> %s in supported!\n", v.Name)
			} else {
				//fmt.Printf("   -> %s NOT in supported!\n", v.Name)
			}
		}
	} else if 1 < len(splitCmd) && "wp-cli.phar" == splitCmd[1] {
		lines := strings.Split(string(out), "\n")
		version := ""
		for _, line := range lines {
			//fmt.Printf("line is |%s|\n", line)
			splitLine := strings.Split(line, ":")
			if 2 == len(splitLine) {
				if "wordpress version" == strings.TrimSpace(strings.ToLower(splitLine[0])) {
					version = strings.TrimSpace(splitLine[1])
					//fmt.Printf("wordpress version is %s\n", version)
					break
				}
			}
		}
		if 0 < len(version) {
			pkgs.packages = append(pkgs.packages, PhpPackage{"wordpress", version})
		}
	} else {
		return nil, fmt.Errorf("ERROR - unknown method '%s'\n", splitCmd[0])
	}

	// sort by package name to aid comparision later
	sort.Slice(pkgs.packages, func(i, j int) bool {
		return pkgs.packages[i].Name < pkgs.packages[j].Name
	})

	return pkgs.packages, nil
}

// convert PhpPackage to collector JSON representation
func (pkgs *PhpPackagesCollection) CollectorJSON() ([]byte, error) {
	buf := &bytes.Buffer{}

	buf.WriteString(`["Jars",[`)
	for i, pkg := range pkgs.packages {
		json, _ := pkg.CollectorJSON()
		buf.Write(json)
		if i != (len(pkgs.packages) - 1) {
			buf.WriteByte(',')
		}
	}
	buf.WriteString(`]]`)

	return buf.Bytes(), nil
}
