//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"math/rand"
	"net"
	"net/http"
	"os"
	"runtime"
	"runtime/pprof"
	"strconv"
	"strings"
	"sync"
	"text/tabwriter"
	"time"

	flatbuffers "github.com/google/flatbuffers/go"

	"newrelic.com/daemon/flatbuffersdata"
	"newrelic.com/daemon/newrelic"

	"newrelic.com/daemon/newrelic/infinite_tracing/proto_testdata"

	"newrelic.com/daemon/newrelic/collector"
	"newrelic.com/daemon/newrelic/protocol"
	"newrelic.com/daemon/newrelic/ratelimit"
	"newrelic.com/daemon/newrelic/sysinfo"
)

const helpMessage = `Usage: stressor [OPTIONS]

Run a stress test.

OPTIONS
  --port=PORT            Daemon port (TCP) or sock file (UDS)
  --lifespan=DURATION    Test duration [default: 200s]
  --rpm=N                Target transactions per minute [default: 6000]
  --concurrency=N        Maximum concurrent transactions [default: auto]
  --spans-per-txn=N      Spans sent with each transaction [default: 0]
  --spans-batch-size=N   Number of spans in one batch [default: 0]
  --logfile=FILE         Log file location [default: stdout]
  --datadir=DIR          Transaction data sample directory
                         [default: src/newrelic/sample_data]
  --cpuprofile=FILE      Write a CPU profile to the specified file before
                         exiting.
  --daemon-pprof=PORT    Capture additional performance data from the daemon's
                         pprof profiling interface. [default: no]
  --agent-hostname       Set the name to be used as the hostname of the
			 reporting agent. [default: local hostname]
  --trace-observer-host  Trace observer endpoint
  --trace-observer-port  Trace observer port

DESCRIPTION
  The stressor is used to test the daemon in isolation (i.e. without
  an agent) under high load. This can be used to empirically determine
  its limits and behavior at the margins. When combined with profiling,
  it can also be used to analyze and improve the behavior of the daemon.

  When the level of concurrency is unspecified (or set less than or equal
  to zero), the stressor will dynamically vary the level of concurrency
  in order to sustain the target requests per minute.

  The stressor considers the environment variables NEW_RELIC_LICENSE_KEY and
  NEW_RELIC_HOST.

EXAMPLES
  stressor --rpm 300 --lifespan 30s
     Simulate 300 transactions per minute for 30 seconds.

  stressor --rpm 300 --lifespan 30s --concurrency 25
     Simulate 300 transactions per minute for 30 seconds with a maximum of
     25 concurrent transactions.

  stressor --rpm 0
     Simulate an unlimited number of transactions per minute.
`

const (
	DefaultApps     = 1
	DefaultLifespan = 200 * time.Second
	DefaultRPM      = 6000
)

var (
	flagPort              = flag.String("port", newrelic.DefaultListenSocket(), "")
	flagApps              = flag.Int("applications", DefaultApps, "")
	flagLifespan          = flag.Duration("lifespan", DefaultLifespan, "")
	flagRPM               = flag.Int("rpm", DefaultRPM, "")
	flagConcurrency       = flag.Int("concurrency", 0, "")
	flagSpans             = flag.Int("spans-per-txn", 0, "")
	flagSpansBatch        = flag.Int("spans-batch-size", 0, "")
	flagDataDir           = flag.String("datadir", "src/newrelic/sample_data", "")
	flagLogFile           = flag.String("logfile", "stdout", "")
	flagCPUProfile        = flag.String("cpuprofile", "", "")
	flagDaemonPprof       = flag.Int("daemon-pprof", 0, "")
	flagHostname          = flag.String("agent-hostname", "", "")
	flagTraceObserverHost = flag.String("trace-observer-host", "", "")
	flagTraceObserverPort = flag.Int("trace-observer-port", 0, "")
)

// EstimatedRTT is an estimate of the typical roundtrip time to
// communicate with the daemon. It is used to calculate the required
// level of concurrency for the target requests per minute.
const EstimatedRTT = 100 * time.Millisecond

// DaemonSampleRate is the sampling frequency of performance data from
// the daemon. This number should be chosen so that sampling does not
// always occur on the same interval as periodic work within the daemon.
// e.g. The harvest.
const DaemonSampleRate = 3 * time.Second

