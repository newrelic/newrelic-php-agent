//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"slices"
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

// Create a package collection - requires config data from the EXPECT_PHP_PACKAGES stanza
func NewPhpPackagesCollection(cfg []byte) *PhpPackagesCollection {

	p := &PhpPackagesCollection{
		config: PhpPackagesConfiguration{command: "composer show -f json", supported_list_file: "../../../include/supported_php_packages.json"},
	}

	return p
}

// Given a configuration that defines how to find installed packages, detect
// installed packages
func (pkgs *PhpPackagesCollection) GatherInstalledPackages(path string) ([]PhpPackage, error) {
	// if nil == pkgs {
	// 	return fmt.Errorf("GatherInstallPackages(): pkgs is nil")
	// }

	var err error

	jsonFile, err := os.Open(filepath.Dir(path) + "/" + pkgs.config.supported_list_file)
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

	pkgs = NewPhpPackagesCollection([]byte(""))
	fmt.Printf("GatherInstalledPackages using path %s, config %+v\n", path, pkgs.config)

	splitCmd := strings.Split(pkgs.config.command, " ")
	cmd := exec.Command(splitCmd[0], splitCmd[1:]...)
	cmd.Dir = filepath.Dir(path)
	cmd.Stderr = os.Stderr

	var out []byte

	out, err = cmd.Output()
	if nil != err {
		fmt.Printf("Error from output command %s\n", err.Error())
	}
	fmt.Printf("The output from composer is %s\n", out)

	if "composer" == splitCmd[0] {
		detected := ComposerJSON{}
		json.Unmarshal([]byte(out), &detected)
		fmt.Printf("detected len = %d contents = %+v\n", len(detected.Installed), detected)
		for _, v := range detected.Installed {
			if slices.Contains(supported, v.Name) {
				pkgs.packages = append(pkgs.packages, PhpPackage{v.Name, v.Version})
			}
		}
	} else {
		fmt.Printf("ERROR - unknown method '%s'\n", splitCmd[0])
		return nil, fmt.Errorf("ERROR - unknown method '%s'\n", splitCmd[0])
	}

	fmt.Printf("pkgs.packages len = %d contexts = %+v\n", len(pkgs.packages), pkgs.packages)

	return pkgs.packages, nil
}
