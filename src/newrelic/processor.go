//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"encoding/json"
	"strings"
	"time"

	"newrelic/collector"
	"newrelic/infinite_tracing"
	"newrelic/limits"
	"newrelic/log"
	"newrelic/utilization"
)

type TxnData struct {
	ID     AgentRunID
	Sample AggregaterInto
}

type SpanBatch struct {
	id    AgentRunID
	count uint64
	batch []byte
}

type AppInfoReply struct {
	RunIDValid       bool
	State            AppState
	ConnectReply     []byte
	SecurityPolicies []byte
	ConnectTimestamp uint64
	HarvestFrequency uint16
	SamplingTarget   uint16
}

type AppInfoMessage struct {
	ID         *AgentRunID
	Info       *AppInfo
	ResultChan chan AppInfoReply
}

type ConnectAttempt struct {
	Key                 AppKey
	Collector           string
	Reply               *ConnectReply
	RawReply            collector.RPMResponse
	Err                 error
	RawSecurityPolicies []byte
}

type HarvestError struct {
	id    AgentRunID
	Reply collector.RPMResponse
	data  FailedHarvestSaver
}

type HarvestType uint16

const (
	HarvestMetrics      HarvestType = (1 << 0)
	HarvestErrors       HarvestType = (1 << 1)
	HarvestSlowSQLs     HarvestType = (1 << 2)
	HarvestTxnTraces    HarvestType = (1 << 3)
	HarvestTxnEvents    HarvestType = (1 << 4)
	HarvestCustomEvents HarvestType = (1 << 5)
	HarvestErrorEvents  HarvestType = (1 << 6)
	HarvestSpanEvents   HarvestType = (1 << 7)
	HarvestLogEvents    HarvestType = (1 << 8)
	HarvestDefaultData  HarvestType = HarvestMetrics | HarvestErrors | HarvestSlowSQLs | HarvestTxnTraces
	HarvestAll          HarvestType = HarvestDefaultData | HarvestTxnEvents | HarvestCustomEvents | HarvestErrorEvents | HarvestSpanEvents | HarvestLogEvents
)

// ProcessorHarvest represents a processor harvest event: when this is received by a
// processor, it indicates that a harvest should be performed for the harvest
// and run ID contained within. The word "event" doesn't appear in the type only
// to avoid confusion with analytic events.
type ProcessorHarvest struct {
	*AppHarvest
	ID       AgentRunID
	Type     HarvestType
	Blocking bool
}

type ProcessorConfig struct {
	Client          collector.Client
	IntegrationMode bool
	UtilConfig      utilization.Config
	AppTimeout      time.Duration
}

type Processor struct {
	// This map contains all applications, even those that are permanently
	// disconnected or have invalid license keys.
	apps map[AppKey]*App
	// This map contains only connected applications.
	harvests map[AgentRunID]*AppHarvest

	txnDataChannel        chan TxnData
	appInfoChannel        chan AppInfoMessage
	spanBatchChannel      chan SpanBatch
	connectAttemptChannel chan ConnectAttempt
	harvestErrorChannel   chan HarvestError
	quitChan              chan struct{}
	processorHarvestChan  chan ProcessorHarvest
	trackProgress         chan struct{} // Usually nil, used for testing
	appConnectBackoff     time.Duration
	cfg                   ProcessorConfig
	util                  *utilization.Data
}

func (p *Processor) processTxnData(d TxnData) {
	// First make sure the agent run id is valid
	h, ok := p.harvests[d.ID]
	if !ok {
		log.Debugf("bad TxnData: run id no longer valid: %s", d.ID)
		return
	}

	h.Harvest.commandsProcessed++
	h.App.LastActivity = time.Now()
	d.Sample.AggregateInto(h.Harvest)
}

func (p *Processor) processSpanBatch(d SpanBatch) {
	h, ok := p.harvests[d.id]
	if !ok {
		log.Debugf("bad span batch: run id no longer valid: %s", d.id)
		return
	}

	if h.TraceObserver != nil {
		h.TraceObserver.QueueBatch(d.count, d.batch)
	} else {
		log.Debugf("no trace observer initialized, dropping span batch")
	}
}

