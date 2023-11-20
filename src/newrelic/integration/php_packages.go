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
// Params 1 : JSON byte string containing upload_module_updates endpoint data
//
// Returns :  []PhpPackage with extracted package info, sorted by package name
//            nil upon error processing JSON
func GetPhpPackagesFromData(data []byte) ([]PhpPackage, error) {
	var pkgs []PhpPackage
	var x []interface{}

	// extract string "Jars" and array of Php Packages JSON
	err := json.Unmarshal(data, &x)
	fmt.Printf("initial x = %+v\n", x)
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
	fmt.Printf("Passed first test\n")

	// walk through array of Php packages JSON
	v2, _ := x[1].([]interface{})
	fmt.Printf("v2 = %+v\n", v2)
	for i, e1 := range v2 {
		v3, _ := e1.([]interface{})
		if 3 != len(v3) {
			return nil, fmt.Errorf("Expected php packages json to have 3 values, has %d : %+v", len(v3), v3)
		}
		fmt.Printf("pkg #%d: len = %d name = '%s' version = '%s'\n", i, len(v3), v3[0].(string), v3[1].(string))
		pkgs = append(pkgs, PhpPackage{v3[0].(string), v3[1].(string)})
	}

	// MSF TESTING
	// add a few packages to test sorting/comparing
	pkgs = append(pkgs, PhpPackage{"predis/predis", "3.5.0"})
	pkgs = append(pkgs, PhpPackage{"guzzlehttp/guzzle", "1.2.9"})
	//pkgs = append(pkgs, PhpPackage{"slim/slim", "5.0.7"})
	// MSF TESTING

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
		fmt.Printf("config line %s\n", string(line))
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

	fmt.Printf("params map = %+v\n", params)

	// verify command and supported_list are defined
	_, ok := params["command"]
	fmt.Printf("ok 1 = %t\n", ok)
	if ok {
		_, ok = params["supported_packages"]
		fmt.Printf("ok 2 = %t\n", ok)
	}
	fmt.Printf("ok final = %t\n", ok)

	if !ok {
		return nil, fmt.Errorf("Improper php applications config - must include 'command' and 'supported_packages' keys, got %+v", params)
	}

	p := &PhpPackagesCollection{
		//config: PhpPackagesConfiguration{command: "composer show -f json", supported_list_file: "../../../include/supported_php_packages.json"},
		config: PhpPackagesConfiguration{command: params["command"], path: path, supported_list_file: params["supported_packages"]},
	}

	fmt.Printf("Final p = %+v\n", p)

	return p, nil
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

	fmt.Printf("pkgs = %+v\n", pkgs)

	if nil == pkgs {
		return nil, fmt.Errorf("GatherInstallPackages(): pkgs is nil")
	}

	jsonFile, err := os.Open(filepath.Dir(pkgs.config.path) + "/" + pkgs.config.supported_list_file)
	if err != nil {
		fmt.Println(err)
		return nil, fmt.Errorf("error opening supported list %s", err.Error())
	} else {
		fmt.Printf("Successfully Opened %s\n", pkgs.config.supported_list_file)
		defer jsonFile.Close()
	}

	supported_json, err := ioutil.ReadAll(jsonFile)
	if err != nil {
		fmt.Printf("Error reading supported list!\n")
	} else {
		fmt.Printf("supported_json = %s\n", supported_json)
	}

	var supported []string

	err = json.Unmarshal([]byte(supported_json), &supported)
	if nil != err {
		fmt.Printf("Error unmarshalling supported list %s\n", err.Error())
		return nil, fmt.Errorf("Error unmarshalling supported list %s\n", err.Error())
	}
	fmt.Printf("supported len = %d contents = %+v\n", len(supported), supported)

	//pkgs = NewPhpPackagesCollection([]byte(""))
	//fmt.Printf("GatherInstalledPackages using path %s, config %+v\n", path, pkgs.config)

	splitCmd := strings.Split(pkgs.config.command, " ")
	cmd := exec.Command(splitCmd[0], splitCmd[1:]...)
	cmd.Dir = filepath.Dir(pkgs.config.path)
	cmd.Stderr = os.Stderr

	var out []byte

	out, err = cmd.Output()
	if nil != err {
		fmt.Printf("Error from output command %s\n", err.Error())
	}

	//fmt.Printf("The output from composer is %s\n", out)

	if "composer" == splitCmd[0] {
		detected := ComposerJSON{}
		json.Unmarshal([]byte(out), &detected)
		//fmt.Printf("detected len = %d contents = %+v\n", len(detected.Installed), detected)
		for _, v := range detected.Installed {
			if slices.Contains(supported, v.Name) {
				pkgs.packages = append(pkgs.packages, PhpPackage{v.Name, v.Version})
			}
		}
	} else {
		fmt.Printf("ERROR - unknown method '%s'\n", splitCmd[0])
		return nil, fmt.Errorf("ERROR - unknown method '%s'\n", splitCmd[0])
	}

	// MSF TESTING
	// add a few packages to test sorting/comparing
	pkgs.packages = append(pkgs.packages, PhpPackage{"predis/predis", "3.4.0"})
	pkgs.packages = append(pkgs.packages, PhpPackage{"guzzlehttp/guzzle", "1.2.3"})
	// MSF TESTING

	// sort by package name to aid comparision later
	sort.Slice(pkgs.packages, func(i, j int) bool {
		return pkgs.packages[i].Name < pkgs.packages[j].Name
	})

	//fmt.Printf("pkgs.packages len = %d contexts = %+v\n", len(pkgs.packages), pkgs.packages)

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
