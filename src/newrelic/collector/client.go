//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
	"crypto/tls"
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"net"
	"net/http"
	"net/url"
	"strings"
	"time"

	"golang.org/x/net/proxy"

	"newrelic/log"
	"newrelic/version"
)

var ErrPayloadTooLarge = errors.New("payload too large")
var ErrUnauthorized = errors.New("unauthorized")
var ErrUnsupportedMedia = errors.New("unsupported media")

type CollectibleFunc func(auditVersion bool) ([]byte, error)

func (fn CollectibleFunc) CollectorJSON(auditVersion bool) ([]byte, error) {
	return fn(auditVersion)
}

type Collectible interface {
	CollectorJSON(auditVersion bool) ([]byte, error)
}

type Cmd struct {
	Name              string
	Collector         string
	License           LicenseKey
	RunID             string
	AgentLanguage     string
	AgentVersion      string
	Collectible       Collectible
	RequestHeadersMap map[string]string

	ua                string
}

// The agent languages we give the collector are not necessarily the ideal
// choices for a user agent string.
var userAgentMappings = map[string]string{
	"c":   "C",
	"sdk": "C",
	"php": "PHP",
	"":    "Native",
}

func (cmd *Cmd) userAgent() string {
	if cmd.ua == "" {
		lang := cmd.AgentLanguage
		if val, ok := userAgentMappings[cmd.AgentLanguage]; ok {
			lang = val
		}

		ver := "unknown"
		if cmd.AgentVersion != "" {
			ver = cmd.AgentVersion
		}

		cmd.ua = fmt.Sprintf("NewRelic-%s-Agent/%s NewRelic-GoDaemon/%s", lang,
			ver, version.Number)
	}

	return cmd.ua
}

type Client interface {
	Execute(cmd Cmd) ([]byte, error)
}

type ClientFn func(cmd Cmd) ([]byte, error)

func (fn ClientFn) Execute(cmd Cmd) ([]byte, error) {
	return fn(cmd)
}

type limitClient struct {
	timeout   time.Duration
	orig      Client
	semaphore chan bool
}

func (l *limitClient) Execute(cmd Cmd) ([]byte, error) {
	var timer <-chan time.Time

	if 0 != l.timeout {
		timer = time.After(l.timeout)
	}

	select {
	case <-l.semaphore:
		defer func() { l.semaphore <- true }()
		b, err := l.orig.Execute(cmd)
		return b, err
	case <-timer:
		return nil, fmt.Errorf("timeout after %v", l.timeout)
	}
}

func NewLimitClient(c Client, max int, timeout time.Duration) Client {
	l := &limitClient{
		orig:      c,
		semaphore: make(chan bool, max),
		timeout:   timeout,
	}
	// We need to use a prefilled semaphore and block on the receive rather
	// than block on the send, because of compiler ordering flexibility.
	//
	// See Channel Communication in the Go Memory Model for details.
	// https://golang.org/ref/mem#tmp_7
	for i := 0; i < max; i++ {
		l.semaphore <- true
	}
	return l
}

type ClientConfig struct {
	CAFile      string
	CAPath      string
	Proxy       string
	MaxParallel int
	Timeout     time.Duration
}

func NewClient(cfg *ClientConfig) (Client, error) {
	if nil == cfg {
		cfg = &ClientConfig{}
	}

	// Use defaults from the http package.
	dialer := &net.Dialer{
		Timeout:   30 * time.Second,
		KeepAlive: 30 * time.Second,
	}

	transport := &http.Transport{
		Proxy:               http.ProxyFromEnvironment,
		Dial:                dialer.Dial,
		TLSHandshakeTimeout: 10 * time.Second,
		TLSClientConfig:     &tls.Config{RootCAs: DefaultCertPool},
	}

	if "" != cfg.CAFile || "" != cfg.CAPath {
		pool, err := NewCertPool(cfg.CAFile, cfg.CAPath)
		if nil != err {
			return nil, err
		}
		transport.TLSClientConfig.RootCAs = pool
	}

	if "" != cfg.Proxy {
		url, err := parseProxy(cfg.Proxy)
		if err != nil {
			return nil, err
		}

		switch url.Scheme {
		case "http", "https":
			transport.Proxy = http.ProxyURL(url)
		default:
			// Other proxy schemes are assumed to use tunneling and require
			// a custom dialer to establish the tunnel.
			proxyDialer, err := proxy.FromURL(url, dialer)
			if err != nil {
				return nil, err
			}
			transport.Proxy = nil
			transport.Dial = proxyDialer.Dial
		}
	}

	c := &clientImpl{
		httpClient: &http.Client{
			Transport: transport,
			Timeout:   cfg.Timeout,
		},
	}

	if cfg.MaxParallel <= 0 {
		return c, nil
	}
	return NewLimitClient(c, cfg.MaxParallel, cfg.Timeout), nil
}