type ConnectArgs struct {
	RedirectCollector            string
	Payload                      []byte
	PayloadRaw                   *RawConnectPayload
	License                      collector.LicenseKey
	SecurityPolicyToken          string
	HighSecurity                 bool
	Client                       collector.Client
	AppKey                       AppKey
	AgentLanguage                string
	AgentVersion                 string
	PayloadPreconnect            []byte
	AppSupportedSecurityPolicies AgentPolicies
}

func ConnectApplication(args *ConnectArgs) ConnectAttempt {
	var err error
	rep := ConnectAttempt{Key: args.AppKey}
	preconnectReply := PreconnectReply{}

	args.Payload, err = EncodePayload(&RawPreconnectPayload{SecurityPolicyToken: args.SecurityPolicyToken, HighSecurity: args.HighSecurity})
	if err != nil {
		log.Errorf("unable to connect application: %v", err)
		rep.Err = err
		return rep
	}

	// Prepare preconnect call
	collectorHostname := collector.CalculatePreconnectHost(args.License, args.RedirectCollector)
	cs := collector.RpmControls{
		AgentLanguage: args.AgentLanguage,
		AgentVersion:  args.AgentVersion,
		Collectible: collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
			return args.Payload, nil
		}),
	}
	cmd := collector.RpmCmd{
		Name:      collector.CommandPreconnect,
		Collector: collectorHostname,
		License:   args.License,
		// Use default maximum, because we don't know the collector limit yet
		MaxPayloadSize: limits.DefaultMaxPayloadSizeInBytes,
	}

	// Make call to preconnect
	// return value is a struct with the body and any error from attempt
	// if something fails from this point on the error needs to be
	// propagated up into the return value (rep.Err) as code downstream
	// expects this field value to contain any errors which occurred
	// during the connect attempt and will not inspect the RawReply
	// field for an error value
	rep.RawReply = args.Client.Execute(cmd, cs)

	if nil != rep.RawReply.Err {
		rep.Err = rep.RawReply.Err
		return rep
	}

	rep.Err = json.Unmarshal(rep.RawReply.Body, &preconnectReply)
	if nil != rep.Err {
		return rep
	}

	// If there's a token that's part of the request, we need to check supported
	// policies vs. what we got back from preconnect
	err = nil
	if "" != args.SecurityPolicyToken {
		_, err = args.AppSupportedSecurityPolicies.verifySecurityPolicies(preconnectReply)
	}

	// Was there disagreement in the agent and preconnect policies? If
	// so, return early with an error on
	if nil != err {
		log.Errorf("%s", err.Error())
		rep.Err = err
		return rep
	}

	// Marshal preconnect security policies to send back to agent on successful connection
	policyReturnMap := make(map[string]bool)
	for k, v := range preconnectReply.SecurityPolicies {
		policyReturnMap[k] = v.Enabled
	}

	// Marshal security policies
	rep.RawSecurityPolicies, rep.Err = json.Marshal(policyReturnMap)
	if nil != rep.Err {
		return rep
	}

	// Prepare the connect call
	// If this is a Language Agent Security Policy (LASP) request, add the supported policies
	// and their enabled/disabled value to the payload
	err = nil
	if "" != args.SecurityPolicyToken {
		err = args.AppSupportedSecurityPolicies.addPoliciesToPayload(preconnectReply.SecurityPolicies, args.PayloadRaw)
	}

	// If something went wrong while adding the policies, bail with an error
	if nil != err {
		log.Errorf("%s", err.Error())
		rep.Err = err
		return rep
	}

	args.Payload, err = EncodePayload(&args.PayloadRaw)
	if err != nil {
		log.Errorf("unable to connect application: %v", err)
		rep.Err = err
		return rep
	}

	rep.Collector = preconnectReply.Collector
	cmd.Collector = rep.Collector

	cs.Collectible = collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
		return args.Payload, nil
	})
	cmd.Name = collector.CommandConnect

	// Make call to connect
	rep.RawReply = args.Client.Execute(cmd, cs)
	if nil != rep.RawReply.Err {
		rep.Err = rep.RawReply.Err
		return rep
	}

	// Process the connect reply
	processConnectMessages(rep.RawReply)
	rep.Reply, rep.Err = parseConnectReply(rep.RawReply.Body)

	return rep
}