// Application Behavior Constants
// In order to roughly mimic the PHP agent, these should settings match
//  NR_APP_UNKNOWN_QUERY_BACKOFF_LIMIT_SECONDS
//  NR_APP_REFRESH_QUERY_PERIOD_SECONDS
const (
	connectedAppInfoPeriod   = 20 * time.Second
	unconnectedAppInfoPeriod = 5 * time.Second
	appInfoReadDeadline      = 50 * time.Millisecond
)

type StressorApp struct {
	appInfoQuery []byte // cached app info query
	id           *newrelic.AgentRunID
	lastAppInfo  time.Time
}

func (app *StressorApp) Populate(index int) error {
	info := flatbuffersdata.SampleAppInfo

	info.Appname = fmt.Sprintf("%s %d", info.Appname, index)

	if *flagHostname != "" {
		info.Hostname = *flagHostname
	} else {
		info.Hostname, _ = sysinfo.Hostname()
	}

	if userLicense := os.Getenv("NEW_RELIC_LICENSE_KEY"); userLicense != "" {
		info.License = collector.LicenseKey(userLicense)
	}
	if userCollector := os.Getenv("NEW_RELIC_HOST"); userCollector != "" {
		info.RedirectCollector = userCollector
	}

	if *flagTraceObserverHost != "" && *flagTraceObserverPort > 0 {
		info.TraceObserverHost = *flagTraceObserverHost
		info.TraceObserverPort = uint16(*flagTraceObserverPort)
	}

	qry, err := flatbuffersdata.MarshalAppInfo(&info)
	if err != nil {
		return err
	}
	app.appInfoQuery = qry
	return err
}

func fatal(e error) {
	fmt.Fprintln(os.Stderr, e)
	os.Exit(1)
}

func getFileContents(filename string) []byte {
	b, err := ioutil.ReadFile(filename)
	if nil != err {
		fatal(err)
	}
	return b
}

func readMessage(r io.Reader) ([]byte, error) {
	b := make([]byte, 8)
	if _, err := io.ReadFull(r, b); err != nil {
		if err == io.EOF {
			return nil, err
		}
		return nil, fmt.Errorf("unable to read preamble: %v", err)
	}

	length := binary.LittleEndian.Uint32(b[0:4])

	body := make([]byte, length)
	if _, err := io.ReadFull(r, body); err != nil {
		return nil, fmt.Errorf("unable to read full message: %v", err)
	}
	return body, nil
}

func writeMessage(conn net.Conn, cmd string, runID *newrelic.AgentRunID, data []byte) error {
	return errors.New("not implemented")
}

func doAppInfo(conn net.Conn, app *StressorApp) error {
	mw := newrelic.MessageWriter{W: conn, Type: newrelic.MessageTypeBinary}
	_, err := mw.Write(app.appInfoQuery)
	if err != nil {
		return err
	}

	data, err := readMessage(conn)
	if err != nil {
		return err
	}

	msg := protocol.GetRootAsMessage(data, 0)
	if msg.DataType() != protocol.MessageBodyAppReply {
		return errors.New("unexpected reply type")
	}

	var tbl flatbuffers.Table
	var reply protocol.AppReply

	if !msg.Data(&tbl) {
		return errors.New("reply missing body")
	}

	reply.Init(tbl.Bytes, tbl.Pos)

	switch reply.Status() {
	case protocol.AppStatusUnknown:
		return nil
	case protocol.AppStatusStillValid:
		return nil
	case protocol.AppStatusInvalidLicense:
		return nil
	case protocol.AppStatusDisconnected:
		return nil
	case protocol.AppStatusConnected:
		// fall through
	}

	var r struct {
		ID *newrelic.AgentRunID `json:"agent_run_id"`
	}

	if err := json.Unmarshal(reply.ConnectReply(), &r); err != nil {
		return fmt.Errorf("unable to parse agent run id from appinfo reply: %v", err)
	}
	app.id = r.ID

	return nil
}

func shouldDoAppInfo(now time.Time, app *StressorApp) bool {
	var period time.Duration

	if nil == app.id {
		period = unconnectedAppInfoPeriod
	} else {
		period = connectedAppInfoPeriod
	}

	if now.Sub(app.lastAppInfo) >= period {
		return true
	}
	return false
}

