package newrelic

import (
	"strings"
	"testing"
	"time"
)

func TestNewPhpPackages(t *testing.T) {
	pkg := NewPhpPackages()

	if nil == pkg {
		t.Fatal("Expected not nil")
	}
	if 0 != pkg.numSeen {
		t.Fatalf("Expected 0, got %d", pkg.numSeen)
	}
	if 0 != pkg.NumSaved() {
		t.Fatalf("Expected 0, got %f", pkg.NumSaved())
	}
	if nil != pkg.data {
		t.Fatalf("Expected nil, got %v", pkg.data)
	}
}

func TestAddPhpPackagesFromData(t *testing.T) {
	// create nil pkgs for testing passing a nil receiver
	var nilpkg *PhpPackages
	validPkgData := []byte(`[["package_a","1.2.3",{}]]`)
	oooPkgData := []byte(`[[{},"package_a","1.2.3"]]`)
	emptyPkgData := []byte(`[[]]`)
	invalidFmt := []byte(`["package_a","1.2.3",{}]`)
	emptyStr := []byte(``)
	emptyNameStr := []byte(`[["","1.2.3",{}]]`)
	missingElem := []byte(`[["package_a", "1.2.3"]]`)
	excessElem := []byte(`[["package_a", "1.2.3", "x.y.z", {}]]`)

	err := nilpkg.AddPhpPackagesFromData(validPkgData)
	if err == nil {
		t.Fatalf("Expected error 'packages is nil!', got nil")
	} else if !strings.Contains(err.Error(), "packages is nil") {
		t.Fatalf("Expected error 'packages is nil!', got '%+v'", err.Error())
	}

	pkgs := NewPhpPackages()
	if nil == pkgs {
		t.Fatal("Expected not nil")
	}

	// nil data
	err = pkgs.AddPhpPackagesFromData(nil)
	if err == nil || len(pkgs.data) != 0 {
		t.Fatalf("Expected error 'data is nil!', got nil")
	} else if !strings.Contains(err.Error(), "data is nil") {
		t.Fatalf("Expected error 'data is nil!', got '%+v'", err.Error())
	}

	// empty string data
	err = pkgs.AddPhpPackagesFromData(emptyStr)
	if err == nil || len(pkgs.data) != 0 {
		t.Fatalf("Expected error 'data is nil!', got nil")
	} else if !strings.Contains(err.Error(), "data is nil") {
		t.Fatalf("Expected error 'data is nil!', got '%+v'", err.Error())
	}

	// out-of-order data
	err = pkgs.AddPhpPackagesFromData(oooPkgData)
	if err == nil || len(pkgs.data) != 0 {
		t.Fatalf("Expected error 'unable to parse package name', got nil")
	} else if !strings.Contains(err.Error(), "unable to parse package name") {
		t.Fatalf("Expected error 'unable to parse package name', got '%+v'", err.Error())
	}

	// empty json array
	err = pkgs.AddPhpPackagesFromData(emptyPkgData)
	if err == nil || len(pkgs.data) != 0 {
		t.Fatalf("Expected error 'invalid php package json structure', got nil")
	} else if !strings.Contains(err.Error(), "invalid php package json structure") {
		t.Fatalf("Expected error 'invalid php package json structure', got '%+v'", err.Error())
	}

	// invalid json array
	err = pkgs.AddPhpPackagesFromData(invalidFmt)
	if err == nil || len(pkgs.data) != 0 {
		t.Fatalf("Expected error 'invalid php package json structure', got nil")
	} else if !strings.Contains(err.Error(), "invalid php package json structure") {
		t.Fatalf("Expected error 'invalid php package json structure', got '%+v'", err.Error())
	}

	// empty field value
	err = pkgs.AddPhpPackagesFromData(emptyNameStr)
	if err == nil || len(pkgs.data) != 0 {
		t.Fatalf("Expected error 'invalid php package json structure', got nil")
	} else if !strings.Contains(err.Error(), "unable to parse package name") {
		t.Fatalf("Expected error 'unable to parse package name', got '%+v'", err.Error())
	}

	// missing field value
	err = pkgs.AddPhpPackagesFromData(missingElem)
	if err == nil || len(pkgs.data) != 0 {
		t.Fatalf("Expected error 'invalid php package json structure', got nil")
	} else if !strings.Contains(err.Error(), "invalid php package json structure") {
		t.Fatalf("Expected error 'invalid php package json structure', got '%+v'", err.Error())
	}

	// too many values
	err = pkgs.AddPhpPackagesFromData(excessElem)
	if err == nil || len(pkgs.data) != 0 {
		t.Fatalf("Expected error 'invalid php package json structure', got nil")
	} else if !strings.Contains(err.Error(), "invalid php package json structure") {
		t.Fatalf("Expected error 'invalid php package json structure', got '%+v'", err.Error())
	}

	// valid pkgs, valid data
	err = pkgs.AddPhpPackagesFromData(validPkgData)
	if err != nil {
		t.Fatalf("Expected nil error, got %s", err.Error())
	}
}