func (p *Processor) shutdownAppHarvest(id AgentRunID) {
	if nil != p.harvests[id] {
		// We don't need to wait for this to happen, as long as it happens: the
		// processor doesn't rely on p.harvests having the agent run ID as a key,
		// and it's possible for this to deadlock if there's a trigger waiting to
		// send to the trigger channel while the app is being shut down.
		go p.harvests[id].Close()
		delete(p.harvests, id)
	}
}

func (p *Processor) shouldConnect(app *App, now time.Time) bool {
	if p.util == nil {
		return false
	}

	if !app.NeedsConnectAttempt(now, p.appConnectBackoff) {
		return false
	}
	return true
}

func (p *Processor) considerConnect(app *App) {
	now := time.Now()
	if !p.shouldConnect(app, now) {
		return
	}
	app.lastConnectAttempt = now

	dataRaw := app.info.ConnectPayload(p.util)

	args := &ConnectArgs{
		RedirectCollector:            app.info.RedirectCollector,
		PayloadRaw:                   dataRaw,
		License:                      app.info.License,
		SecurityPolicyToken:          app.info.SecurityPolicyToken,
		HighSecurity:                 app.info.HighSecurity,
		Client:                       p.cfg.Client,
		AppKey:                       app.Key(),
		AgentLanguage:                app.info.AgentLanguage,
		AgentVersion:                 app.info.AgentVersion,
		AppSupportedSecurityPolicies: app.info.SupportedSecurityPolicies,
	}

	go func() {
		p.connectAttemptChannel <- ConnectApplication(args)
	}()

}

func (p *Processor) processAppInfo(m AppInfoMessage) {
	var app *App
	r := AppInfoReply{State: AppStateUnknown}

	defer func() {
		if nil != app {
			r.State = app.state
			if AppStateConnected == app.state {
				r.ConnectReply = app.RawConnectReply
				r.SecurityPolicies = app.RawSecurityPolicies
				r.ConnectTimestamp = uint64(app.connectTime.Unix())
				r.HarvestFrequency = uint16(app.harvestFrequency.Seconds())
				r.SamplingTarget = uint16(app.samplingTarget)
			}
		}
		// Send the response back before attempting to connect the application
		m.ResultChan <- r
		if nil != app {
			p.considerConnect(app)
		}
	}()

	if nil != m.ID {
		if _, ok := p.harvests[*m.ID]; ok {
			r.RunIDValid = true
			return
		}
		// This agent run id must be out of date, fall through:
	}

	key := m.Info.Key()
	app = p.apps[key]
	if nil != app {
		//set LastActivity so we treat an AppInfo request for
		//a known app as activity.
		app.LastActivity = time.Now()
		return
	}

	if len(p.apps) > limits.AppLimit {
		log.Errorf("unable to add app '%s', limit of %d applications reached",
			m.Info, limits.AppLimit)
		return
	}

	app = NewApp(m.Info)
	p.apps[key] = app
}

func processConnectMessages(reply collector.RPMResponse) {
	var msgs struct {
		Messages []struct {
			Message string `json:"message"`
			Level   string `json:"level"`
		} `json:"messages"`
	}

	err := json.Unmarshal(reply.Body, &msgs)
	if nil != err {
		return
	}

	for _, msg := range msgs.Messages {
		switch strings.ToLower(msg.Level) {
		case "error":
			log.Errorf("%s", msg.Message)
		case "warn":
			log.Warnf("%s", msg.Message)
		case "info":
			log.Infof("%s", msg.Message)
		case "debug", "verbose":
			log.Debugf("%s", msg.Message)
		}
	}
}