type Stats struct {
	// Number of logical transactions simulated, where a logical transaction
	// represents a simulation of a single web request monitored by the agent.
	NumTxn int

	NumMsg   int // Number of messages sent to the daemon.
	NumSpans int // Number of spans sent to the daemon.
	Errors   int // Total errors
	Timeouts int // Total timeout errors
}

func (s *Stats) Aggregate(t *Stats) {
	s.NumTxn += t.NumTxn
	s.NumMsg += t.NumMsg
	s.NumSpans += t.NumSpans
	s.Errors += t.Errors
	s.Timeouts += t.Timeouts
}

func hammer(bucket *ratelimit.Bucket, stopChan <-chan struct{}, txn flatbuffersdata.Txn) (*Stats, error) {
	stats := &Stats{}

	conn, err := newrelic.OpenClientConnection(*flagPort)
	if nil != err {
		return stats, err
	}
	defer conn.Close()

	rng := rand.New(rand.NewSource(time.Now().UnixNano()))

	// Build applications.
	apps := make([]StressorApp, *flagApps)
	for i := range apps {
		if err := apps[i].Populate(i); err != nil {
			return stats, err
		}
	}

	var txnMsg []byte

	mw := newrelic.MessageWriter{W: conn, Type: newrelic.MessageTypeBinary}
	for {
		select {
		case <-stopChan:
			return stats, nil
		default:
		}

		if bucket != nil {
			bucket.Take()
		}

		app := &apps[rng.Intn(len(apps))] // Pick a random application.
		now := time.Now()

		if shouldDoAppInfo(now, app) {
			stats.NumMsg++
			app.lastAppInfo = now
			if err := doAppInfo(conn, app); err != nil {
				return stats, err
			}

			if app.id != nil && txn.RunID != string(*app.id) {
				txn.RunID = string(*app.id)
				msg, err := txn.MarshalBinary()
				if err != nil {
					return stats, err
				}
				txnMsg = msg
			}
		}

		if nil != app.id {
			stats.NumTxn++
			stats.NumMsg++
			_, err := mw.Write(txnMsg)
			if err != nil {
				return stats, err
			}

			// Send spans in batches
			for numSpans := *flagSpans; numSpans > 0; numSpans -= *flagSpansBatch {
				batchSize := *flagSpansBatch

				if batchSize > numSpans {
					batchSize = numSpans
				}

				// Protobuf encoded span batch
				protoSpanBatch, err := proto_testdata.MarshalSpanBatch(uint(batchSize))
				if err != nil {
					return stats, err
				}

				// flatbuffer encoded span batch
				spanMsg, err := txn.MarshalSpanBatchBinary(batchSize, protoSpanBatch)
				if err != nil {
					return stats, err
				} else {
					_, err := mw.Write(spanMsg)
					if err != nil {
						return stats, err
					} else {
						stats.NumSpans += batchSize
						stats.NumMsg += 1
					}
				}
			}
		}
	}
}

// setLogName sets the output destination for the logger. It is analogous to
// the log.SetOutput function, except it accepts a file name instead of a
// writer.
func setLogName(name string) error {
	switch {
	case name == "":
		log.SetOutput(ioutil.Discard)
	case strings.EqualFold(name, "stdout"):
		log.SetOutput(os.Stdout)
	case strings.EqualFold(name, "stderr"):
		log.SetOutput(os.Stderr)
	default:
		f, err := os.Create(name)
		if err != nil {
			return err
		}
		log.SetOutput(f)
	}
	return nil
}

var iecUnits = []string{"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"}

func formatBytes(n uint64) string {
	var i int
	var m uint64

	for n > 1024 {
		m = n % 1024
		n /= 1024
		i++
	}

	x := float64(n) + float64(m)/1024
	return strconv.FormatFloat(x, 'f', 2, 64) + iecUnits[i]
}

func formatNumber(n uint64) string {
	if n == 0 {
		return "0"
	}

	s := fmt.Sprintf("%03d", n%1000)
	n /= 1000

	for n > 0 {
		s = fmt.Sprintf("%03d,", n%1000) + s
		n /= 1000
	}
	return strings.TrimLeft(s, "0")
}

