//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"sync"
	"time"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/collector"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/integration"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/log"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/secrets"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/utilization"
)

var (
	DefaultMaxCustomEvents = 30000
)

var (
	flagAgent           = flag.String("agent", "", "")
	flagCGI             = flag.String("cgi", "", "")
	flagCollector       = flag.String("collector", "", "the collector host")
	flagLoglevel        = flag.String("loglevel", "", "agent log level")
	flagOutputDir       = flag.String("output-dir", ".", "")
	flagPattern         = flag.String("pattern", "test_*", "shell pattern describing tests to run")
	flagPHP             = flag.String("php", "", "")
	flagPort            = flag.String("port", defaultPort(), "")
	flagRetry           = flag.Int("retry", 0, "maximum retry attempts")
	flagTimeout         = flag.Duration("timeout", 10*time.Second, "")
	flagValgrind        = flag.String("valgrind", "", "if given, this is the path to valgrind")
	flagWorkers         = flag.Int("threads", 1, "")
	flagTime            = flag.Bool("time", false, "time each test")
	flagMaxCustomEvents = flag.Int("max_custom_events", 30000, "value for newrelic.custom_events.max_samples_stored")
	flagWarnIsFail      = flag.Bool("warnisfail", false, "warn result is treated as a fail")
	flagOpcacheOff      = flag.Bool("opcacheoff", false, "run without opcache. Some tests are intended to fail when run this way")

	// externalPort is the port on which we start a server to handle
	// external calls.
	flagExternalPort = flag.Int("external_port", 0, "")

	// Allows an end user to change the hard coded license key the integration runner
	// uses.  Useful for running a set of integration tests that are separate from
	// the main suite.
	//
	// Supports an @license.txt format to allow reading the license in from a file.
	//
	// Expected format:
	//
	// abcdefghij1234567890abcdefghij1234567890
	//
	flagLicense FlagStringOrFile

	// Allows an end user to set a security policy token to use when connecting.  In the
	// real world daemon, this value comes from the application's php.ini configuration.
	// However, since the integration runner handles agent/daemon communication in its
	// own specialized way, we need to allow integration runner users to set this value
	//
	// Supports a @security-token.txt format to allow reading the token in from
	// a file.
	//
	// Expected format:
	//
	// ffff-fffb-ffff-ffff
	//
	flagSecurityToken FlagStringOrFile

	// Allows an end user to pass in a set of supported security policies to use when
	// connecting.  In the real world daemon, this value comes from values hard coded in
	// the agent source code.  However, since the integration runner handles agent/daemon
	// communication in its own specialized way, we need to allow integration runner users
	// to set this value.
	//
	// Flag supports an @file-name.json format to allow reading supported policies in
	// from a file.
	//
	// Expected format:
	//
	// {
	//    "record_sql": {
	//        "enabled": false,
	//        "supported": true
	//    },
	//    "allow_raw_exception_messages": {
	//        "enabled": false,
	//        "supported": true
	//    },
	//    "custom_events": {
	//        "enabled": false,
	//        "supported": true
	//    },
	//    "custom_parameters": {
	//        "enabled": false,
	//        "supported": true
	//    }
	// }
	flagSecuityPolicies FlagStringOrFile

	// Header names for headers that are ignored when conducting response
	// header checks.
	ignoreResponseHeaders = map[string]bool{"Content-Type": true, "X-Powered-By": true}

	// Global storage for response headers that are sent by the CAT
	// endpoint during a test run.
	responseHeaders http.Header
	// Lock for protecting against concurrent writes of response headers
	responseHeadersLock sync.Mutex
)

// Default directories to search for tests.
var defaultArgs = []string{
	"tests/integration",
	"tests/library",
	"tests/regression",
}

