//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

// Package utilization implements the newrelic-internal Utilization spec

package utilization

import (
	"fmt"
	"os"
	"runtime"
	"sync"

	"newrelic/log"
	"newrelic/sysinfo"
)

const (
	metadataVersion = 5
)

type Config struct {
	DetectAWS         bool
	DetectAzure       bool
	DetectGCP         bool
	DetectPCF         bool
	DetectDocker      bool
	DetectKubernetes  bool
	LogicalProcessors int
	TotalRamMIB       int
	BillingHostname   string
}

type override struct {
	LogicalProcessors *int   `json:"logical_processors,omitempty"`
	TotalRamMIB       *int   `json:"total_ram_mib,omitempty"`
	BillingHostname   string `json:"hostname,omitempty"`
}

type Data struct {
	MetadataVersion int `json:"metadata_version"`
	// Although `runtime.NumCPU()` will never fail, this field is a pointer
	// to facilitate the cross agent tests.
	LogicalProcessors *int      `json:"logical_processors"`
	RamMiB            *uint64   `json:"total_ram_mib"`
	Hostname          string    `json:"hostname"`
	FullHostname      string    `json:"full_hostname,omitempty"`
	Addresses         []string  `json:"ip_address,omitempty"`
	BootID            string    `json:"boot_id,omitempty"`
	Config            *override `json:"config,omitempty"`
	Vendors           *vendors  `json:"vendors,omitempty"`
}

type docker struct {
	ID string `json:"id",omitempty`
}

type vendors struct {
	AWS        *aws        `json:"aws,omitempty"`
	Azure      *azure      `json:"azure,omitempty"`
	GCP        *gcp        `json:"gcp,omitempty"`
	PCF        *pcf        `json:"pcf,omitempty"`
	Docker     *docker     `json:"docker,omitempty"`
	Kubernetes *kubernetes `json:"kubernetes,omitempty"`
}

func (v *vendors) isEmpty() bool {
	return nil == v || *v == vendors{}
}

func overrideFromConfig(config Config) *override {
	ov := &override{}

	if 0 != config.LogicalProcessors {
		x := config.LogicalProcessors
		ov.LogicalProcessors = &x
	}
	if 0 != config.TotalRamMIB {
		x := config.TotalRamMIB
		ov.TotalRamMIB = &x
	}
	ov.BillingHostname = config.BillingHostname

	if "" == ov.BillingHostname &&
		nil == ov.LogicalProcessors &&
		nil == ov.TotalRamMIB {
		ov = nil
	}
	return ov
}

func Gather(config Config) *Data {
	var wg sync.WaitGroup

	uDat := &Data{
		MetadataVersion: metadataVersion,
		Vendors:         &vendors{},
	}

	// This anonymous function allows us to run each gather function in a separate goroutine
	// and wait for them at the end by closing over the wg WaitGroup we
	// instantiated at the start of the function.
	goGather := func(gather func(util *Data) error, util *Data) {
		wg.Add(1)
		go func() {
			defer wg.Done()
			if err := gather(util); err != nil {
				log.Debugf("%s", err)
			}
		}()
	}

	// System things we gather no matter what.
	goGather(GatherBootID, uDat)
	goGather(GatherCPU, uDat)
	goGather(GatherMemory, uDat)

	// Gather IPs before spawning goroutines since the IPs are used in
	// gathering full hostname.
	if ips, err := utilizationIPs(); nil == err {
		uDat.Addresses = ips
	} else {
		log.Debugf("Error gathering addresses: %s", err)
	}

	// Now things the user can turn off.
	if config.DetectDocker {
		goGather(GatherDockerID, uDat)
	}

	if config.DetectAWS {
		goGather(GatherAWS, uDat)
	}

	if config.DetectAzure {
		goGather(GatherAzure, uDat)
	}

	if config.DetectGCP {
		goGather(GatherGCP, uDat)
	}

	if config.DetectPCF {
		goGather(GatherPCF, uDat)
	}

	wg.Add(1)
	go func() {
		defer wg.Done()
		uDat.FullHostname = getFQDN(uDat.Addresses)
	}()

	if config.DetectKubernetes {
		if err_k8s := GatherKubernetes(uDat.Vendors, os.Getenv); err_k8s != nil {
			log.Debugf("%s", err_k8s)
		}
	}

	// Now we wait for everything!
	wg.Wait()

	// Override whatever needs to be overridden.
	uDat.Config = overrideFromConfig(config)

	return uDat
}

func GatherBootID(util *Data) error {
	id, err := sysinfo.BootID()
	if err != nil {
		if err != sysinfo.ErrFeatureUnsupported {
			return fmt.Errorf("Invalid boot ID detected: %s", err)
		}
	} else {
		util.BootID = id
	}

	return nil
}

func GatherCPU(util *Data) error {
	cpu := runtime.NumCPU()
	util.LogicalProcessors = &cpu
	return nil
}

func GatherDockerID(util *Data) error {
	id, err := sysinfo.DockerID()
	if err != nil {
		if err != sysinfo.ErrFeatureUnsupported {
			return fmt.Errorf("Did not detect Docker on this platform: %s", err)
		}
	} else {
		util.Vendors.Docker = &docker{ID: id}
	}

	return nil
}

func OverrideDockerId(util *Data, id string) {
	if nil == util.Vendors {
		util.Vendors = &vendors{}
		util.Vendors.Docker = &docker{}
	}
	util.Vendors.Docker = &docker{ID: id}		
}

func OverrideVendors(util *Data) {
	if util.Vendors.isEmpty() {
		// Per spec, we MUST NOT send any vendors hash if it's empty.
		util.Vendors = nil
	}
}

func GetDockerId(util *Data) (string, error) {
    id := ""
    if nil == util {
        return id, fmt.Errorf("Util is nil")
    }
    if util.Vendors.isEmpty() {
        return id, fmt.Errorf("Vendors structure is empty")
    }
    if nil == util.Vendors.Docker {
        return id, fmt.Errorf("Docker structure is empty")
    }

    id = util.Vendors.Docker.ID

    return id, nil
}

func GatherMemory(util *Data) error {
	ram, err := sysinfo.PhysicalMemoryBytes()
	if nil == err {
		ram = ram / (1024 * 1024) // bytes -> MiB
		util.RamMiB = &ram
	} else {
		return fmt.Errorf("Could not find host memory: %s", err)
	}

	return nil
}