func getDaemonStats(url string, v interface{}) error {
	resp, err := http.Get(url)
	if err != nil {
		return err
	}

	js, err := ioutil.ReadAll(resp.Body)
	resp.Body.Close()

	if err != nil {
		return err
	}
	return json.Unmarshal(js, &v)
}

func sampleDaemonStats(url string, tick <-chan time.Time, stopChan <-chan struct{}) (*MemStats, error) {
	var initial, tmp struct{ MemStats *runtime.MemStats }
	var maxAlloc uint64

	// Save initial values, so we can compute the delta on exit.
	if err := getDaemonStats(url, &initial); err != nil {
		return nil, err
	}

	pauses := &GCFreq{}
	pauses.Reset(initial.MemStats)

	for {
		select {
		case <-tick:
			if err := getDaemonStats(url, &tmp); err != nil {
				return nil, err
			}
			pauses.Record(tmp.MemStats)
			if tmp.MemStats.Alloc > maxAlloc {
				maxAlloc = tmp.MemStats.Alloc
			}
		case <-stopChan:
			if err := getDaemonStats(url, &tmp); err != nil {
				return nil, err
			}

			pauses.Record(tmp.MemStats)
			if tmp.MemStats.Alloc > maxAlloc {
				maxAlloc = tmp.MemStats.Alloc
			}

			t0, t1 := initial.MemStats, tmp.MemStats
			stats := &MemStats{
				NumAlloc: t1.Mallocs - t0.Mallocs,
				SumAlloc: t1.TotalAlloc - t0.TotalAlloc,
				MaxAlloc: maxAlloc,
				NumGC:    uint64(t1.NumGC - t0.NumGC),
				SumGC:    time.Duration(t1.PauseTotalNs - t0.PauseTotalNs),
				Pauses:   pauses,
			}
			return stats, nil
		}
	}
}

func aggregate(stats <-chan *Stats) *Stats {
	var r Stats
	for x := range stats {
		r.Aggregate(x)
	}
	return &r
}

func pprofURL(port int) string {
	return "http://127.0.0.1:" + strconv.Itoa(port) + "/debug/vars"
}

func reportDaemonStats(stats *MemStats, numMsg uint64, lifespan time.Duration) {
	buf := bytes.Buffer{}
	w := tabwriter.NewWriter(&buf, 0, 8, 1, ' ', 0)

	fmt.Fprint(w, "Daemon Memory Usage\f")
	fmt.Fprint(w, "==========================\f")

	if stats == nil {
		fmt.Fprintln(w, "max(inuse):\tN/A")
		fmt.Fprintln(w, "count(allocs):\tN/A")
		fmt.Fprintln(w, "sum(allocs):\tN/A")
		fmt.Fprintln(w, "avg(allocs):\tN/A")
		fmt.Fprintln(w, "count(GC):\tN/A")
		fmt.Fprintln(w, "max(GC):\tN/A")
		fmt.Fprintln(w, "avg(GC):\tN/A")
	} else {
		fmt.Fprintf(w, "max(inuse):\t%s (%s bytes)\n",
			formatBytes(stats.MaxAlloc), formatNumber(stats.MaxAlloc))
		fmt.Fprint(w, "count(allocs):\t", formatNumber(stats.NumAlloc), "\n")
		fmt.Fprintf(w, "sum(allocs):\t%s (%s bytes)\n",
			formatBytes(stats.SumAlloc), formatNumber(stats.SumAlloc))

		fmt.Fprint(w, "avg(allocs):\t")
		if lifespan > 0 {
			fmt.Fprint(w, formatBytes(stats.SumAlloc/uint64(lifespan.Seconds())), "/sec")
		}
		if numMsg > 0 {
			fmt.Fprint(w, " ", formatBytes(stats.SumAlloc/numMsg), "/msg")
		}
		fmt.Fprintln(w)

		fmt.Fprint(w, "\f")
		fmt.Fprint(w, "Daemon GC Summary\f")
		fmt.Fprint(w, "====================\f")
		fmt.Fprint(w, "count(GC):\t", formatNumber(stats.NumGC), "\n")
		fmt.Fprint(w, "max(GC):\t", stats.MaxGC(), "\n")

		fmt.Fprint(w, "avg(GC):\t")
		if stats.NumGC > 0 {
			fmt.Fprint(w, stats.SumGC/time.Duration(stats.NumGC))
			if lifespan > 0 {
				fmt.Fprintf(w, " %d/sec", stats.NumGC/uint64(lifespan.Seconds()))
			}
		} else {
			fmt.Fprint(w, "N/A")
		}
		fmt.Fprint(w, "\n")

		if stats.Pauses != nil {
			fmt.Fprint(w, "\f")
			fmt.Fprint(w, "Daemon GC Pauses\f")
			fmt.Fprint(w, "==========================\f")

			count := float64(stats.Pauses.Count())
			stats.Pauses.Do(func(d time.Duration, n uint32) {
				percent := 100 * (float64(n) / count)
				fmt.Fprintf(w, "%v - %v:\t%6d (%5.2f%%)\n", d, d+time.Millisecond, n, percent)
			})
		}
	}

	w.Flush()

	for buf.Len() > 0 {
		line, _ := buf.ReadString('\n')
		log.Print(line)
	}
}