// PHP Test Account 1
var (
	TestApp = newrelic.AppInfo{
		Appname:           "Agent Integration Tests",
		RedirectCollector: "collector.newrelic.com",
		AgentVersion:      "0",
		AgentLanguage:     "php",
		HighSecurity:      false,
		Environment:       nil,
		Labels:            nil,
		Metadata:          nil,
		Settings:
		// Ensure that we get Javascript agent code in the reply
		map[string]interface{}{"newrelic.browser_monitoring.debug": false, "newrelic.browser_monitoring.loader": "rum"},
		SecurityPolicyToken: "",
		// Set log and customer event limits to non-zero values or else collector will return 0.
		// Use default value for logging.
		// Use max value for custom event so integration tests can exercise full range of values for max to record.
		AgentEventLimits: collector.EventConfigs{
			LogEventConfig: collector.Event{
				Limit: 10000,
			},
			CustomEventConfig: collector.Event{
				Limit: DefaultMaxCustomEvents,
			},
		},
	}

	// Integration tests have this mock cross process id hard coded into
	// metric assertions
	MockCrossProcessId = fmt.Sprintf("%s#%s", secrets.NewrelicAccountId, secrets.NewrelicAppId)
)

var ctx *integration.Context

func defaultPort() string {
	name := fmt.Sprintf("newrelic-daemon-%d.sock", os.Getpid())
	name = filepath.Join(os.TempDir(), name)
	if runtime.GOOS == "linux" {
		// Use an abstract domain socket.
		name = "@" + name
	}
	return name
}

func merge(a, b map[string]string) map[string]string {
	merged := make(map[string]string)
	for k, v := range a {
		merged[k] = v
	}
	for k, v := range b {
		merged[k] = v
	}
	return merged
}

func catRequest(w http.ResponseWriter, r *http.Request) {
	catFile := r.URL.Query().Get("file")
	dtEnabled := r.URL.Query().Get("dt_enabled")
	catEnabled := r.URL.Query().Get("cat_enabled")
	if "" == catFile {
		http.Error(w, "cat failure: no file provided", http.StatusBadRequest)
		return
	}

	env := merge(ctx.Env, nil)
	settings := merge(ctx.Settings, nil)
	settings["newrelic.appname"] = "ignore"
	if "false" == dtEnabled {
		settings["newrelic.distributed_tracing_enabled"] = "false"
	} else if "true" == dtEnabled {
		settings["newrelic.distributed_tracing_enabled"] = "true"
	} else {
		http.Error(w, "cat request: invalid value of dt_enabled - expected 'true' or 'false', got '"+dtEnabled+"'.", http.StatusBadRequest)
		return
	}

	if "false" == catEnabled {
		settings["newrelic.cross_application_tracer.enabled"] = "false"
	} else if "true" == catEnabled {
		settings["newrelic.cross_application_tracer.enabled"] = "true"
	} else {
		http.Error(w, "cat request: invalid value of cat_enabled - expected 'true' or 'false', got '"+catEnabled+"'.", http.StatusBadRequest)
		return
	}

	tx, err := integration.CgiTx(integration.ScriptFile(catFile), env, settings, r.Header, ctx)
	if nil != err {
		http.Error(w, "cat failure: "+err.Error(), http.StatusInternalServerError)
		return
	}

	headers, body, err := tx.Execute()
	if nil != err {
		http.Error(w, "cat failure: "+err.Error(), http.StatusInternalServerError)
		return
	}

	// Copy response headers
	h := w.Header()
	for key, vals := range headers {
		for _, val := range vals {
			h.Add(key, val)
			if true != ignoreResponseHeaders[key] && responseHeaders != nil {
				responseHeadersLock.Lock()
				responseHeaders.Add(key, val)
				responseHeadersLock.Unlock()
			}
		}
	}

	w.Write(body)
}

func delayRequest(w http.ResponseWriter, r *http.Request) {
	duration := r.URL.Query().Get("duration")
	io.WriteString(w, "waiting...")
	d, err := time.ParseDuration(duration)
	if nil != err {
		d = 0
	}
	time.Sleep(d)
}

