//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"strings"
	"time"

	"newrelic/collector"
	"newrelic/limits"
	"newrelic/log"
	"newrelic/sysinfo"
	"newrelic/utilization"
)

// AgentRunID is a string as of agent listener protocol version 14.
type AgentRunID string

func (id AgentRunID) String() string {
	return string(id)
}

type AppState int

const (
	AppStateUnknown AppState = iota
	AppStateConnected
	AppStateDisconnected
	AppStateRestart
	AppStateInvalidLicense
	AppStateInvalidSecurityPolicies
)

// An AppKey uniquely identifies an application.
type AppKey struct {
	License           collector.LicenseKey
	Appname           string
	RedirectCollector string
	HighSecurity      bool
	AgentLanguage     string
	AgentPolicies     string
	AgentHostname     string
	TraceObserverHost string
	TraceObserverPort uint16
}

// AppInfo encapsulates information provided by an agent about an
// application. The information is used to construct part of the connect
// message sent to the collector, and the fields should not be modified.
type AppInfo struct {
	License                   collector.LicenseKey
	Appname                   string
	AgentLanguage             string
	AgentVersion              string
	HostDisplayName           string
	Settings                  map[string]interface{}
	Environment               JSONString
	HighSecurity              bool
	Labels                    JSONString
	Metadata                  JSONString
	RedirectCollector         string
	SecurityPolicyToken       string
	SupportedSecurityPolicies AgentPolicies
	Hostname                  string
	TraceObserverHost         string
	TraceObserverPort         uint16
	SpanQueueSize             uint64
	AgentEventLimits          collector.EventConfigs
}

func (info *AppInfo) String() string {
	return info.Appname
}

// Structure of the security policies used on Preconnect and Connect
type SecurityPolicy struct {
	Enabled  bool `json:"enabled"`
	Required bool `json:"required,omitempty"`
}

type RawPreconnectPayload struct {
	SecurityPolicyToken string `json:"security_policies_token,omitempty"`
	HighSecurity        bool   `json:"high_security"`
}

type RawConnectPayload struct {
	Pid                int                          `json:"pid"`
	Language           string                       `json:"language"`
	Version            string                       `json:"agent_version"`
	Host               string                       `json:"host"`
	HostDisplayName    string                       `json:"display_host,omitempty"`
	Settings           map[string]interface{}       `json:"settings"`
	AppName            []string                     `json:"app_name"`
	HighSecurity       bool                         `json:"high_security"`
	Labels             JSONString                   `json:"labels"`
	Environment        JSONString                   `json:"environment"`
	Metadata           JSONString                   `json:"metadata"`
	Identifier         string                       `json:"identifier"`
	Util               *utilization.Data            `json:"utilization,omitempty"`
	SecurityPolicies   map[string]SecurityPolicy    `json:"security_policies,omitempty"`
	EventHarvestConfig collector.EventHarvestConfig `json:"event_harvest_config"`
}

// PreconnectReply contains all of the fields from the app preconnect command reply
// that are used in the daemon.
type PreconnectReply struct {
	Collector        string                    `json:"redirect_host"`
	SecurityPolicies map[string]SecurityPolicy `json:"security_policies"`
}

// ConnectReply contains all of the fields from the app connect command reply
// that are used in the daemon.  The reply contains many more fields, but most
// of them are used in the agent.
type ConnectReply struct {
	ID                     *AgentRunID                      `json:"agent_run_id"`
	MetricRules            MetricRules                      `json:"metric_name_rules"`
	SamplingFrequency      int                              `json:"sampling_target_period_in_seconds"`
	SamplingTarget         int                              `json:"sampling_target"`
	EventHarvestConfig     collector.EventHarvestConfig     `json:"event_harvest_config"`
	SpanEventHarvestConfig collector.SpanEventHarvestConfig `json:"span_event_harvest_config"`
	RequestHeadersMap      map[string]string                `json:"request_headers_map"`
	MaxPayloadSizeInBytes  int                              `json:"max_payload_size_in_bytes"`
}

// An App represents the state of an application.
type App struct {
	state               AppState
	collector           string
	lastConnectAttempt  time.Time
	connectTime         time.Time
	harvestFrequency    time.Duration
	samplingTarget      uint16
	info                *AppInfo
	connectReply        *ConnectReply
	RawSecurityPolicies []byte
	RawConnectReply     []byte
	HarvestTrigger      HarvestTriggerFunc
	LastActivity        time.Time
	Rules               MetricRules
}

func (app *App) String() string {
	return app.info.String()
}

func (info *AppInfo) Key() AppKey {
	return AppKey{
		License:           info.License,
		Appname:           info.Appname,
		RedirectCollector: info.RedirectCollector,
		HighSecurity:      info.HighSecurity,
		AgentLanguage:     info.AgentLanguage,
		AgentPolicies:     info.SupportedSecurityPolicies.getSupportedPoliciesHash(),
		AgentHostname:     info.Hostname,
		TraceObserverHost: info.TraceObserverHost,
		TraceObserverPort: info.TraceObserverPort,
	}
}

func (app *App) Key() AppKey {
	return app.info.Key()
}