func main() {
	flag.Usage = func() { fmt.Fprint(os.Stderr, helpMessage, '\n') }
	flag.Parse()

	if *flagCPUProfile != "" {
		f, err := os.Create(*flagCPUProfile)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		pprof.StartCPUProfile(f)
		defer pprof.StopCPUProfile()
	}

	if err := setLogName(*flagLogFile); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	var limiter *ratelimit.Bucket

	if *flagRPM > 0 {
		log.Print("RPM = ", *flagRPM)
		limiter = ratelimit.ConstantRPM(*flagRPM)
	} else {
		log.Print("RPM = unlimited")
	}

	nworkers := *flagConcurrency
	if nworkers <= 0 {
		// Estimate the number of concurrent connections required based on
		// the expected roundtrip time for each transaction. Currently this
		// is a conservative, but wild guess.
		nworkers = *flagRPM/int(time.Minute/EstimatedRTT) + 1
	}

	log.Print("maximum concurrent transactions = ", nworkers)

	stopChan := make(chan struct{})           // close to signal workers to stop
	statChan := make(chan *Stats, nworkers)   // accumulates worker stats
	daemonStatChan := make(chan *MemStats, 1) // accumulates daemon stats
	wg := sync.WaitGroup{}

	if *flagDaemonPprof != 0 {
		wg.Add(1)
		go func() {
			ticker := time.NewTicker(DaemonSampleRate)
			defer func() {
				ticker.Stop()
				wg.Done()
			}()

			url := pprofURL(*flagDaemonPprof)
			stats, err := sampleDaemonStats(url, ticker.C, stopChan)
			daemonStatChan <- stats
			if err != nil {
				log.Print(err)
			}
		}()
	}

	for i := 0; i < nworkers; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()

			totals := &Stats{}
			for {
				results, err := hammer(limiter, stopChan, flatbuffersdata.SampleTxn)
				if results != nil {
					totals.Aggregate(results)
				}

				if err != nil {
					log.Print(err)
					totals.Errors++

					if strings.Contains(err.Error(), "timeout") {
						totals.Timeouts++
					}
				} else {
					break
				}
			}
			statChan <- totals
		}()
	}

	time.AfterFunc(*flagLifespan, func() { close(stopChan) })
	wg.Wait()

	close(statChan) // so we can range over its contents
	stats := aggregate(statChan)

	log.Print(" ")
	log.Print("Results")
	log.Print("====================")
	log.Printf("transactions: %s (%.2f/min)", formatNumber(uint64(stats.NumTxn)),
		60*float64(stats.NumTxn)/flagLifespan.Seconds())
	log.Printf("messages:     %s (%.2f/sec)", formatNumber(uint64(stats.NumMsg)),
		float64(stats.NumMsg)/flagLifespan.Seconds())
	log.Printf("spans:        %s (%.2f/sec)", formatNumber(uint64(stats.NumSpans)),
		float64(stats.NumSpans)/flagLifespan.Seconds())
	log.Print("errors:       ", formatNumber(uint64(stats.Errors)))
	log.Print("timeouts:     ", formatNumber(uint64(stats.Timeouts)))

	log.Print(" ")
	close(daemonStatChan) // ensure following read does not block
	reportDaemonStats(<-daemonStatChan, uint64(stats.NumMsg), *flagLifespan)
}