func init() {
	//setup typed flags
	flag.Var(&flagLicense, "license", "use a license key other than the hard coded default. Supports @filename syntax for loading from files.")
	flag.Var(&flagSecurityToken, "security_token", "if given, the integration runner will connect with this security token. Supports @filename syntax for loading from files.")
	flag.Var(&flagSecuityPolicies, "supported_policies", "if given, the integration runner will connect with the provided supported policies. Supports @filename syntax for loading from files.")
}

func main() {
	// Respect GOMAXPROCS if set; otherwise, use all available CPUs.
	if os.Getenv("GOMAXPROCS") == "" {
		runtime.GOMAXPROCS(runtime.NumCPU())
	}

	flag.Parse()

	// The license key and collector can be set via make variables in make/secrets.mk
	TestApp.RedirectCollector = secrets.NewrelicCollectorHost
	if TestApp.RedirectCollector == "" {
		TestApp.RedirectCollector = "collector.newrelic.com"
	}
	TestApp.License = collector.LicenseKey(secrets.NewrelicLicenseKey)

	// Set value that will be sent to collector for the max custom event samples
	TestApp.AgentEventLimits.CustomEventConfig.Limit = *flagMaxCustomEvents

	// Set the redirect collector from the flag, if given.
	if *flagCollector != "" {
		TestApp.RedirectCollector = *flagCollector
	}

	if *flagPHP == "" {
		*flagPHP = "php"
	}

	if flagLicense != "" {
		TestApp.License = collector.LicenseKey(flagLicense)
	}

	if len(*flagCGI) == 0 {
		if len(*flagPHP) > 0 {
			*flagCGI = stringReplaceLast(*flagPHP, "php", "php-cgi", 1)
		} else {
			*flagCGI = "php-cgi"
		}
	}

	var err error
	*flagCGI, err = exec.LookPath(*flagCGI)
	if nil != err {
		fmt.Fprintf(os.Stderr, "WARNING: unable to find cgi: %v\n", err)
	}

	// Start server for external requests.
	mux := http.NewServeMux()
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		io.WriteString(w, "Hello world!")
	})
	mux.HandleFunc("/cat", catRequest)
	mux.HandleFunc("/delay", delayRequest)
	addr := "127.0.0.1:" + strconv.Itoa(*flagExternalPort)
	srv := &http.Server{Addr: addr, Handler: mux}
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		fmt.Fprintf(os.Stderr, "unable start external server: %v\n", err)
		os.Exit(1)
	}
	externalHost := ln.Addr().String()
	go func() {
		err := srv.Serve(ln)
		if nil != err {
			fmt.Fprintf(os.Stderr, "unable serve external server: %v\n", err)
			os.Exit(1)
		}
	}()

	if len(*flagPort) == 0 {
		*flagPort = defaultPort()
	}

	*flagOutputDir, _ = filepath.Abs(*flagOutputDir)
	daemonLog := filepath.Join(*flagOutputDir, "integration-tests.log")
	agentLog := filepath.Join(*flagOutputDir, "php_agent.log")
	os.Remove(daemonLog)
	os.Remove(agentLog)

	log.Init(log.LogDebug, daemonLog)

	ctx = integration.NewContext(*flagPHP, *flagCGI)
	ctx.Valgrind = *flagValgrind
	ctx.Timeout = *flagTimeout

	// Settings common to all tests.
	ctx.Settings = make(map[string]string)
	ctx.Settings["newrelic.license"] = string(TestApp.License)
	ctx.Settings["newrelic.logfile"] = agentLog
	ctx.Settings["newrelic.daemon.port"] = `"` + *flagPort + `"`
	ctx.Settings["newrelic.daemon.dont_launch"] = "3"
	ctx.Settings["newrelic.special"] = "debug_cat"

	if *flagLoglevel != "" {
		ctx.Settings["newrelic.loglevel"] = *flagLoglevel
	}

	if false == *flagOpcacheOff {
		// PHP Modules common to all tests
		ctx.Settings["zend_extension"] = "opcache.so"

		// PHP INI values common to all tests
		// These settings can be overwritten by adding new values to the INI block
		ctx.Settings["opcache.enable"] = "1"
		ctx.Settings["opcache.enable_cli"] = "1"
	}

	// If the user provided a custom agent extension, use it.
	if len(*flagAgent) > 0 {
		ctx.Settings["extension"], _ = filepath.Abs(*flagAgent)
	}

	// Env vars common to all tests.
	ctx.Env["EXTERNAL_HOST"] = externalHost

	handler, err := startDaemon("unix", *flagPort, flagSecurityToken.String(), flagSecuityPolicies.String())
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	args := flag.Args()
	if 0 == len(args) {
		args = defaultArgs
	}

	testFiles := discoverTests(*flagPattern, args)
	tests := make([]*integration.Test, 0, len(testFiles))
	testsToRun := make(chan *integration.Test, len(testFiles))
	for _, filename := range testFiles {
		if test := integration.ParseTestFile(filename); test != nil {
			tests = append(tests, test)
			testsToRun <- test
		}
	}

	runTests(testsToRun, *flagWorkers)

	// Now wait for all data to be flushed, then delete the sock file.
	time.Sleep(50 * time.Millisecond)

	for i := 0; i < *flagRetry; i++ {
		testsToRetry := make(chan *integration.Test, len(testFiles))

		handler.Lock()
		for _, tc := range tests {
			if !tc.Failed && !tc.Skipped && !tc.Warned {
				if handler.harvests[tc.Name] == nil {
					testsToRetry <- tc
				}
			}
		}
		handler.Unlock()

		if len(testsToRetry) == 0 {
			break
		}

		retryTests(testsToRetry, *flagWorkers)
		time.Sleep(50 * time.Millisecond)
	}

	deleteSockfile("unix", *flagPort)

	var numFailed int
	var numWarned int

	// Compare the output
	handler.Lock()
	for _, tc := range tests {
		if !tc.Failed && !tc.Skipped && !tc.Warned {
			tc.Compare(handler.harvests[tc.Name])
		}
		if tc.Failed && tc.Xfail == "" {
			numFailed++
		}
		if tc.Warned {
			numWarned++
		}
	}

	tapOutput(tests)

	if numFailed > 0 {
		os.Exit(1)
	}
	if *flagWarnIsFail && numWarned > 0 {
		os.Exit(2)
	}
}

