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

func TestSetPhpPackages(t *testing.T) {
	// create nil pkgs for testing passing a nil receiver
	var nilpkg *PhpPackages
	validData := []byte("hello")

	err := nilpkg.SetPhpPackages(validData)
	if !strings.Contains(err.Error(), "packages is nil") {
		t.Fatalf("Expected error 'packages is nil!', got '%s'", err.Error())
	}

	// test with valid pkgs, invalid data
	pkg := NewPhpPackages()
	if nil == pkg {
		t.Fatal("Expected not nil")
	}
	err = pkg.SetPhpPackages(nil)
	if !strings.Contains(err.Error(), "data is nil") {
		t.Fatalf("Expected error 'data is nil!', got '%s'", err.Error())
	}

	//valid pkgs, valid data
	err = pkg.SetPhpPackages(validData)
	if nil != err {
		t.Fatalf("Expected nil error, got %s", err.Error())
	}
	if string(validData) != string(pkg.data) {
		t.Fatalf("Expected '%s', got '%s'", string(validData), string(pkg.data))
	}
}

func TestAddPhpPackagesFromData(t *testing.T) {
	// create nil pkgs for testing passing a nil receiver
	var nilpkg *PhpPackages
	validData := []byte("hello")

	err := nilpkg.AddPhpPackagesFromData(validData)
	if !strings.Contains(err.Error(), "packages is nil") {
		t.Fatalf("Expected error 'packages is nil!', got '%s'", err.Error())
	}

	// test with valid pkgs, invalid data
	pkg := NewPhpPackages()
	if nil == pkg {
		t.Fatal("Expected not nil")
	}
	err = pkg.AddPhpPackagesFromData(nil)
	if !strings.Contains(err.Error(), "data is nil") {
		t.Fatalf("Expected error 'data is nil!', got '%s'", err.Error())
	}

	//valid pkgs, valid data
	err = pkg.AddPhpPackagesFromData(validData)
	if nil != err {
		t.Fatalf("Expected nil error, got %s", err.Error())
	}
	if string(validData) != string(pkg.data) {
		t.Fatalf("Expected '%s', got '%s'", string(validData), string(pkg.data))
	}
}

func TestCollectorJSON(t *testing.T) {
	// create nil pkgs for testing passing a nil receiver
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

	pkg.SetPhpPackages([]byte(`["package", "1.2.3",{}]`))

	json, err = pkg.CollectorJSON(id)
	if nil != err {
		t.Fatalf("Expected nil error, got %s", err.Error())
	}
	expectedJSON = `["Jars",["package", "1.2.3",{}]]`
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
	validData := []byte("hello")
	err := pkg.SetPhpPackages(validData)
	if nil != err {
		t.Fatal("Expected not nil")
	}
	empty = pkg.Empty()
	if true == empty {
		t.Fatalf("Expected 'false' got '%t'", empty)
	}
}
