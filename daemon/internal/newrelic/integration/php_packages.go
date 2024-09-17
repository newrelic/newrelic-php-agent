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
	path                 string   //
	command              string   // command to run to detect packages
	supportedListFile    string   // JSON file containing list of packages we expect agent to detect
	overrideVersionsFile string   // JSON file containing overrides for expected package versions
	expectedPackages     []string // manual override of packages we expect to detect
	packageNameOnly      []string // list of packages which only have a name because agent cannot determine the version
	expectAllDetected    bool     // flag to indicate we expect all packages detected by the command "command"
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

// Given a JSON harvest payload, extract the PHP packages
//
// Params 1:
//   - JSON byte string containing update_loaded_modules endpoint data
//
// Returns:
//   - []PhpPackage with extracted package info, sorted by package name
//   - nil upon error processing JSON
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
	var supportedListFile string
	var expectedPackages string
	var packageNameOnly string
	var overrideVersionsFile string
	var expectedPackagesArr []string
	var packageNameOnlyArr []string
	var commandOK, supportedOK, expectedOK bool
	var ok bool
	var err error

	command, commandOK := params["command"]

	// either expect a "supported_packages" key which specifies a file listing all possible packages agent
	// can detect and this is used to filter the auto-discovered packages (by integration_runner using "command")
	supportedListFile, supportedOK = params["supported_packages"]

	// or "expected_packages" which specifies a fixed list of packages we expect to show up in this test
	expectedPackages, expectedOK = params["expected_packages"]
	if expectedOK {
		expectedPackagesArr, err = ParsePackagesList(expectedPackages)
		if nil != err {
			return nil, fmt.Errorf("Error parsing expected_packages list %s\n", err.Error())
		}
	}

	// or "expect_all" which means we expect the agent to detect all the packages that the "command" option would detect
	_, expectAllOK := params["expect_all"]

	if (supportedOK && expectedOK) || (supportedOK && expectAllOK) || (expectedOK && expectAllOK) {
		return nil, fmt.Errorf("Improper EXPECT_PHP_PACKAGES config - must specify one of 'supported_packages', "+
			"'expected packages' or 'expect_all' - got %+v", params)
	}

	if !supportedOK && !expectedOK && !expectAllOK {
		return nil, fmt.Errorf("Improper EXPECT_PHP_PACKAGES config - must specify 'supported_packages' or 'expected packages' "+
			"or 'expect_all' - got %+v", params)
	}

	if (supportedOK || expectAllOK) && !commandOK {
		return nil, fmt.Errorf("Improper EXPECT_PHP_PACKAGES config - must specify 'command' option with `supported_packages` / "+
			"'expect_all' - got %+v", params)
	}

	// optional option to specify which packages will only have a name because agent cannot determine the version
	packageNameOnly, ok = params["package_name_only"]
	if ok {
		packageNameOnlyArr, err = ParsePackagesList(packageNameOnly)
		if nil != err {
			return nil, fmt.Errorf("Error parsing package_name_only list %s\n", err.Error())
		}
	}

	// option file containing overrides for expected package versions
	// this is useful when a package is detected as the wrong version
	// because the internal mechanism the agent uses to get pacakge
	// versions was not updated by the upstream maintainers properly
	// on a release.
	//
	// this is a JSON file of the format
	//  {
	//     "<expected>": "<override",
	//     ...
	//  }
	//
	// such as:
	//  {
	//     "4.13.0": "4.12.0",
	//     "3.4.5": "3.4.4"
	//  }
	//
	//  which creates overrides to version "4.13.0" to change its expecation to "4.12.0"
	//  and "3.4.5" to be changed to an expectation of "3.4.4"
	overrideVersionsFile, ok = params["override_versions_file"]

	p := &PhpPackagesCollection{
		config: PhpPackagesConfiguration{
			command:              command,
			path:                 path,
			supportedListFile:    supportedListFile,
			overrideVersionsFile: overrideVersionsFile,
			expectedPackages:     expectedPackagesArr,
			packageNameOnly:      packageNameOnlyArr,
			expectAllDetected:    expectAllOK},
	}

	return p, nil
}

