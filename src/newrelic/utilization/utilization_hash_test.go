//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package utilization

import (
	"bytes"
	"encoding/json"
	"testing"

	"newrelic/crossagent"
)

func TestJSONMarshalling(t *testing.T) {
	ramInitializer := new(uint64)
	*ramInitializer = 1024
	actualProcessors := 4
	configProcessors := 16
	u := Data{
		MetadataVersion:   metadataVersion,
		LogicalProcessors: &actualProcessors,
		RamMiB:            ramInitializer,
		Hostname:          "localhost",
		Vendors: &vendors{
			AWS: &aws{
				InstanceID:       "8BADFOOD",
				InstanceType:     "t2.micro",
				AvailabilityZone: "us-west-1",
			},
			Docker: &docker{ID: "47cbd16b77c50cbf71401"},
			Kubernetes: &kubernetes{KubernetesServiceHost: "10.96.0.1"},
		},
		Config: &override{
			LogicalProcessors: &configProcessors,
		},
	}

	expect := `{
	"metadata_version": 5,
	"logical_processors": 4,
	"total_ram_mib": 1024,
	"hostname": "localhost",
	"config": {
		"logical_processors": 16
	},
	"vendors": {
		"aws": {
			"instanceId": "8BADFOOD",
			"instanceType": "t2.micro",
			"availabilityZone": "us-west-1"
		},
		"docker": {
			"id": "47cbd16b77c50cbf71401"
		},
		"kubernetes": {
			"kubernetes_service_host": "10.96.0.1"
		}
	}
}`

	j, err := json.MarshalIndent(u, "", "\t")
	if err != nil {
		t.Error(err)
	}
	if string(j) != expect {
		t.Errorf("strings don't match; \nexpected: %s\n  actual: %s\n", expect, string(j))
	}

	// Test that we marshal not-present values to nil.
	u.RamMiB = nil
	u.Hostname = ""
	u.Config = nil
	expect = `{
	"metadata_version": 5,
	"logical_processors": 4,
	"total_ram_mib": null,
	"hostname": "",
	"vendors": {
		"aws": {
			"instanceId": "8BADFOOD",
			"instanceType": "t2.micro",
			"availabilityZone": "us-west-1"
		},
		"docker": {
			"id": "47cbd16b77c50cbf71401"
		},
		"kubernetes": {
			"kubernetes_service_host": "10.96.0.1"
		}
	}
}`

	j, err = json.MarshalIndent(u, "", "\t")
	if err != nil {
		t.Error(err)
	}
	if string(j) != expect {
		t.Errorf("strings don't match; \nexpected: %s\n  actual: %s\n", expect, string(j))
	}

}

// Smoke test the Gather method and JSON marshalling.
func TestUtilizationHash(t *testing.T) {
	configs := []Config{
		{
			DetectAWS:    true,
			DetectAzure:  true,
			DetectGCP:    true,
			DetectPCF:    true,
			DetectDocker: true,
			DetectKubernetes: true,
		},
		{
			DetectAWS:    false,
			DetectAzure:  false,
			DetectGCP:    false,
			DetectPCF:    false,
			DetectDocker: false,
			DetectKubernetes: false,
		},
	}
	for _, c := range configs {
		u := Gather(c)

		if u == nil {
			t.Fatal("Utilization should not return nil if enabled.")
		}

		j, err := json.MarshalIndent(u, "", "\t")
		if err != nil {
			t.Errorf("Marshalling failed and shouldn't: %s", err)
		}
		if u.MetadataVersion == 0 || nil == u.LogicalProcessors ||
			0 == *u.LogicalProcessors || u.RamMiB == nil || *u.RamMiB == 0 {
			t.Errorf("Emptiness in utilization hash: %s", j)
		}

		js, err := json.Marshal(u)
		if err != nil {
			t.Errorf("Marshalling failed and shouldn't: %s", err)
		}
		js2, err := json.Marshal(u)
		if err != nil {
			t.Errorf("Marshalling failed and shouldn't: %s", err)
		}
		if !bytes.Equal(js, js2) {
			t.Errorf("JSON doesn't match json.marshal.\n\nActual: %s\n\nExpected: %s\n\n", js, js2)
		}

		b, err := json.Marshal(Gather(c))
		if err != nil || b == nil || len(b) == 0 {
			t.Error(err, b)
		}
		b, err = json.MarshalIndent(Gather(c), "", "\t")
		if err != nil || b == nil || len(b) == 0 {
			t.Error(err, b)
		}
	}
}

