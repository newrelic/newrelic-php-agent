# Development

## Requirements

### Development Platform

Currently, the PHP agent has only been developed and built on linux systems. Below is a non-exhaustive list of platforms (and their dependences) that the PHP agent can be built on:

[ This will be filled in with the findings from make system investigation ]

### PHP

The PHP agent supports PHP versions `5.3`, `5.4`, `5.5`, `5.6`, `7.0`, `7.1`, `7.2`, `7.3`, and `7.4`.

### Go

Install the [latest version of Go](https://golang.org/dl/). Typically it's installed in `/usr/local/go`. Be sure to extend your **`$PATH`** to include it!

### Submodules

To run all the unit tests, you will need to get the `cross_agent_tests` submodule. This can be done by running:
```
git submodule update --init
```
## Build the PHP Agent

Hooray, you've met all the requirements! Now let's build the agent. From the top level of the php agent directory, run `make` to build both the agent and daemon. The agent can be found in `agent/.libs/newrelic.so`, and the daemon and associated Go binaries can be found in `newrelic-php-agent/bin`.

```
make
```

To only build the agent:

```
make agent
```

To only build the daemon:

```
make daemon
```

See [make](#make) for more information of the makefile capabilities.

### Configuration settings

The PHP agent is configured using a **`newrelic.ini`** config file. To get started with configuring the PHP agent, create your own copy of the template found in `agent/scripts/newrelic.ini.template`. The `newrelic.ini` file needs to be placed in the same location as your `php.ini`. Running `php --ini` will tell you where that is. Once your `newrelic.ini` is created and in the correct location, edit it to add a license key, a unique application name, and a full path to your daemon. Make sure your log directory exists and is read/writable; `/opt/nr/logs` or `/var/log/` are good options. You may want to change the log levels for the [agent](https://docs.newrelic.com/docs/agents/php-agent/configuration/php-agent-configuration#inivar-loglevel) and [daemon](https://docs.newrelic.com/docs/agents/php-agent/configuration/php-agent-configuration#inivar-daemon-loglevel). For more information on the PHP agent configuration see our [documentation](https://docs.newrelic.com/docs/agents/php-agent/configuration/php-agent-configuration).

Here are some suggested settings to change:
```
newrelic.license = "<my_license_key>"
newrelic.logfile = "/opt/nr/logs/php_agent.log"
newrelic.loglevel = "verbosedebug"
newrelic.appname = "<my_app_name>"
newrelic.daemon.logfile = "/opt/nr/logs/newrelic-daemon.log"
newrelic.daemon.loglevel = "debug"
newrelic.daemon.location = "<parent_directory_of_php_agent>/newrelic-php-agent/bin/daemon"
newrelic.daemon.collector_host = "staging-collector.newrelic.com"
```

Once the agent is compiled, make sure the `newrelic.so` extension you just created is somewhere PHP can see it. You can do this by editing the `extension` setting.
```
extension = "<parent_directory_of_php_agent>/newrelic-php-agent/agent/.libs/newrelic.so"
```

## Testing

The PHP agent unit tests are split into two categories. [Agent](../agent/tests) and [axiom](../axiom/tests) unit tests. The agent unit tests contain all of the tests related to PHP instrumentation while the axiom unit tests contain all of the PHP independent tests. 

To run both the agent and axiom unit tests:

```
make run_tests
```

Both unit tests can be run under valgrind to check for memory issues:

```
make valgrind
```

Agent integration tests:
Although integration testing is available in the makefile, all tests will fail due to internal licensing requirements. This will be changed in the future and will allow integration tests to be run locally by anyone. 

## Make

Here are the non-exhaustive capabilities of the Makefile. All targets should support parallel compilation using `-j`.

### Build commands

|command|function|
|-------|--------|
|`make -j8`|use 8 CPUs|
|`make agent`|just build the agent|
|`make agent-install`|build the agent and install it to the active PHP installation.|
|`make daemon`|build the daemon|

### Test commands

|command|function|
|-------|--------|
|`make run_tests`|build and run the agent and axiom unit tests (Agent unit tests currently require Linux)|
|`make run_tests TESTS=test_txn`|run only the **test_txn.c** tests|
|`make run_tests SKIP_TESTS=test_rpm`|skip the **test_rpm.c** tests|
|`make valgrind`|build and run the agent tests under valgrind and fail if any memory leaks occur|
|`make agent-valgrind`|build and run the agent tests under valgrind|
|`make axiom-valgrind`|build and run the axiom tests under valgrind|
|`make daemon_integration`|build the daemon unit tests|
|`make daemon_bench`|run the daemon performance tests|

### Clean commands

|command|function|
|-------|--------|
|`make clean`|Cleans all the things! This should return your working directory to a pristine, slightly minty state|
|`make agent-clean`|Just cleans the agent. Useful if you want to build against a different PHP version but don't want to rebuild axiom or the daemon.|
|`make axiom-clean`|Just clean axiom|
|`make daemon-clean`|Just clean the daemon|

## Take the agent for a spin

Make sure the new newrelic.so you just created is somewhere PHP can see it. This is done by using the `extension` setting in the `newrelic.ini`.

**phpinfo()**
Request phpinfo() in the terminal and watch the activity in the daemon and agent logs. Any php program will automatically start the agent and daemon and in a few moments you can view the activity in then check your New relic one account. Wow, look at all those non-web background transactions!

```
php -i # Do this a few times to fully connect
tail -f /opt/nr/logs/php_agent.log
tail -f /opt/nr/logs/newrelic-daemon.log
```
### Start the daemon manually

You can start the daemon manually or via the agent (the previous example had the agent start the daemon). To start it manually, tell the daemon not to launch automatically in your newrelic.ini with newrelic.daemon.dont_launch = 3 and start it with appropriate flags. See our [daemon .ini](https://docs.newrelic.com/docs/agents/php-agent/configuration/php-agent-configuration#inivar-daemon-settings) and [newrelic.cfg](https://docs.newrelic.com/docs/agents/php-agent/configuration/proxy-daemon-newreliccfg-settings) settings for more info.

```
./bin/daemon -f -logfile stdout -loglevel debug
```
## PHP Agent API

The PHP agent API allows you to extend the functionality of the PHP agent. The agent API is included by default with your installation, so no additional configuration is required to use the agent.

Among other things, you can use the API to customize your app name collect errors; record custom attributes, custom events, and custom metrics; and enable or disable browser monitoring. For an introduction to this API, see Guide to [PHP agent API](https://docs.newrelic.com/docs/agents/php-agent/api-guides/guide-using-php-agent-api).

## Troubleshooting

If you are having issues with the PHP Agent, our [troubleshooting](https://docs.newrelic.com/docs/agents/php-agent/troubleshooting) is a great start to look for an answer. 