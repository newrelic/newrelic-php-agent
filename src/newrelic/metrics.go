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
	"regexp"
	"sort"
	"strings"
	"time"

	"newrelic/jsonx"
	"newrelic/limits"
	"newrelic/log"
)

// MetricForce describes the kind of a metric. Metrics can be either forced or
// unforced.
type MetricForce int

const (
	// Forced indicates a metric that is critical to quality in some way.
	// For example, the WebTransaction metric that powers the overview
	// charts. Forced metrics are not subject to per-harvest metric limits.
	Forced MetricForce = iota

	// Unforced indicates a metric that is not critical. Unforced metrics
	// are safe to discard when the maximum number of unique metrics per
	// harvest is reached.
	Unforced
)

type metricID struct {
	Name  string `json:"name"`
	Scope string `json:"scope,omitempty"`
}

type metricData struct {
	// These values are in the units expected by the collector.
	countSatisfied  float64 // Seconds, or count for Apdex
	totalTolerated  float64 // Seconds, or count for Apdex
	exclusiveFailed float64 // Seconds, or count for Apdex
	min             float64 // Seconds
	max             float64 // Seconds
	sumSquares      float64 // Seconds**2, or 0 for Apdex
}

type metric struct {
	forced MetricForce
	data   metricData
}

// A MetricTable represents an aggregate of metrics reported by agents
// during a harvest period. Each metric table enforces a limit on the
// maximum number of unique metrics that can be recorded. However,
// this is a soft limit.  Some metrics are critical and must be
// delivered. These are called forced metrics, and the maximum number
// of unique, forced metrics is unlimited.
type MetricTable struct {
	metricPeriodStart time.Time
	failedHarvests    int
	maxTableSize      int // After this max is reached, only forced metrics are
	// added
	count      int // The total number of metrics stored
	numDropped int // Number of unforced metrics dropped due to full
	// table
	// Metrics are uniquely identified by their name and scope.  Rather than
	// use a map which is indexed by a struct containing the name and scope,
	// we use a nested map approach to allow for looking up metrics without
	// copying their names from a byte slice to a string.
	//
	// The first (outer) map is by name, second (inner) map is by scope.
	// Unscoped metrics use an empty scope string.
	metrics map[string]map[string]*metric
}

// NewMetricTable returns a new metric table with capacity maxTableSize.
func NewMetricTable(maxTableSize int, now time.Time) *MetricTable {
	return &MetricTable{
		metricPeriodStart: now,
		metrics:           make(map[string]map[string]*metric),
		maxTableSize:      maxTableSize,
		failedHarvests:    0,
	}
}

func (mt *MetricTable) full() bool {
	return mt.count >= mt.maxTableSize
}

func (data *metricData) aggregate(src *metricData) {
	data.countSatisfied += src.countSatisfied
	data.totalTolerated += src.totalTolerated
	data.exclusiveFailed += src.exclusiveFailed

	if src.min < data.min {
		data.min = src.min
	}
	if src.max > data.max {
		data.max = src.max
	}

	data.sumSquares += src.sumSquares
}

// NumAttempts returns the total number of attempts sent to this endpoint.
// The value is the number of times the agent attempted to call the given endpoint before it was successful.
// This metric MUST NOT be generated if only one attempt was made.
// Does not include the successful attempt.
func (mt *MetricTable) NumFailedAttempts() float64 {
	return float64(mt.failedHarvests)
}

func (mt *MetricTable) mergeMetric(nameSlice []byte, nameString, scope string,
	m *metric) {
	var s map[string]*metric
	if nil == nameSlice {
		s = mt.metrics[nameString]
	} else {
		// This lookup is optimized by Go to avoid a copy.
		// See:  https://github.com/golang/go/issues/3512
		s = mt.metrics[string(nameSlice)]
	}

	var to *metric
	if nil != s {
		to = s[scope]
	}

	if nil == to {
		if mt.full() && (Unforced == m.forced) {
			mt.numDropped++
			return
		}

		if nil == s {
			if nil != nameSlice {
				nameString = string(nameSlice)
			}

			s = make(map[string]*metric)
			mt.metrics[nameString] = s
		}

		to = &metric{}
		*to = *m
		s[scope] = to
		mt.count++
		return
	}

	to.data.aggregate(&m.data)
}

// MergeFailed merges the given metrics into mt after a failed
// delivery attempt.  If FailedMetricAttemptsLimit attempts have been
// made, the metrics in from are discarded.  Unforced metrics in from
// may be discarded if mt is full.
func (mt *MetricTable) MergeFailed(from *MetricTable) {
	fails := from.failedHarvests + 1
	if fails > limits.FailedMetricAttemptsLimit {
		log.Debugf("discarding metrics: %d failed harvest attempts", fails)
		return
	}
	if from.metricPeriodStart.Before(mt.metricPeriodStart) {
		mt.metricPeriodStart = from.metricPeriodStart
	}
	mt.failedHarvests = fails
	log.Debugf("merging metrics: %d failed harvest attempts", fails)
	mt.Merge(from)
}

