//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package utilization

import (
	"fmt"
)

type kubernetes struct {
	KubernetesServiceHost string `json:"kubernetes_service_host",omitempty`	

	// Having a custom getter allows the unit tests to mock os.Getenv().
	environmentVariableGetter func(key string) string
}

func GatherKubernetes(v *vendors, getenv func(string) string) error {
	k8s := newKubernetes(getenv)
	if err := k8s.Gather(); err != nil {
		return fmt.Errorf("Kubernetes not detected: %s", err)
	} else {
		if k8s.KubernetesServiceHost != "" {
			v.Kubernetes = k8s
		}
	}

	return nil
}

func newKubernetes(getenv func(string) string) *kubernetes {
	return &kubernetes{
		environmentVariableGetter: getenv,
	}
}

func (k8s *kubernetes) Gather() error {
	k8s.KubernetesServiceHost = k8s.environmentVariableGetter("KUBERNETES_SERVICE_HOST")

	if err := k8s.validate(); err != nil {
		return err
	}

	return nil
}

func (k8s *kubernetes) validate() (err error) {
	k8s.KubernetesServiceHost, err = normalizeValue(k8s.KubernetesServiceHost)
	if err != nil {
		return fmt.Errorf("Invalid Kubernetes Service Host: %v", err)
	}

	return
}