var (
	skipRE  = regexp.MustCompile(`^(?i)\s*skip`)
	xfailRE = regexp.MustCompile(`^(?i)\s*xfail`)
	warnRE  = regexp.MustCompile(`^warn:\s+`)
)

func runTests(testsToRun chan *integration.Test, numWorkers int) {
	var wg sync.WaitGroup

	for i := 0; i < numWorkers; i++ {
		wg.Add(1)

		go func() {
			defer wg.Done()
			for {
				select {
				case tc := <-testsToRun:
					fmt.Println("running", tc.Name)
					// Note that runTest will modify test
					// fields.  These will be visible to the
					// main goroutine because of the
					// wg.Done() call.
					runTest(tc)
				default:
					return
				}
			}
		}()
	}

	wg.Wait()
}

func retryTests(testsToRun chan *integration.Test, numWorkers int) {
	var wg sync.WaitGroup

	for i := 0; i < numWorkers; i++ {
		wg.Add(1)

		go func() {
			defer wg.Done()
			for {
				select {
				case tc := <-testsToRun:
					fmt.Println("retrying", tc.Name)
					// Note that runTest will modify test
					// fields.  These will be visible to the
					// main goroutine because of the
					// wg.Done() call.
					tc.Reset()
					runTest(tc)
				default:
					return
				}
			}
		}()
	}

	wg.Wait()
}