func TestOverrideFromConfig(t *testing.T) {
	testcases := []struct {
		config Config
		expect string
	}{
		{Config{}, `null`},
		{Config{LogicalProcessors: 16}, `{"logical_processors":16}`},
		{Config{TotalRamMIB: 1024}, `{"total_ram_mib":1024}`},
		{Config{BillingHostname: "localhost"}, `{"hostname":"localhost"}`},
		{Config{
			LogicalProcessors: 16,
			TotalRamMIB:       1024,
			BillingHostname:   "localhost",
		}, `{"logical_processors":16,"total_ram_mib":1024,"hostname":"localhost"}`},
	}

	for _, tc := range testcases {
		ov := overrideFromConfig(tc.config)
		js, err := json.Marshal(ov)
		if nil != err {
			t.Error(tc.expect, err)
			continue
		}
		if string(js) != tc.expect {
			t.Error(tc.expect, string(js))
		}
	}
}

type utilizationCrossAgentTestcase struct {
	Name              string          `json:"testname"`
	RAMMIB            *uint64         `json:"input_total_ram_mib"`
	LogicalProcessors *int            `json:"input_logical_processors"`
	Hostname          string          `json:"input_hostname"`
	FullHostname      string          `json:"input_full_hostname"`
	Addresses         []string        `json:"input_ip_address"`
	BootID            string          `json:"input_boot_id"`
	AWSID             string          `json:"input_aws_id"`
	AWSType           string          `json:"input_aws_type"`
	AWSZone           string          `json:"input_aws_zone"`
	AzureLocation     string          `json:"input_azure_location"`
	AzureName         string          `json:"input_azure_name"`
	AzureID           string          `json:"input_azure_id"`
	AzureSize         string          `json:"input_azure_size"`
	GCPID             json.Number     `json:"input_gcp_id"`
	GCPType           string          `json:"input_gcp_type"`
	GCPName           string          `json:"input_gcp_name"`
	GCPZone           string          `json:"input_gcp_zone"`
	PCFGUID           string          `json:"input_pcf_guid"`
	PCFIP             string          `json:"input_pcf_ip"`
	PCFMemLimit       string          `json:"input_pcf_mem_limit"`
	ExpectedOutput    json.RawMessage `json:"expected_output_json"`
	Config            struct {
		LogicalProcessors json.RawMessage `json:"NEW_RELIC_UTILIZATION_LOGICAL_PROCESSORS"`
		RAWMMIB           json.RawMessage `json:"NEW_RELIC_UTILIZATION_TOTAL_RAM_MIB"`
		Hostname          string          `json:"NEW_RELIC_UTILIZATION_BILLING_HOSTNAME"`
		KubernetesHost    string          `json:"KUBERNETES_SERVICE_HOST"`
	} `json:"input_environment_variables"`
}