func TestCollectorJSON(t *testing.T) {
	// create nil pkgs for testing passing a nil receiver
	info := AppInfo{}
	app := NewApp(&info)

	var nilpkg *PhpPackages
	id := AgentRunID(`12345`)

	json, err := nilpkg.CollectorJSON(id)
	if nil != err {
		t.Fatalf("Expected nil error, got %s", err.Error())
	}
	expectedJSON := `["Jars",[]]`
	if expectedJSON != string(json) {
		t.Fatalf("Expected '%s', got '%s'", expectedJSON, string(json))
	}

	// test with valid pkgs
	pkg := NewPhpPackages()
	if nil == pkg {
		t.Fatalf("Expected not nil")
	}
	json, err = pkg.CollectorJSON(id)
	if nil != err {
		t.Fatalf("Expected nil error, got %s", err.Error())
	}
	expectedJSON = `["Jars",[]]`
	if expectedJSON != string(json) {
		t.Fatalf("Expected '%s', got '%s'", expectedJSON, string(json))
	}

	pkg.AddPhpPackagesFromData([]byte(`[["package_a", "1.2.3",{}]]`))
	pkg.AddPhpPackagesFromData([]byte(`[["package_b", "1.2.3",{}]]`))

	pkg.Filter(app.PhpPackages)
	json, err = pkg.CollectorJSON(id)
	if nil != err {
		t.Fatalf("Expected nil error, got %s", err.Error())
	}
	expectedJSON = `["Jars",[["package_a","1.2.3",{}],["package_b","1.2.3",{}]]]`
	if expectedJSON != string(json) {
		t.Fatalf("Expected '%s', got '%s'", expectedJSON, string(json))
	}

	// Data method should return same values
	now := time.Now()
	json, err = pkg.Data(id, now)
	if nil != err {
		t.Fatalf("Expected nil error, got %s", err.Error())
	}
	if expectedJSON != string(json) {
		t.Fatalf("Expected '%s', got '%s'", expectedJSON, string(json))
	}
}

func TestPackagesEmpty(t *testing.T) {
	// create nil pkgs for testing passing a nil receiver
	var nilpkg *PhpPackages

	empty := nilpkg.Empty()
	if false == empty {
		t.Fatalf("Expected 'true' got '%t'", empty)
	}

	// test with valid pkgs
	pkg := NewPhpPackages()
	if nil == pkg {
		t.Fatalf("Expected not nil")
	}
	empty = pkg.Empty()
	if false == empty {
		t.Fatalf("Expected 'true' got '%t'", empty)
	}

	// test with data
	validData := []byte(`[["package","version",{}]]`)
	err := pkg.AddPhpPackagesFromData(validData)
	if nil != err {
		t.Fatal("Expected not nil")
	}
	empty = pkg.Empty()
	if true == empty {
		t.Fatalf("Expected 'false' got '%t'", empty)
	}
}

func comparePkgs(expect, actual *PhpPackagesKey) bool {
	if expect == nil || actual == nil {
		return false
	}

	if expect.Name != actual.Name || expect.Version != actual.Version {
		return false
	}
	return true
}