func (p *Processor) processConnectAttempt(rep ConnectAttempt) {
	app := p.apps[rep.Key]
	if nil == app {
		return
	}

	app.RawConnectReply = rep.RawReply.Body
	if rep.RawReply.IsDisconnect() {
		app.state = AppStateDisconnected
		log.Warnf("app '%s' connect attempt returned %s; disconnecting", app, rep.RawReply.Err)
		return
	} else if rep.RawReply.IsRestartException() {
		// in accord with the spec, invalid license is a restart exception. Except we want
		//    to shutdown instead of restart.
		if rep.RawReply.IsInvalidLicense() {
			app.state = AppStateInvalidLicense
			log.Warnf("app '%s' connect attempt returned %s; shutting down", app, rep.RawReply.Err)
		} else {
			app.state = AppStateRestart
			log.Warnf("app '%s' connect attempt returned %s; restarting", app, rep.RawReply.Err)
		}
		return
	} else if nil != rep.Err {
		app.state = AppStateUnknown
		log.Warnf("app '%s' connect attempt returned %s", app, rep.Err)
		return
	}

	app.connectReply = rep.Reply
	app.state = AppStateConnected
	app.collector = rep.Collector
	app.RawSecurityPolicies = rep.RawSecurityPolicies

	// Set up the timing data to send to the agent for harvest estimation during event sampling.
	app.connectTime = time.Now()

	// The sampling_target and sampling_target_period have default values of 10 samples per 60 seconds.
	app.harvestFrequency = time.Duration(app.connectReply.SamplingFrequency) * time.Second
	app.samplingTarget = uint16(app.connectReply.SamplingTarget)

	if 0 == app.samplingTarget {
		app.samplingTarget = 10
	}
	if 0 == app.harvestFrequency {
		app.harvestFrequency = 60 * time.Second
	}

	// Set up the trigger that controls how often the daemon harvests all data.
	app.HarvestTrigger = getHarvestTrigger(app.info.License, app.connectReply)

	log.Infof("app '%s' connected with run id '%s'", app, app.connectReply.ID)

	p.harvests[*app.connectReply.ID] = NewAppHarvest(*app.connectReply.ID, app,
		NewHarvest(time.Now(), app.connectReply.EventHarvestConfig.EventConfigs), p.processorHarvestChan)
}

type harvestArgs struct {
	HarvestStart        time.Time
	id                  AgentRunID
	license             collector.LicenseKey
	collector           string
	agentLanguage       string
	agentVersion        string
	rules               MetricRules
	harvestErrorChannel chan<- HarvestError
	client              collector.Client
	splitLargePayloads  bool
	RequestHeadersMap   map[string]string
	maxPayloadSize      int

	// Used for final harvest before daemon exit
	blocking bool
}

func harvestPayload(p PayloadCreator, args *harvestArgs) {
	cmd := collector.RpmCmd{
		Name:              p.Cmd(),
		Collector:         args.collector,
		License:           args.license,
		RunID:             args.id.String(),
		RequestHeadersMap: args.RequestHeadersMap,
		MaxPayloadSize:    args.maxPayloadSize,
	}
	cs := collector.RpmControls{
		AgentLanguage: args.agentLanguage,
		AgentVersion:  args.agentVersion,
		Collectible: collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
			if auditVersion {
				return p.Audit(args.id, args.HarvestStart)
			}
			return p.Data(args.id, args.HarvestStart)
		}),
	}

	reply := args.client.Execute(cmd, cs)

	// We don't need to process the response to a harvest command unless an
	// error happened.  (Note that this may change if we have to support metric
	// cache ids).
	if nil == reply.Err {
		return
	}

	args.harvestErrorChannel <- HarvestError{
		Reply: reply,
		id:    args.id,
		data:  p,
	}
}

func considerHarvestPayload(p PayloadCreator, args *harvestArgs) {
	if p.Empty() {
		return
	}

	if args.blocking {
		// Invoked primarily by CleanExit
		harvestPayload(p, args)
	} else {
		go harvestPayload(p, args)
	}
}

func considerHarvestPayloadTxnEvents(txnEvents *TxnEvents, args *harvestArgs) {
	if args.splitLargePayloads && (txnEvents.events.Len() >= (limits.MaxTxnEvents / 2)) {
		events1, events2 := txnEvents.Split()
		considerHarvestPayload(&TxnEvents{events1}, args)
		considerHarvestPayload(&TxnEvents{events2}, args)
	} else {
		considerHarvestPayload(txnEvents, args)
	}
}