func NewApp(info *AppInfo) *App {
	now := time.Now()

	return &App{
		state:              AppStateUnknown,
		collector:          "",
		lastConnectAttempt: time.Time{},
		info:               info,
		HarvestTrigger:     nil,
		LastActivity:       now,
	}
}

func EncodePayload(payload interface{}) ([]byte, error) {
	buf := &bytes.Buffer{}
	buf.Grow(2048)
	buf.WriteByte('[')

	enc := json.NewEncoder(buf)
	if err := enc.Encode(&payload); err != nil {
		return nil, err
	}

	// json.Encoder always writes a trailing newline, replace it with the
	// closing bracket for the connect array.
	buf.Bytes()[buf.Len()-1] = ']'

	return buf.Bytes(), nil
}

func (info *AppInfo) ConnectPayloadInternal(pid int, util *utilization.Data) *RawConnectPayload {

	data := &RawConnectPayload{
		Pid:             pid,
		Language:        info.AgentLanguage,
		Version:         info.AgentVersion,
		Host:            info.Hostname,
		HostDisplayName: stringLengthByteLimit(info.HostDisplayName, limits.HostLengthByteLimit),
		Settings:        info.Settings,
		AppName:         strings.Split(info.Appname, ";"),
		HighSecurity:    info.HighSecurity,
		Environment:     info.Environment,
		// This identifier is used by the collector to look up the real agent. If an
		// identifier isn't provided, the collector will create its own based on the
		// first appname, which prevents a single daemon from connecting "a;b" and
		// "a;c" at the same time.
		//
		// Providing the identifier below works around this issue and allows users
		// more flexibility in using application rollups.
		Identifier:         info.Appname,
		EventHarvestConfig: collector.NewEventHarvestConfig(&info.AgentEventLimits),
	}

	// Fallback solution: if no host name was provided with the application
	// info, then the daemon host name is used.
	if data.Host == "" {
		hostname, err := sysinfo.Hostname()
		if err == nil {
			log.Debugf("Host name not specified in application info. Using daemon host name %s", hostname)

			data.Host = hostname
		} else {
			log.Errorf("Cannot determine host name: %s", err)
		}
	}

	// Utilization data is copied for each connect payload, as the host
	// name differs for applications connecting from different hosts.
	//
	// Per spec, the host name sent up in the connect payload MUST be the
	// same as the host name sent up in utilization.
	if util != nil {
		utilCopy := *util
		data.Util = &utilCopy
		data.Util.Hostname = data.Host
	}

	if len(info.Labels) > 0 {
		data.Labels = info.Labels
	} else {
		data.Labels = JSONString("[]")
	}
	if len(info.Metadata) > 0 {
		data.Metadata = info.Metadata
	} else {
		data.Metadata = JSONString("{}")
	}

	return data
}

// ConnectPayload creates the JSON of a connect request to be sent to the
// New Relic backend.
//
// Utilization is always expected to be present.
func (info *AppInfo) ConnectPayload(util *utilization.Data) *RawConnectPayload {
	return info.ConnectPayloadInternal(os.Getpid(), util)
}

func (info *AppInfo) initSettings(data []byte) {
	var dataDec interface{}

	err := json.Unmarshal(data, &dataDec)
	if err != nil {
		return
	}

	dataMap, ok := dataDec.(map[string]interface{})
	if ok {
		info.Settings = dataMap
	}
}

func (app *App) NeedsConnectAttempt(now time.Time, backoff time.Duration) bool {
	if app.state != AppStateUnknown {
		return false
	}
	if now.Sub(app.lastConnectAttempt) >= backoff {
		return true
	}
	return false
}

//Since span events are not included in Faster Event Harvest due to concerns
//about downsampling within a distributed trace, the report period and harvest
//limit are reported separately in span_event_harvest_config instead of
//event_harvest_config.  Combine them both into EventHarvestConfig here.
func combineEventConfig(ehc collector.EventHarvestConfig, sehc collector.SpanEventHarvestConfig) collector.EventHarvestConfig {
	ehc.EventConfigs.SpanEventConfig.Limit = sehc.SpanEventConfig.Limit
	ehc.EventConfigs.SpanEventConfig.ReportPeriod = sehc.SpanEventConfig.ReportPeriod
	return ehc
}

func parseConnectReply(rawConnectReply []byte) (*ConnectReply, error) {
	c := ConnectReply{MaxPayloadSizeInBytes: limits.DefaultMaxPayloadSizeInBytes}

	err := json.Unmarshal(rawConnectReply, &c)
	if nil != err {
		return nil, err
	}
	if nil == c.ID {
		return nil, errors.New("missing agent run id")
	}

	// Since the collector now sends seperately, we need to internally combine the limits.
	c.EventHarvestConfig = combineEventConfig(c.EventHarvestConfig, c.SpanEventHarvestConfig)

	return &c, nil
}

// Inactive determines whether the elapsed time since app last had activity
// exceeds a threshold.
func (app *App) Inactive(threshold time.Duration) bool {
	if threshold < 0 {
		panic(fmt.Errorf("invalid inactivity threshold: %v", threshold))
	}
	return time.Since(app.LastActivity) > threshold
}