func crossAgentVendors(tc utilizationCrossAgentTestcase) *vendors {
	v := &vendors{}

	if tc.AWSID != "" && tc.AWSType != "" && tc.AWSZone != "" {
		v.AWS = &aws{
			InstanceID:       tc.AWSID,
			InstanceType:     tc.AWSType,
			AvailabilityZone: tc.AWSZone,
		}
		v.AWS.validate()
	}

	if tc.AzureLocation != "" && tc.AzureName != "" && tc.AzureID != "" && tc.AzureSize != "" {
		v.Azure = &azure{
			Location: tc.AzureLocation,
			Name:     tc.AzureName,
			VMID:     tc.AzureID,
			VMSize:   tc.AzureSize,
		}
		v.Azure.validate()
	}

	if tc.GCPID.String() != "" && tc.GCPType != "" && tc.GCPName != "" && tc.GCPZone != "" {
		v.GCP = &gcp{
			ID:          numericString(tc.GCPID.String()),
			MachineType: tc.GCPType,
			Name:        tc.GCPName,
			Zone:        tc.GCPZone,
		}
		v.GCP.validate()
	}

	if tc.PCFIP != "" && tc.PCFGUID != "" && tc.PCFMemLimit != "" {
		v.PCF = &pcf{
			InstanceGUID: tc.PCFGUID,
			InstanceIP:   tc.PCFIP,
			MemoryLimit:  tc.PCFMemLimit,
		}
		v.PCF.validate()
	}

	GatherKubernetes(v, func(key string) string {
		if key == "KUBERNETES_SERVICE_HOST" {
			return tc.Config.KubernetesHost
		}

		return ""
	})

	if v.isEmpty() {
		return nil
	}
	return v
}

func compactJSON(js []byte) []byte {
	buf := new(bytes.Buffer)
	if err := json.Compact(buf, js); err != nil {
		return nil
	}
	return buf.Bytes()
}

func runUtilizationCrossAgentTestcase(t *testing.T, tc utilizationCrossAgentTestcase) {

	// Skip utilitzation cross agent tests for metadata versions newer than
	// the supported one (which is version 3).
	type version struct {
		MetadataVersion uint64 `json:metadata_version`
	}

	var v version

	err := json.Unmarshal(tc.ExpectedOutput, &v)

	//if err == nil && v.MetadataVersion != 3 {
	//	t.Skip("Unsupported utilization metdata version: ", v.MetadataVersion)
	//}

	var ConfigRAWMMIB int
	if nil != tc.Config.RAWMMIB {
		json.Unmarshal(tc.Config.RAWMMIB, &ConfigRAWMMIB)
	}
	var ConfigLogicalProcessors int
	if nil != tc.Config.LogicalProcessors {
		json.Unmarshal(tc.Config.LogicalProcessors, &ConfigLogicalProcessors)
	}

	cfg := Config{
		LogicalProcessors: ConfigLogicalProcessors,
		TotalRamMIB:       ConfigRAWMMIB,
		BillingHostname:   tc.Config.Hostname,
	}

	data := &Data{
		MetadataVersion:   metadataVersion,
		LogicalProcessors: tc.LogicalProcessors,
		RamMiB:            tc.RAMMIB,
		Hostname:          tc.Hostname,
		BootID:            tc.BootID,
		Vendors:           crossAgentVendors(tc),
		Config:            overrideFromConfig(cfg),
		FullHostname:      tc.FullHostname,
		Addresses:         tc.Addresses,
	}

	js, err := json.Marshal(data)
	if nil != err {
		t.Error(tc.Name, err)
	}

	expect := string(compactJSON(tc.ExpectedOutput))
	if string(js) != expect {
		t.Error(tc.Name, string(js), expect)
	}
}

func TestUtilizationCrossAgent(t *testing.T) {
	var tcs []utilizationCrossAgentTestcase

	input, err := crossagent.ReadFile(`utilization/utilization_json.json`)
	if nil != err {
		t.Fatal(err)
	}

	err = json.Unmarshal(input, &tcs)
	if nil != err {
		t.Fatal(err)
	}
	for _, tc := range tcs {
		runUtilizationCrossAgentTestcase(t, tc)
	}
}

func TestVendorsIsEmpty(t *testing.T) {
	v := &vendors{}

	if !v.isEmpty() {
		t.Fatal("default vendors does not register as empty")
	}

	v.AWS = &aws{}
	if v.isEmpty() {
		t.Fatal("non-empty vendors registers as empty")
	}

	var nilVendors *vendors
	if !nilVendors.isEmpty() {
		t.Fatal("nil vendors should be empty")
	}
}