func harvestAll(harvest *Harvest, args *harvestArgs, harvestLimits collector.EventHarvestConfig, to *infinite_tracing.TraceObserver) {
	log.Debugf("harvesting %d commands processed", harvest.commandsProcessed)

	harvest.createFinalMetrics(harvestLimits, to)
	harvest.Metrics = harvest.Metrics.ApplyRules(args.rules)

	considerHarvestPayload(harvest.Metrics, args)
	considerHarvestPayload(harvest.CustomEvents, args)
	considerHarvestPayload(harvest.ErrorEvents, args)
	considerHarvestPayload(harvest.Errors, args)
	considerHarvestPayload(harvest.SlowSQLs, args)
	considerHarvestPayload(harvest.TxnTraces, args)
	considerHarvestPayloadTxnEvents(harvest.TxnEvents, args)
	considerHarvestPayload(harvest.SpanEvents, args)
	considerHarvestPayload(harvest.LogEvents, args)
}

func harvestByType(ah *AppHarvest, args *harvestArgs, ht HarvestType) {

	// The collector may provide custom reporting periods for harvesting
	// TxnEvents, CustomEvents, or ErrorEvents.  As a result, this
	// function harvests by type.
	harvest := ah.Harvest

	// In many cases, all types are harvested
	//    at the same time
	//       at the same rate.
	// In such cases, harvest all types and return.
	if ht&HarvestAll == HarvestAll {
		ah.Harvest = NewHarvest(time.Now(), ah.App.connectReply.EventHarvestConfig.EventConfigs)
		if args.blocking {
			// Invoked primarily by CleanExit
			harvestAll(harvest, args, ah.connectReply.EventHarvestConfig, ah.TraceObserver)
		} else {
			go harvestAll(harvest, args, ah.connectReply.EventHarvestConfig, ah.TraceObserver)
		}
		return
	}

	// Otherwise, harvest by type.  The first type is DefaultData.  This
	// comprises the Metrics, Errors, SlowSQLs, and TxnTraces whose
	// reporting periods have no custom reporting periods.
	if ht&HarvestDefaultData == HarvestDefaultData {

		log.Debugf("harvesting %d commands processed", harvest.commandsProcessed)

		harvest.createFinalMetrics(ah.connectReply.EventHarvestConfig, ah.TraceObserver)
		harvest.Metrics = harvest.Metrics.ApplyRules(args.rules)

		metrics := harvest.Metrics
		errors := harvest.Errors
		slowSQLs := harvest.SlowSQLs
		txnTraces := harvest.TxnTraces
		spanEvents := harvest.SpanEvents
		logEvents := harvest.LogEvents

		harvest.Metrics = NewMetricTable(limits.MaxMetrics, time.Now())
		harvest.Errors = NewErrorHeap(limits.MaxErrors)
		harvest.SlowSQLs = NewSlowSQLs(limits.MaxSlowSQLs)
		harvest.TxnTraces = NewTxnTraces()
		harvest.commandsProcessed = 0
		harvest.pidSet = make(map[int]struct{})

		considerHarvestPayload(metrics, args)
		considerHarvestPayload(errors, args)
		considerHarvestPayload(slowSQLs, args)
		considerHarvestPayload(txnTraces, args)
		considerHarvestPayload(spanEvents, args)
		considerHarvestPayload(logEvents, args)
	}

	eventConfigs := ah.App.connectReply.EventHarvestConfig.EventConfigs

	// The next three types are those which may have individually-configured
	// custom reporting periods; they each may be harvested at different rates.
	if ht&HarvestCustomEvents == HarvestCustomEvents && eventConfigs.CustomEventConfig.Limit != 0 {
		log.Debugf("harvesting custom events")

		customEvents := harvest.CustomEvents
		harvest.CustomEvents = NewCustomEvents(eventConfigs.CustomEventConfig.Limit)
		considerHarvestPayload(customEvents, args)
	}

	if ht&HarvestErrorEvents == HarvestErrorEvents && eventConfigs.ErrorEventConfig.Limit != 0 {
		log.Debugf("harvesting error events")

		errorEvents := harvest.ErrorEvents
		harvest.ErrorEvents = NewErrorEvents(eventConfigs.ErrorEventConfig.Limit)
		considerHarvestPayload(errorEvents, args)
	}

	if ht&HarvestTxnEvents == HarvestTxnEvents && eventConfigs.AnalyticEventConfig.Limit != 0 {
		log.Debugf("harvesting transaction events")

		txnEvents := harvest.TxnEvents
		harvest.TxnEvents = NewTxnEvents(eventConfigs.AnalyticEventConfig.Limit)
		considerHarvestPayloadTxnEvents(txnEvents, args)
	}

	if ht&HarvestSpanEvents == HarvestSpanEvents && eventConfigs.SpanEventConfig.Limit != 0 {
		log.Debugf("harvesting span events")

		spanEvents := harvest.SpanEvents
		harvest.SpanEvents = NewSpanEvents(eventConfigs.SpanEventConfig.Limit)
		considerHarvestPayload(spanEvents, args)

	}
}