// Merge merges the given metric table into mt.
func (mt *MetricTable) Merge(from *MetricTable) {
	for name, s := range from.metrics {
		for scope, m := range s {
			mt.mergeMetric(nil, name, scope, m)
		}
	}
}

func (mt *MetricTable) add(nameSlice []byte, nameString, scope string,
	data metricData, force MetricForce) {
	mt.mergeMetric(nameSlice, nameString, scope,
		&metric{data: data, forced: force})
}

// AddRaw adds a metric to mt. If mt is full, and the metric is unforced,
// the metric will not be added.
func (mt *MetricTable) AddRaw(nameSlice []byte, nameString, scope string,
	data [6]float64, force MetricForce) {
	d := metricData{
		countSatisfied:  data[0],
		totalTolerated:  data[1],
		exclusiveFailed: data[2],
		min:             data[3],
		max:             data[4],
		sumSquares:      data[5],
	}
	mt.add(nameSlice, nameString, scope, d, force)
}

// AddCount adds a metric with the given call count to mt. If mt is
// full, and the metric is unforced, the metric will not be added.
func (mt *MetricTable) AddCount(name, scope string, count float64,
	force MetricForce) {
	mt.add(nil, name, scope, metricData{countSatisfied: count}, force)
}

// AddValue adds a metric with the given duration to mt. If mt is
// full, and the metric is unforced, the metric will not be added.
func (mt *MetricTable) AddValue(name, scope string, value float64,
	force MetricForce) {
	data := metricData{
		countSatisfied:  1,
		totalTolerated:  value,
		exclusiveFailed: 0,
		min:             value,
		max:             value,
		sumSquares:      value * value,
	}
	mt.add(nil, name, scope, data, force)
}

type collectorMetric struct {
	ID   metricID
	Data interface{}
}

type collectorMetrics []*collectorMetric

func (cms collectorMetrics) Len() int {
	return len(cms)
}

func (cms collectorMetrics) Swap(i, j int) {
	cms[i], cms[j] = cms[j], cms[i]
}

func (id1 metricID) Less(id2 metricID) bool {
	if id1.Scope == id2.Scope {
		return id1.Name < id2.Name
	}
	return id1.Scope < id2.Scope
}

func (cms collectorMetrics) Less(i, j int) bool {
	return cms[i].ID.Less(cms[j].ID)
}

func (c *collectorMetric) MarshalJSON() ([]byte, error) {
	return json.Marshal([]interface{}{c.ID, c.Data})
}

func (data *metricData) collectorData() [6]float64 {
	return [6]float64{
		data.countSatisfied,
		data.totalTolerated,
		data.exclusiveFailed,
		data.min,
		data.max,
		data.sumSquares,
	}
}

// CollectorJSON marshals the metric table to JSON according to the
// schema expected by the collector.
func (mt *MetricTable) CollectorJSON(id AgentRunID, now time.Time) ([]byte,
	error) {
	estimatedLen := mt.count * 128 /* bytes per metric */
	buf := bytes.NewBuffer(make([]byte, 0, estimatedLen))
	buf.WriteByte('[')

	jsonx.AppendString(buf, string(id))
	buf.WriteByte(',')
	jsonx.AppendInt(buf, mt.metricPeriodStart.Unix())
	buf.WriteByte(',')
	jsonx.AppendInt(buf, now.Unix())
	buf.WriteByte(',')

	buf.WriteByte('[')
	for name, scopes := range mt.metrics {
		for scope, metric := range scopes {
			buf.WriteByte('[')
			buf.WriteByte('{')
			buf.WriteString(`"name":`)
			jsonx.AppendString(buf, name)
			if scope != "" {
				buf.WriteString(`,"scope":`)
				jsonx.AppendString(buf, scope)
			}
			buf.WriteByte('}')
			buf.WriteByte(',')

			err := jsonx.AppendFloatArray(buf,
				metric.data.countSatisfied,
				metric.data.totalTolerated,
				metric.data.exclusiveFailed,
				metric.data.min,
				metric.data.max,
				metric.data.sumSquares)
			if err != nil {
				return nil, err
			}

			buf.WriteByte(']')
			buf.WriteByte(',')
		}
	}
	if mt.count > 0 {
		// Strip trailing comma from final metric.
		buf.Truncate(buf.Len() - 1)
	}
	buf.WriteByte(']')

	buf.WriteByte(']')
	return buf.Bytes(), nil
}

// CollectorJSONSorted marshals the metric table to JSON according to
// the schema expected by the collector. The metrics are ordered by
// name and scope.
func (mt *MetricTable) CollectorJSONSorted(id AgentRunID,
	now time.Time) ([]byte, error) {
	d, err := mt.CollectorJSON(id, now)
	if nil != err {
		return nil, err
	}
	return OrderScrubMetrics(d, nil)
}

// Empty returns true if the metric table is empty.
func (mt *MetricTable) Empty() bool {
	return 0 == mt.count
}

// Has returns true if the given metric exists in the metric table (regardless
// of scope).
func (mt *MetricTable) Has(name string) bool {
	_, ok := mt.metrics[name]
	return ok
}

