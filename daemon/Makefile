#
# Build the daemon and related utilities
#

include ../make/version.mk
-include ../make/secrets.mk

#
# Global variables
#

# Executable targets
BINARIES := client daemon integration_runner stressor

# Default go invocation command
GO := go

# module path
GO_MODULE := github.com/newrelic/newrelic-php-agent/daemon

# Pass relevant flags from secrets.mk && version.mk to the version and secrets packages
GOFLAGS := -ldflags '-X $(GO_MODULE)/internal/newrelic/version.Number=$(AGENT_VERSION) 							\
					-X $(GO_MODULE)/internal/newrelic/version.Commit=$(GIT_COMMIT) 								\
					-X $(GO_MODULE)/internal/newrelic/secrets.NewrelicCollectorHost=$(NEWRELIC_COLLECTOR_HOST) 	\
					-X $(GO_MODULE)/internal/newrelic/secrets.NewrelicLicenseKey=$(NEWRELIC_LICENSE_KEY) 		\
					-X $(GO_MODULE)/internal/newrelic/secrets.NewrelicCollectorKeys=$(NEWRELIC_COLLECTOR_KEYS)	\
					-X $(GO_MODULE)/internal/newrelic/secrets.NewrelicAccountId=$(ACCOUNT_supportability)		\
					-X $(GO_MODULE)/internal/newrelic/secrets.NewrelicAppId=$(APP_supportability)' $(GO_TAGS)	

DAEMON_COV_FILE = daemon_coverage.out

#
# Make targets
#
# default target to build ALL binaries
all: $(BINARIES)

# cleanup rules
.PHONY: clean
clean:
	$(GO) clean $(GOFLAGS) -i -cache -testcache ./...

# Delete any compiled Go binaries before each build to force a re-link
# Ensures the version and commit variables are set correctly by the linker
.PHONY: clean-bin
clean-bin:
	$(GO) clean $(GOFLAGS) -i ./...


.PHONY: go-minimum-version
go-minimum-version:
	@if $(GO) version | awk '/go1.(1[1-9]|[2-9][0-9])[\. ]/ { exit 0 } { exit 1 }'; then \
		true; \
	else \
		echo -n 'Go 1.11 or newer required; found '; $(GO) version; false; \
	fi

# setup depenencies for daemon
# THIS IS TEMPORARY NEEDS TO BE REPLACED BY PINNED VENDOR DEPENDENCIES
# will download vendor dependencies
.PHONY: go-setup-dependencies
go-setup-dependencies:
	$(GO) mod tidy
	$(GO) mod vendor

# Build the binaries
.PHONY: $(BINARIES)
$(BINARIES): clean-bin go-setup-dependencies go-minimum-version
	$(GO) install $(GOFLAGS) $(GO_MODULE)/cmd/$@

# the -race flag enabled the integrated Go race detector. Output to stderr.
race: clean-bin go-minimum-version go-setup-dependencies
	$(GO) install -race $(GOFLAGS) $(GO_MODULE)/$@

# Test targets
.PHONY: bench
bench:
	$(GO) test $(GOFLAGS) -bench=. ./...

.PHONY: cover
cover:
	rm -f $(DAEMON_COV_FILE)
	$(GO) test -coverprofile=$(DAEMON_COV_FILE) $(GOFLAGS) $(GO_MODULE)/newrelic
	$(GO) tool cover -html=$(DAEMON_COV_FILE)
	rm -f $(DAEMON_COV_FILE)

.PHONY: integration
integration: go-minimum-version go-setup-dependencies
	$(GO) test $(GOFLAGS) -tags integration ./...

.PHONY: test
test: go-minimum-version go-setup-dependencies
	$(GO) test $(GOFLAGS) ./...

