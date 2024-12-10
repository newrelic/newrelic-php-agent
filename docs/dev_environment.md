# Dockerized development environment for the New Relic PHP Agent

The dockerized development environment prototype allows contributors to both develop and test (using unit tests AND integration tests) without having to set up a specific environment on their own system.  Integration tests require  a valid New Relic license key. Sign up at https://newrelic.com/signup for a free account.

*PLEASE NOTE* While this is now usable, this is still a work in progress.

docker-compose spins up `mysql` and `redis` and other databases in separate containers.

## Prerequisites

### 1. Docker Compose

Dockerized development environment for the New Relic PHP Agent uses following Docker Compose services as a runtime platform. So you need `docker` with `docker compose` installed.

### 2. Environment variables

Dockerized development environment for the New Relic PHP Agent needs a valid license key available in `NEW_RELIC_LICENSE_KEY` environment variable.
This environment variable must be set prior to starting Dockerized development environment for the New Relic PHP Agent. The easiest way to set
`NEW_RELIC_LICENSE_KEY` environment variable is via `.env` file. Simply create `.env` file in the top level directory, and add definition
of `NEW_RELIC_LICENSE_KEY` environment variable there, e.g.:
```
NEW_RELIC_LICENSE_KEY=...
```

The second, optional environment variable, that controls PHP version in Dockerized development environment for the New Relic PHP Agent is `PHP`. `PHP` defaults to latest PHP supported by the agent.
This environment variable can be provided at the time when Dockerized development environment for the New Relic PHP Agent is started, e.g.:
```
make dev-shell PHP=8.2
```

## Options for using the environment

### With a shell environment

To start the dev environment type `make dev-shell`.  This will spin up `devenv` service in `agent-devenv` container, with:
 - latest PHP supported by the agent (this can be overriden with `PHP` environment variable like this: `make dev-shell PHP=8.2`)
 - all the tools needed to build the agent
 - all the tools needed to run unit tests
 - all the tools and supporting services to run integration tests

A prompt will open and you’ll be able to compile and run all `make` commands right away with no additional setup (for example: `make -j4 all` or `make -j4 valgrind` or `make -j4 run_tests`).

After compiling the agent, the integration tests can be run using the `integration_runner`.

To run all integration tests, from the prompt, run: `./bin/integration_runner -agent ./agent/.libs/newrelic.so`

To run `redis` only: `./bin/integration_runner -agent ./agent/.libs/newrelic.so -pattern tests/integration/redis/*`

To end the session type `exit`.  You can run `make dev-stop` to stop the docker-compose containers.

In the shell, you can run all `make` commands as you normally would.

### Build only

`make dev-build`

### Unit Tests only

`make dev-unit tests`

### Integration Tests only

`make dev-integration-tests`

### Build and test all

`make dev-all`

### Stop all containers

`make dev-stop`

## Next steps and issues

### There is possibly some incompatibility with mysql in the main build container as one of the mysql unit tests fails.  Unless this is resolved, It might make sense at a future point to have the integration tests run from a different container than the build container.