func TestFilterPackageData(t *testing.T) {
	info := AppInfo{}
	app := NewApp(&info)
	pkg := NewPhpPackages()
	expect_a := PhpPackagesKey{"package_a", "1.2.3"}
	expect_b := PhpPackagesKey{"package_b", "1.2.3"}
	expect_c := PhpPackagesKey{"package_c", "1.2.3"}

	// Test nil package data
	pkg.Filter(app.PhpPackages)
	if pkg.filteredPkgs != nil {
		t.Fatalf("Expected nil, got '%+v'", pkg.filteredPkgs)
	}

	// Test empty package data
	pkg.AddPhpPackagesFromData([]byte(`[[]]`))
	pkg.Filter(app.PhpPackages)
	if pkg.filteredPkgs != nil {
		t.Fatalf("Expected nil, got '%+v'", pkg.filteredPkgs)
	}

	// Test invalid payload
	pkg.AddPhpPackagesFromData([]byte(`["invalid","x",{}]`))
	pkg.Filter(app.PhpPackages)
	if pkg.filteredPkgs != nil {
		t.Fatalf("Expected nil, got '%+v'", pkg.filteredPkgs)
	}

	// Test invalid number of elements
	// too few
	pkg.AddPhpPackagesFromData([]byte(`[["package", "x.y.z"]]`))
	pkg.Filter(app.PhpPackages)
	if pkg.filteredPkgs != nil {
		t.Fatalf("Expected nil, got '%+v'", pkg.filteredPkgs)
	}

	// wrong order
	pkg.AddPhpPackagesFromData([]byte(`[[{}, "package", "x.y.z"]]`))
	pkg.Filter(app.PhpPackages)
	if pkg.filteredPkgs != nil {
		t.Fatalf("Expected nil, got '%+v'", pkg.filteredPkgs)
	}

	// wrong order
	pkg.AddPhpPackagesFromData([]byte(`[["package", {}, "x.y.z"]]`))
	pkg.Filter(app.PhpPackages)
	if pkg.filteredPkgs != nil {
		t.Fatalf("Expected nil, got '%+v'", pkg.filteredPkgs)
	}

	// too many
	pkg.AddPhpPackagesFromData([]byte(`[["package", "x.y.z", "u.v.w", {}]]`))
	pkg.Filter(app.PhpPackages)
	if pkg.filteredPkgs != nil {
		t.Fatalf("Expected nil, got '%+v'", pkg.filteredPkgs)
	}

	// Test single valid package
	pkg.AddPhpPackagesFromData([]byte(`[["package_a", "1.2.3",{}]]`))
	pkg.Filter(app.PhpPackages)
	if !comparePkgs(&expect_a, &pkg.filteredPkgs[0]) || len(pkg.filteredPkgs) != 1 {
		t.Fatalf("Expected '%+v', got '%+v'", expect_a, pkg.filteredPkgs[0])
	}

	// Test multiple valid packages
	pkg.AddPhpPackagesFromData([]byte(`[["package_b", "1.2.3",{}]]`))
	pkg.AddPhpPackagesFromData([]byte(`[["package_c", "1.2.3",{}]]`))
	pkg.Filter(app.PhpPackages)
	if !comparePkgs(&expect_b, &pkg.filteredPkgs[1]) || !comparePkgs(&expect_c, &pkg.filteredPkgs[2]) || len(pkg.filteredPkgs) != 3 {
		t.Fatalf("Expected '%+v', '%+v', got '%+v', '%+v'", expect_b, expect_c, pkg.filteredPkgs[1], pkg.filteredPkgs[2])
	}

	// Test duplicate package data in same harvest
	pkg.AddPhpPackagesFromData([]byte(`[["package_a", "1.2.3",{}]]`))
	pkg.Filter(app.PhpPackages)
	if len(pkg.filteredPkgs) != 3 {
		t.Fatalf("Expected len == 3, got '%+v'", len(pkg.filteredPkgs))
	}

	// Test package reset after harvest
	pkg = NewPhpPackages()
	pkg.Filter(app.PhpPackages)
	if pkg.filteredPkgs != nil {
		t.Fatalf("Expected nil, got '%+v'", pkg.filteredPkgs)
	}

	// Test duplicate package data not sent after harvest
	pkg.AddPhpPackagesFromData([]byte(`[["package_a", "1.2.3",{}]]`))
	pkg.Filter(app.PhpPackages)
	if pkg.filteredPkgs != nil {
		t.Fatalf("Expected nil, got '%+v'", pkg.filteredPkgs)
	}
}
