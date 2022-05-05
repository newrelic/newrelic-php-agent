# Dockerized development environment for the New Relic PHP Agent

The dockerized development environment prototype allows contributors to both develop and test (using unit tests AND integration tests) without having to set up a specific environment on their own system.  Integration tests send actual data to the New Relic backend and therefore requires a license key (Sign up at https://newrelic.com/signup for a free account.).

*PLEASE NOTE* While this is now usable, this is still a work in progress.

Currently docker-compose spins up `mysql` and `redis` databases in separate containers.

Two environment variables to note:
`NEWRELIC_LICENSE_KEY` is required to run the integration tests and should be set to your NR license key.
If your collector isn’t the default (collector.newrelic.com), set the `NEWRELIC_COLLECTOR_HOST` to the appropriate value.

PHP_VER can also be set, but hasn’t been tested beyond `8.0`.

Set all environment variables prior to running the development environment.  

#Options for using the environment

## With a shell environment

To start the dev environment type `make dev-shell`.  This will create a set of docker containers.
A prompt will open and you’ll be able to compile and run all `make` commands right away with no additional setup (for example: `make -j4 all` or `make -j4 valgrind` or `make -j4 run_tests`).

After compiling the agent, the integration tests can be run using the `integration_runner`.
To run all integration tests, from the prompt, run: `./bin/integration_runner -agent ./agent/.libs/newrelic.so`
To run `redis` only: `./bin/integration_runner -agent ./agent/.libs/newrelic.so -pattern tests/integration/redis/*`

To end the session type `exit`.  You can run `make dev-stop` to stop the docker-compose containers.

In the shell, you can run all `make` commands as you normally would.

## Build only

`make dev-build`

## Unit Tests only

`make dev-unit tests`

## Integration Tests only

`make dev-integration-tests

## Build and test all

`make dev-all`

## Stop all containers

`make dev-all`

# Next steps and issues
Section needs to be updated.

## Currently only `mysql` and `redis` containers are integrated.  Other databases need to be added to the docker-compose.yml, but many of the tests cases run as-is.

## The explain plan in the sql tests contain partition/filtered properties
 but the tests can't currently be updated  to reflect that since the sql 
 database on our CI doesn't include those values.
 RUN docker-php-ext-install pdo pdo_mysql

## There is possibly some incompatibility with mysql in the main build container.  It might make sense at a future point to have the integration tests run from a different container than the build container.

## There are currently 0 known failing integration tests with this prototype.