func LoadSupportedPackagesList(path, supportedListFile string) ([]string, error) {

	jsonFile, err := os.Open(filepath.Dir(path) + "/" + supportedListFile)
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
func ParsePackagesList(expectedPackages string) ([]string, error) {
	tmp := strings.ReplaceAll(expectedPackages, " ", "")
	return strings.Split(tmp, ","), nil
}

func ParseOverrideVersionsFile(path, overrideVersionFile string) (map[string]interface{}, error) {
	jsonFile, err := os.Open(filepath.Dir(path) + "/" + overrideVersionFile)
	if err != nil {
		return nil, fmt.Errorf("error opening versions override list %s!", err.Error())
	} else {
		defer jsonFile.Close()
	}

	versions_json, err := ioutil.ReadAll(jsonFile)
	if err != nil {
		return nil, fmt.Errorf("Error reading versions override list %s!", err.Error())
	}

	var versions_override map[string]interface{}

	err = json.Unmarshal([]byte(versions_json), &versions_override)
	if nil != err {
		return nil, fmt.Errorf("Error unmarshalling versions override list %s\n", err.Error())
	}

	return versions_override, nil
}

// Returns name of versions override file (if exists)
func (pkgs *PhpPackagesCollection) OverrideVersionsFile() string {
	if nil == pkgs {
		return ""
	}

	return pkgs.config.overrideVersionsFile
}

// Detects installed PHP packages
//
// Returns:
//   - []PhpPackage with extracted package info, sorted by package name
//   - nil upon error processing JSON
//
// Notes: Currently only supports an application created with composer
func (pkgs *PhpPackagesCollection) GatherInstalledPackages() ([]PhpPackage, error) {

	var err error

	if nil == pkgs {
		return nil, fmt.Errorf("GatherInstallPackages(): pkgs is nil")
	}

	var supported []string

	// get list of packages we expected the agent to detect
	// this can be one of 3 scenarios:
	//  1) test case used the "supported_packages" option which gives a JSON file which
	//     lists all the packages the agent can detect
	//  2) test case used the "expected_packages" options which provides a comma separated
	//     list of packages we expect the agent to detect
	//  3) test case used the "expect_all" option which means we expect the agent to
	//     detect all the packages that the "command" option would detect
	//
	//  Options #1 and #2 are mutually exclusive, and are intended for testing the legacy VM detection
	//  mechanism where the agent looks for "magic" files of a package and examinew internals of the
	//  package to determine its version.
	//
	//  Option #1 is preferable when it available as it provides the most comprehensive view of what the agent can do.
	//
	//  Option #2 is needed because some test cases do not exercise all the packages which are
	//  installed and so the agent will not detect everything for that test case run which it could
	//  theorectically detect if the test case used all the available packages installed.
	//
	//  Option #3 is used when testing the agent's ability to detect packages using the Composer API.  In
	//  this case we expect the agent to detect the exact same packages as composer would detect.
	//
	//  Once the list of packages the agent is expected to detect is created it is used to filter
	//  down the package list returned by running the "command" (usually composer) option for the
	//  test case provided.
	if 0 < len(pkgs.config.supportedListFile) {
		supported, err = LoadSupportedPackagesList(pkgs.config.path, pkgs.config.supportedListFile)
		if nil != err {
			return nil, err
		}
	} else if 0 < len(pkgs.config.expectedPackages) {
		supported = pkgs.config.expectedPackages
	} else if !pkgs.config.expectAllDetected {
		return nil, fmt.Errorf("Error determining expected packages - supported_packages and expected_packages are both empty " +
			"and expect_all is false")
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
			if pkgs.config.expectAllDetected || StringSliceContains(supported, v.Name) {
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
	} else if 1 < len(splitCmd) && "composer-show.php" == splitCmd[1] {
		lines := strings.Split(string(out), "\n")
		version := ""
		for _, line := range lines {
			//fmt.Printf("line is |%s|\n", line)
			splitLine := strings.Split(line, "=>")
			if 2 == len(splitLine) {
				name := strings.TrimSpace(splitLine[0])
				version = strings.TrimSpace(splitLine[1])
				pkgs.packages = append(pkgs.packages, PhpPackage{name, version})
			}
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