func runTest(t *integration.Test) {
	if nil != t.Err {
		return
	}

	skipIf, _ := t.MakeSkipIf(ctx)
	if skipIf != nil {
		_, body, err := skipIf.Execute()

		if err != nil {
			t.Output = body
			t.Fatal(fmt.Errorf("error executing skipif: %v %v", err, skipIf))
			return
		}

		if skipRE.Match(body) {
			reason := string(bytes.TrimSpace(head(body)))
			t.Skip(reason)
			return
		}
	}

	// Reset global response headers before the test is run. This feature
	// only works for sequential test runs.
	responseHeaders = make(http.Header)

	run, _ := t.MakeRun(ctx)

	start := time.Now()
	_, body, err := run.Execute()

	// Set the duration on the test if --time was given. A zero duration
	// will not be printed.
	if *flagTime {
		t.Duration = time.Since(start)
	} else {
		t.Duration = 0
	}

	// Always save the test output. If an error occurred it may contain
	// critical information regarding the cause. Currently, it may also
	// contain valgrind commentary which we want to display.
	t.Output = body

	if *flagWorkers == 1 && t.ShouldCheckResponseHeaders() {
		// Response header test active.
		t.ResponseHeaders = responseHeaders
	} else if t.ShouldCheckResponseHeaders() {
		// Response headers expected but disabled because of parallel
		// test runs.
		fmt.Println("SKIPPING response header test for ", t.Name)
	}

	if err != nil {
		if _, ok := err.(*exec.ExitError); !ok {
			t.Fatal(fmt.Errorf("error executing script: %v", err))
			return
		}
	}

	if skipRE.Match(body) {
		reason := string(bytes.TrimSpace(head(body)))
		t.Skip(reason)
		return
	}

	if warnRE.Match(body) {
		reason := string(bytes.TrimSpace(head(body)))
		t.Warn(reason)
		return
	}

	if xfailRE.Match(body) {
		// Strip xfail message from body so it does not affect expectations.
		tmp := bytes.SplitN(body, []byte("\n"), 2)
		t.XFail(string(bytes.TrimSpace(tmp[0])))
		if len(tmp) == 2 {
			body = tmp[1]
		}
	}
}

// head returns the first line of s.
func head(s []byte) []byte {
	if i := bytes.IndexByte(s, '\n'); i >= 0 {
		return s[:i+1]
	}
	return s
}

// discoverTests recursively searches the paths in searchPaths and
// returns the paths of each file that matches pattern.
func discoverTests(pattern string, searchPaths []string) []string {
	testFiles := make([]string, 0, 100)

	for _, root := range searchPaths {
		if info, err := os.Stat(root); err == nil && info.Mode().IsRegular() {
			testFiles = append(testFiles, root)
			continue
		}

		filepath.Walk(root, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return nil
			}

			if info.Mode().IsRegular() {
				if ok, _ := filepath.Match(pattern, info.Name()); ok {
					testFiles = append(testFiles, path)
				}
			}

			return nil
		})
	}

	return testFiles
}

func injectIntoConnectReply(reply collector.RPMResponse, newRunID, crossProcessId string) []byte {
	var x map[string]interface{}

	json.Unmarshal(reply.Body, &x)

	x["agent_run_id"] = newRunID
	x["cross_process_id"] = crossProcessId

	out, _ := json.Marshal(x)
	return out
}

type IntegrationDataHandler struct {
	sync.Mutex                                       // Protects harvests
	harvests            map[string]*newrelic.Harvest // Keyed by tc.Name (which is used as AgentRunID)
	reply               collector.RPMResponse        // Constant after creation
	rawSecurityPolicies []byte                       // policies from connection attempt, needed for AppInfo reply
}