func (p *Processor) doHarvest(ph ProcessorHarvest) {
	app := ph.AppHarvest.App
	harvestType := ph.Type
	id := ph.ID

	if p.cfg.AppTimeout > 0 && app.Inactive(p.cfg.AppTimeout) {
		log.Infof("removing %q with run id %q for lack of activity within %v",
			app, id, p.cfg.AppTimeout)
		p.shutdownAppHarvest(id)
		delete(p.apps, app.Key())

		return
	}

	args := harvestArgs{
		HarvestStart:        time.Now(),
		id:                  id,
		license:             app.info.License,
		collector:           app.collector,
		agentLanguage:       app.info.AgentLanguage,
		agentVersion:        app.info.AgentVersion,
		rules:               app.connectReply.MetricRules,
		harvestErrorChannel: p.harvestErrorChannel,
		client:              p.cfg.Client,
		RequestHeadersMap:   app.connectReply.RequestHeadersMap,
		maxPayloadSize:      app.connectReply.MaxPayloadSizeInBytes,
		// Splitting large payloads is limited to applications that have
		// distributed tracing on. That restriction is a saftey measure
		// to not overload the backend by sending two payloads instead
		// of one every 60 seconds.
		splitLargePayloads: app.info.Settings["newrelic.distributed_tracing_enabled"] == true,
		blocking:           ph.Blocking,
	}
	harvestByType(ph.AppHarvest, &args, harvestType)
}

func (p *Processor) processHarvestError(d HarvestError) {
	h, ok := p.harvests[d.id]
	if !ok {
		// Very possible:  One harvest goroutine may encounter a ErrForceRestart
		// before this.
		log.Debugf("unable to process harvest response %q for unknown id %q",
			d.Reply, d.id)
		return
	}

	app := h.App
	log.Warnf("app %q with run id %q received %s", app, d.id, d.Reply.Err)

	h.Harvest.IncrementHttpErrors(d.Reply.StatusCode)

	if d.Reply.ShouldSaveHarvestData() {
		d.data.FailedHarvest(h.Harvest)
	}
	switch {
	case d.Reply.IsDisconnect() || app.state == AppStateDisconnected:
		app.state = AppStateDisconnected
		p.shutdownAppHarvest(d.id)
	case d.Reply.IsRestartException() || app.state == AppStateRestart:
		app.state = AppStateUnknown
		p.shutdownAppHarvest(d.id)
		p.considerConnect(app)
	}
}

func NewProcessor(cfg ProcessorConfig) *Processor {
	return &Processor{
		apps:                  make(map[AppKey]*App),
		harvests:              make(map[AgentRunID]*AppHarvest),
		txnDataChannel:        make(chan TxnData, limits.TxnDataChanBuffering),
		appInfoChannel:        make(chan AppInfoMessage, limits.AppInfoChanBuffering),
		spanBatchChannel:      make(chan SpanBatch, limits.SpanBatchChanBuffering),
		connectAttemptChannel: make(chan ConnectAttempt),
		harvestErrorChannel:   make(chan HarvestError),
		quitChan:              make(chan struct{}),
		processorHarvestChan:  make(chan ProcessorHarvest),
		appConnectBackoff:     limits.AppConnectAttemptBackoff,
		cfg:                   cfg,
	}
}