// Data marshals the collection to JSON according to the schema expected
// by the collector.
func (mt *MetricTable) Data(id AgentRunID, harvestStart time.Time) ([]byte,
	error) {
	return mt.CollectorJSON(id, harvestStart)
}

// Audit marshals the collection to JSON according to the schema
// expected by the audit log. For metrics, the audit schema is the
// same as the schema expected by the collector.
func (mt *MetricTable) Audit(id AgentRunID, harvestStart time.Time) ([]byte,
	error) {
	return nil, nil // Use data
}

// FailedHarvest is a callback invoked by the processor when an
// attempt to deliver the contents of mt to the collector
// fails. After a failed delivery attempt, mt is merged into
// the upcoming harvest. This may result in some unforced metrics
// being discarded.
func (mt *MetricTable) FailedHarvest(newHarvest *Harvest) {
	newHarvest.Metrics.MergeFailed(mt)
}

// ApplyRules returns a new MetricTable containing the results of applying
// the given metric rename rules to mt.
func (mt *MetricTable) ApplyRules(rules MetricRules) *MetricTable {
	if nil == rules {
		return mt
	}
	if len(rules) == 0 {
		return mt
	}

	applied := NewMetricTable(mt.maxTableSize, mt.metricPeriodStart)

	for name, s := range mt.metrics {
		_, out := rules.Apply(name)

		if out != name {
			log.Debugf("metric renamed by rules: '%s' -> '%s'", name, out)
		}
		for scope, metric := range s {
			applied.mergeMetric(nil, out, scope, metric)
		}
	}

	return applied
}

type debugMetric struct {
	Name   string      `json:"name"`
	Forced bool        `json:"forced"`
	Data   interface{} `json:"data"`
	ID     metricID    `json:"-"` // For sorting
}

type debugMetrics []debugMetric

func (ds debugMetrics) Len() int           { return len(ds) }
func (ds debugMetrics) Swap(i, j int)      { ds[i], ds[j] = ds[j], ds[i] }
func (ds debugMetrics) Less(i, j int) bool { return ds[i].ID.Less(ds[j].ID) }

// DebugJSON marshals the metrics to JSON in a format useful for debugging.
func (mt *MetricTable) DebugJSON() string {
	metrics := make(debugMetrics, mt.count)
	i := 0
	for name, s := range mt.metrics {
		for scope, metric := range s {
			metrics[i].ID = metricID{Name: name, Scope: scope}
			metrics[i].Data = metric.data.collectorData()
			if metric.forced == Forced {
				metrics[i].Forced = true
			}
			metrics[i].Name = name
			i++
		}
	}
	// sort metrics for easy and deterministic JSON comparison tests
	sort.Sort(metrics)
	b, err := json.Marshal(metrics)
	if nil != err {
		return ""
	}
	return string(b)
}

func parseCollectorMetrics(data interface{},
	scrub []*regexp.Regexp) collectorMetrics {
	metricArr, ok := data.([]interface{})
	if !ok {
		return nil
	}

	cms := make(collectorMetrics, 0, len(metricArr))

	for _, x := range metricArr {
		m, ok := x.([]interface{})
		if !ok {
			return nil
		}

		if len(m) < 2 {
			return nil
		}
		id, ok := (m[0]).(map[string]interface{})
		if !ok {
			return nil
		}

		name := id["name"]
		if nil == name {
			return nil
		}
		nameStr, ok := name.(string)
		if !ok {
			return nil
		}

		scope := id["scope"]
		scopeStr := ""
		if nil != scope {
			scopeStr, ok = scope.(string)
			if !ok {
				return nil
			}
		}

		skip := false

		for _, re := range scrub {
			if re.MatchString(nameStr) {
				skip = true
				break
			}
		}

		if skip {
			continue
		}

		cms = append(cms, &collectorMetric{
			ID:   metricID{Name: nameStr, Scope: scopeStr},
			Data: m[1],
		})
	}

	return cms
}

// OrderScrubMetrics is used to sort the metric JSON for the collector for
// deterministic tests.
func OrderScrubMetrics(metrics []byte, scrub []*regexp.Regexp) ([]byte, error) {
	if nil == metrics {
		return nil, nil
	}

	if "" == strings.TrimSpace(string(metrics)) {
		return metrics, nil
	}

	var arr []JSONString
	err := json.Unmarshal(metrics, &arr)
	if nil != err {
		return nil, fmt.Errorf("invalid metrics format: %s", err)
	}

	if len(arr) < 3 {
		return nil, errors.New("invalid metrics format")
	}

	var inner []interface{}

	err = json.Unmarshal(arr[3], &inner)
	if nil != err {
		return nil, fmt.Errorf("invalid metrics format: %s", err)
	}

	cms := parseCollectorMetrics(inner, scrub)
	if nil == cms {
		return nil, errors.New("invalid metrics format")
	}

	sort.Sort(cms)

	js, err := json.Marshal(cms)
	if nil != err {
		return nil, err
	}

	arr[3] = js

	return json.Marshal(arr)
}