func (h *IntegrationDataHandler) IncomingTxnData(id newrelic.AgentRunID, sample newrelic.AggregaterInto) {
	h.Lock()
	defer h.Unlock()

	harvest := h.harvests[string(id)]
	if nil == harvest {
		harvest = newrelic.NewHarvest(time.Now(), collector.NewHarvestLimits(nil))
		// Save a little memory by reducing the event pools.
		harvest.TxnEvents = newrelic.NewTxnEvents(50)
		harvest.CustomEvents = newrelic.NewCustomEvents(50)
		h.harvests[string(id)] = harvest
	}

	sample.AggregateInto(harvest)
}

func (h *IntegrationDataHandler) IncomingSpanBatch(batch newrelic.SpanBatch) {}

func (h *IntegrationDataHandler) IncomingAppInfo(id *newrelic.AgentRunID, info *newrelic.AppInfo) newrelic.AppInfoReply {
	return newrelic.AppInfoReply{
		State: newrelic.AppStateConnected,
		// Use the appname (which has been set to the filename) as
		// the agent run id to enable running the tests in
		// parallel.
		ConnectReply:     injectIntoConnectReply(h.reply, info.Appname, MockCrossProcessId),
		SecurityPolicies: h.rawSecurityPolicies,
		ConnectTimestamp: uint64(time.Now().Unix()),
		HarvestFrequency: 60,
		SamplingTarget:   10,
	}
}

func deleteSockfile(network, address string) {
	if network == "unix" && !(address[0] == '@') {
		err := os.Remove(address)
		if err != nil && !os.IsNotExist(err) {
			fmt.Fprintf(os.Stderr, "unable to remove stale sock file: %v"+
				" - another daemon may already be running?\n", err)
		}
	}
}

// startDaemon bootstraps the daemon components required to run the
// tests. There are two types of messages an agent can send that affect
// the integration tests: appinfo queries, and txndata. Normally, these
// would be handled by newrelic.Process. We do not do so here, instead
// the test runner intercepts these messages for inspection. This has
// the side-effect of disabling the harvest.
//
// Note: with a little refactoring in the daemon we could continue to
// stub out appinfo queries and inspect txndata while preserving the
// harvest.
func startDaemon(network, address string, securityToken string, securityPolicies string) (*IntegrationDataHandler, error) {
	// Gathering utilization data during integration tests.
	client, _ := newrelic.NewClient(&newrelic.ClientConfig{})
	connectPayload := TestApp.ConnectPayload(utilization.Gather(
		utilization.Config{
			DetectAWS:        true,
			DetectAzure:      true,
			DetectGCP:        true,
			DetectPCF:        true,
			DetectDocker:     true,
			DetectKubernetes: true,
		}))

	policies := newrelic.AgentPolicies{}
	json.Unmarshal([]byte(securityPolicies), &policies.Policies)

	connectAttempt := newrelic.ConnectApplication(&newrelic.ConnectArgs{
		RedirectCollector:            TestApp.RedirectCollector,
		PayloadRaw:                   connectPayload,
		License:                      TestApp.License,
		Client:                       client,
		SecurityPolicyToken:          securityToken,
		AppSupportedSecurityPolicies: policies,
	})

	if nil != connectAttempt.Err {
		return nil, fmt.Errorf("unable to connect application: %v", connectAttempt.Err)
	}

	handler := &IntegrationDataHandler{
		reply:               connectAttempt.RawReply,
		harvests:            make(map[string]*newrelic.Harvest),
		rawSecurityPolicies: connectAttempt.RawSecurityPolicies,
	}

	go func() {
		deleteSockfile(network, address) // in case there's a stale one hanging around.

		list, err := newrelic.Listen(network, address)
		if err != nil {
			deleteSockfile(network, address)
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}

		defer list.Close()

		if err = list.Serve(newrelic.CommandsHandler{Processor: handler}); err != nil {
			deleteSockfile(network, address)
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
	}()

	// Grace period for the listener to come up.
	time.Sleep(50 * time.Millisecond)

	return handler, nil
}