func (p *Processor) Run() error {
	utilChan := make(chan *utilization.Data, 1)

	go func() {
		utilChan <- utilization.Gather(p.cfg.UtilConfig)
	}()

	for {
		// Nested select to give priority to appInfoChannel.
		select {
		case d := <-p.appInfoChannel:
			p.processAppInfo(d)
		case <-p.quitChan:
			return nil
		default:
			select {
			case d := <-utilChan:
				p.util = d
				utilChan = nil // We'll never check again.
			case <-p.quitChan:
				return nil
			case d := <-p.processorHarvestChan:
				p.doHarvest(d)

			case d := <-p.txnDataChannel:
				p.processTxnData(d)

			case d := <-p.appInfoChannel:
				p.processAppInfo(d)

			case d := <-p.spanBatchChannel:
				p.processSpanBatch(d)

			case d := <-p.connectAttemptChannel:
				p.processConnectAttempt(d)

			case d := <-p.harvestErrorChannel:
				p.processHarvestError(d)
			}
		}

		// This is only used for testing
		if nil != p.trackProgress {
			p.trackProgress <- struct{}{}
		}
	}
}

type AgentDataHandler interface {
	IncomingTxnData(id AgentRunID, sample AggregaterInto)
	IncomingSpanBatch(batch SpanBatch)
	IncomingAppInfo(id *AgentRunID, info *AppInfo) AppInfoReply
}

func integrationLog(now time.Time, id AgentRunID, p PayloadCreator) {
	if p.Empty() {
		return
	}
	js, err := IntegrationData(p, id, now)
	if nil != err {
		log.Errorf("unable to create audit json payload for '%s': %s", p.Cmd(), err)
		return
	}
	log.Infof("NR_INTEGRATION_TEST '%s' '%s'", p.Cmd(), js)
}

func (p *Processor) IncomingTxnData(id AgentRunID, sample AggregaterInto) {
	if p.cfg.IntegrationMode {
		h := NewHarvest(time.Now(), collector.NewHarvestLimits(nil))
		sample.AggregateInto(h)
		now := time.Now()
		integrationLog(now, id, h.Metrics)
		integrationLog(now, id, h.CustomEvents)
		integrationLog(now, id, h.ErrorEvents)
		integrationLog(now, id, h.Errors)
		integrationLog(now, id, h.SlowSQLs)
		integrationLog(now, id, h.SpanEvents)
		integrationLog(now, id, h.TxnTraces)
		integrationLog(now, id, h.TxnEvents)
	}
	p.txnDataChannel <- TxnData{ID: id, Sample: sample}
}

func (p *Processor) IncomingAppInfo(id *AgentRunID, info *AppInfo) AppInfoReply {
	resultChan := make(chan AppInfoReply, 1)
	p.appInfoChannel <- AppInfoMessage{ID: id, Info: info, ResultChan: resultChan}
	out := <-resultChan
	close(resultChan)
	return out
}

func (p *Processor) IncomingSpanBatch(batch SpanBatch) {
	p.spanBatchChannel <- batch
}

// CleanExit terminates p.Run()'s loop and iterates over the processor's
// harvests, explicitly calling doHarvest on them. By setting the
// ProcessorHarvest's Type to HarvestFinal, later functions avoid goroutines
// so that we only return from this function when all harvests complete
func (p *Processor) CleanExit() {
	// Terminate p.Run()'s loop and stop receiving data
	p.quitChan <- struct{}{}
	p.txnDataChannel = nil
	p.appInfoChannel = nil

	// Harvest all remaining data
	for id, ah := range p.harvests {
		p.doHarvest(ProcessorHarvest{
			AppHarvest: ah,
			ID:         id,
			Type:       HarvestAll,
			Blocking:   true,
		})
	}
}

func (p *Processor) quit() {
	p.quitChan <- struct{}{}
}