// parseProxy parses the URL for a proxy similar to url.Parse, but adds
// support for URLs that do not specify a scheme. Such URLs are assumed
// to be HTTP proxies. This mimics the behavior of libcurl.
func parseProxy(proxy string) (*url.URL, error) {
	url, err := url.Parse(proxy)
	if err != nil || !strings.Contains(proxy, "://") {
		// Try again, assuming HTTP as the scheme. If this fails, return the
		// original error so the error message matches the original URL.
		if httpURL, err := url.Parse("http://" + proxy); err == nil {
			return httpURL, nil
		}
	}
	return url, err
}

type clientImpl struct {
	httpClient *http.Client
}

func (c *clientImpl) perform(url string, data []byte, userAgent string, requestHeadersMap map[string]string) ([]byte, error) {
	deflated, err := Compress(data)
	if nil != err {
		return nil, err
	}

	req, err := http.NewRequest("POST", url, deflated)
	if nil != err {
		return nil, err
	}

	req.Header.Add("Accept-Encoding", "identity, deflate")
	req.Header.Add("Content-Type", "application/octet-stream")
	req.Header.Add("User-Agent", userAgent)
	req.Header.Add("Content-Encoding", "deflate")

	for k, v := range requestHeadersMap {
		req.Header.Add(k, v)
	}

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return nil, err
	}

	defer resp.Body.Close()

	switch resp.StatusCode {
	case 200:
		// Nothing to do.
	case 202:
		// "New" style P17 success response code with no JSON payload to decode
		// FIXME - this just gets something working but need to change
		//         all this code to not expect a reply payload on success 
		//         as was the case with P16
		//         Will be replaced by proper P17 response code handling
		return []byte(""), nil
	case 401:
		return nil, ErrUnauthorized
	case 413:
		return nil, ErrPayloadTooLarge
	case 415:
		return nil, ErrUnsupportedMedia
	default:
		// If the response code is not 200, then the collector may not return
		// valid JSON
		return nil, fmt.Errorf("unexpected collector HTTP status code: %d",
			resp.StatusCode)
	}

	b, err := ioutil.ReadAll(resp.Body)
	if nil != err {
		return nil, err
	}
	return parseResponse(b)
}

func (c *clientImpl) Execute(cmd Cmd) ([]byte, error) {
	data, err := cmd.Collectible.CollectorJSON(false)
	if nil != err {
		return nil, fmt.Errorf("unable to create json payload for '%s': %s",
			cmd.Name, err)
	}

	var audit []byte

	if log.Auditing() {
		audit, err = cmd.Collectible.CollectorJSON(true)
		if nil != err {
			log.Errorf("unable to create audit json payload for '%s': %s", cmd.Name, err)
			audit = data
		}
		if nil == audit {
			audit = data
		}
	}

	url := cmd.url(false)
	cleanURL := cmd.url(true)

	log.Audit("command='%s' url='%s' payload={%s}", cmd.Name, url, audit)
	log.Debugf("command='%s' url='%s' payload={%s}", cmd.Name, cleanURL, data)

	resp, err := c.perform(url, data, cmd.userAgent(), cmd.RequestHeadersMap)
	if err != nil {
		log.Debugf("attempt to perform %s failed: %q, url=%s",
			cmd.Name, err.Error(), cleanURL)
	}

	log.Audit("command='%s' url='%s', response={%s}", cmd.Name, url, resp)
	log.Debugf("command='%s' url='%s', response={%s}", cmd.Name, cleanURL, resp)

	return resp, err
}

type rpmException struct {
	Message   string `json:"message"`
	ErrorType string `json:"error_type"`
}

func (e *rpmException) Error() string {
	return fmt.Sprintf("%s: %s", e.ErrorType, e.Message)
}

func hasType(e error, expected string) bool {
	rpmErr, ok := e.(*rpmException)
	if !ok {
		return false
	}
	return rpmErr.ErrorType == expected

}

const (
	forceRestartType   = "NewRelic::Agent::ForceRestartException"
	disconnectType     = "NewRelic::Agent::ForceDisconnectException"
	licenseInvalidType = "NewRelic::Agent::LicenseException"
	runtimeType        = "RuntimeError"
)

// These clients exist for testing.
var (
	DisconnectClient = ClientFn(func(cmd Cmd) ([]byte, error) {
		return nil, SampleDisonnectException
	})
	LicenseInvalidClient = ClientFn(func(cmd Cmd) ([]byte, error) {
		return nil, SampleLicenseInvalidException
	})
	SampleRestartException        = &rpmException{ErrorType: forceRestartType}
	SampleDisonnectException      = &rpmException{ErrorType: disconnectType}
	SampleLicenseInvalidException = &rpmException{ErrorType: licenseInvalidType}
)

func IsRestartException(e error) bool { return hasType(e, forceRestartType) }
func IsLicenseException(e error) bool { return hasType(e, licenseInvalidType) }
func IsRuntime(e error) bool          { return hasType(e, runtimeType) }
func IsDisconnect(e error) bool       { return hasType(e, disconnectType) }

func parseResponse(b []byte) ([]byte, error) {
	var r struct {
		ReturnValue json.RawMessage `json:"return_value"`
		Exception   *rpmException   `json:"exception"`
	}

	err := json.Unmarshal(b, &r)
	if nil != err {
		return nil, err
	}

	if nil != r.Exception {
		return nil, r.Exception
	}

	return r.ReturnValue, nil
}
