//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
    "crypto/tls"
    "encoding/json"
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

type CollectibleFunc func(auditVersion bool) ([]byte, error)

func (fn CollectibleFunc) CollectorJSON(auditVersion bool) ([]byte, error) {
    return fn(auditVersion)
}

type Collectible interface {
    CollectorJSON(auditVersion bool) ([]byte, error)
}

// RpmCmd contains fields specific to an individual call made to RPM.
type RpmCmd struct {
    Name              string
    Collector         string
    RunID             string
    Data              []byte
    License       LicenseKey
	RequestHeadersMap map[string]string
}

// RpmControls contains fields which will be the same for all calls made
// by the same application.
type RpmControls struct {
    Collectible   Collectible
    AgentLanguage string
    AgentVersion  string
    ua            string
}

// RPMResponse contains a NR endpoint response.
//
// Agent Behavior Summary:
//
// on connect/preconnect:
//     410 means shutdown
//     200, 202 mean success (start run)
//     all other response codes and errors mean try after backoff
//
// on harvest:
//     410 means shutdown
//     401, 409 mean restart run
//     408, 429, 500, 503 mean save data for next harvest
//     all other response codes and errors discard the data and continue the current harvest
type RPMResponse struct {
    StatusCode int
    Body       []byte
    // Err indicates whether or not the call was successful: newRPMResponse
    // should be used to avoid mismatch between StatusCode and Err.
    Err                      error
    disconnectSecurityPolicy bool
}

func newRPMResponse(StatusCode int) RPMResponse {
    var err error
    if StatusCode != 200 && StatusCode != 202 {
        err = fmt.Errorf("response code: %d: %s", StatusCode, GetStatusCodeMessage(StatusCode))
    }
    return RPMResponse{StatusCode: StatusCode, Err: err}
}
// IsDisconnect indicates that the agent should disconnect.
func (resp RPMResponse) IsDisconnect() bool {
    return resp.StatusCode == 410 || resp.disconnectSecurityPolicy
}
// IsRestartException indicates that the agent should restart.
func (resp RPMResponse) IsRestartException() bool {
    return resp.StatusCode == 401 || resp.StatusCode == 409
}
// This is in place because, to update the license ini value, the PHP app must be shut off
func (resp RPMResponse) IsInvalidLicense() bool {
    return resp.StatusCode == 401
}
// ShouldSaveHarvestData indicates that the agent should save the data and try
// to send it in the next harvest.
func (resp RPMResponse) ShouldSaveHarvestData() bool {
    switch resp.StatusCode {
    case 408, 429, 500, 503:
        return true
    default:
        return false
    }
}

// Not a method of RPMResponse so that it can be called during creation
func GetStatusCodeMessage(StatusCode int) string {
    switch StatusCode {
        case 400: return "Invalid request formatting"
        case 401: return "Authentication failure"
        case 403: return "Forbidden"
        case 404: return "Not found"
        case 405: return "HTTP method not found"
        case 407: return "Proxy authentication failure (misconfigured)"
        case 408: return "Timeout"
        case 409: return "Conflict: you should reconnect"
        case 410: return "Gone: you should disconnect"
        case 411: return "Content-length required"
        case 413: return "Payload too large"
        case 414: return "URI too large"
        case 415: return "Content-type or content-encoding is wrong"
        case 417: return "Expectation failed"
        case 429: return "Too many requests"
        case 431: return "Request headers too large"
        case 500: return "NR server internal error"
        case 503: return "NR service unavailable"
        default: return "Unknown response code"
    }
}

// The agent languages we give the collector are not necessarily the ideal
// choices for a user agent string.
var userAgentMappings = map[string]string{
    "c":   "C",
    "sdk": "C",
    "php": "PHP",
    "":    "Native",
}

func (control *RpmControls) userAgent() string {
    if control.ua == "" {
        lang := control.AgentLanguage
        if val, ok := userAgentMappings[control.AgentLanguage]; ok {
            lang = val
        }

        ver := "unknown"
        if control.AgentVersion != "" {
            ver = control.AgentVersion
        }

        control.ua = fmt.Sprintf("NewRelic-%s-Agent/%s NewRelic-GoDaemon/%s", lang,
                                 ver, version.Number)
    }

    return control.ua
}

type Client interface {
    Execute(cmd RpmCmd, cs RpmControls) RPMResponse
}

type ClientFn func(cmd RpmCmd, cs RpmControls) RPMResponse

func (fn ClientFn) Execute(cmd RpmCmd, cs RpmControls) RPMResponse {
    return fn(cmd, cs)
}

type limitClient struct {
    timeout   time.Duration
    orig      Client
    semaphore chan bool
}

func (l *limitClient) Execute(cmd RpmCmd, cs RpmControls) RPMResponse {
    var timer <-chan time.Time

    if 0 != l.timeout {
        timer = time.After(l.timeout)
    }

    select {
        case <-l.semaphore:
            defer func() { l.semaphore <- true }()
            resp := l.orig.Execute(cmd, cs)
            return resp
        case <-timer:
            return RPMResponse{Err: fmt.Errorf("timeout after %v", l.timeout)}
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

func (c *clientImpl) perform(url string, cmd RpmCmd, cs RpmControls) RPMResponse {
    deflated, err := Compress(cmd.Data)
    if nil != err {
        return RPMResponse{Err: err}
    }

    req, err := http.NewRequest("POST", url, deflated)
    if nil != err {
        return RPMResponse{Err: err}
    }

    req.Header.Add("Accept-Encoding", "identity, deflate")
    req.Header.Add("Content-Type", "application/octet-stream")
    req.Header.Add("User-Agent", cs.userAgent())
    req.Header.Add("Content-Encoding", "deflate")

	for k, v := range cmd.RequestHeadersMap {
		req.Header.Add(k, v)
	}

    resp, err := c.httpClient.Do(req)
    if err != nil {
        return RPMResponse{Err: err}
    }

    defer resp.Body.Close()

    r := newRPMResponse(resp.StatusCode)

    body, err := ioutil.ReadAll(resp.Body)
    if nil != err {
        r.Err = err
    } else {
        r.Body, r.Err = parseResponse(body)
    }
    return r
}
type rpmException struct {
	Message   string `json:"message"`
	ErrorType string `json:"error_type"`
}

func (e *rpmException) Error() string {
	return fmt.Sprintf("%s: %s", e.ErrorType, e.Message)
}
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

func (c *clientImpl) Execute(cmd RpmCmd, cs RpmControls) RPMResponse {
    data, err := cs.Collectible.CollectorJSON(false)
    if nil != err {
        return RPMResponse{Err: err}
    }
    cmd.Data = data

    var audit []byte

    if log.Auditing() {
        audit, err = cs.Collectible.CollectorJSON(true)
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

    resp := c.perform(url, cmd, cs)
    if err != nil {
        log.Debugf("attempt to perform %s failed: %q, url=%s",
                   cmd.Name, err.Error(), cleanURL)
    }

    log.Audit("command='%s' url='%s', response={%s}", cmd.Name, url, string(resp.Body))
    log.Debugf("command='%s' url='%s', response={%s}", cmd.Name, cleanURL, string(resp.Body))
    return resp
}
