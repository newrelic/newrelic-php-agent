/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_agent.h"
#include "nr_app.h"
#include "nr_app_private.h"
#include "nr_commands.h"
#include "nr_rules.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_reply.h"
#include "util_strings.h"
#include "util_system.h"

#include "tlib_main.h"

#define TEST_LICENSE "0123456789012345678901234567890123456789"
#define TEST_AGENT_RUN_ID "12345678"
#define TEST_LABELS_JSON \
  "{\"Data Center\":\"US-East\",\"Server Color\":\"Beige\"}"
#define TEST_METADATA_JSON \
  "{\"NEW_RELIC_METADATA_ZIP\":\"zap\",\"NEW_RELIC_METADATA_ONE\":\"one\"}"

typedef struct _test_app_state_t {
  bool cmd_appinfo_succeed;
  int cmd_appinfo_called;
  bool last_daemon_query_reset;
} test_app_state_t;

int nr_get_daemon_fd(void) {
  return 0;
}

typedef struct _nrintharvest_t {
  int dummy;
} nrintharvest_t;

static nrobj_t* settings_callback_fn(void) {
  return nro_create_from_json("[\"my_settings\"]");
}

#define NR_EXPECTED_PRINTABLE_LICENSE "12...89"

static void test_app_match(void) {
  nr_status_t rv;
  nrapp_t app;
  nr_app_info_t info;

  nr_memset(&info, 0, sizeof(info));
  nr_memset(&app.info, 0, sizeof(app.info));

  app.info.license = nr_strdup("mylicense");
  app.info.appname = nr_strdup("one;two");

  rv = nr_app_match(0, 0);
  tlib_pass_if_status_failure("zero params", rv);

  rv = nr_app_match(&app, 0);
  tlib_pass_if_status_failure("null info", rv);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("one;two");
  rv = nr_app_match(0, &info);
  tlib_pass_if_status_failure("null app", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = NULL;
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_failure("null appname", rv);
  nr_app_info_destroy_fields(&info);

  info.license = NULL;
  info.appname = nr_strdup("one;two");
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_failure("license doesnt match", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("on;two");
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_failure("appname doesnt match", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("onee");
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_failure("appname doesnt match", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("on");
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_failure("appname doesnt match", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("one;two");
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_success("multiple appname success", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("one;other");
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_failure("all appnames are used", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("one");
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_failure("all appnames are used", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("one;two;three");
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_failure("all appnames are used", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("one;two");
  info.trace_observer_host = nr_strdup("trace-observer");
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_failure("trace observer host failure", rv);
  nr_app_info_destroy_fields(&info);

  app.info.trace_observer_host = nr_strdup("trace-observer");
  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("one;two");
  info.trace_observer_host = NULL;
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_failure("trace observer host failure", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("one;two");
  info.trace_observer_host = nr_strdup("trace-observer");
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_success("trace observer host success", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("one;two");
  info.trace_observer_host = nr_strdup("trace-observer");
  info.trace_observer_port = 443;
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_failure("trace observer port failure", rv);
  nr_app_info_destroy_fields(&info);

  info.license = nr_strdup("mylicense");
  info.appname = nr_strdup("one;two");
  info.trace_observer_host = nr_strdup("trace-observer");
  info.trace_observer_port = 443;
  app.info.trace_observer_port = 443;
  rv = nr_app_match(&app, &info);
  tlib_pass_if_status_success("trace observer port success", rv);
  nr_app_info_destroy_fields(&info);
  nr_app_info_destroy_fields(&app.info);
}

static void test_applist_create_destroy(void) {
  nrapplist_t* applist;

  applist = nr_applist_create();
  tlib_pass_if_not_null("applist created", applist);

  nr_applist_destroy(&applist);
  tlib_pass_if_null("applist destroy", applist);

  /* Don't blow up */
  applist = 0;
  nr_applist_destroy(0);
  nr_applist_destroy(&applist);
}

static void test_find_or_add_app(void) {
  int i;
  nrapp_t* app;
  nrapplist_t* applist = nr_applist_create();
  nr_app_info_t info;
  char* appname;
  char* license;
  char* system_host_name = nr_system_get_hostname();
  char* trace_observer_host;

  license = nr_strdup("1234500000000000000000000000000000006789");
  appname = nr_strdup("test-app");
  trace_observer_host = nr_strdup("trace-observer");

  nr_memset(&info, 0, sizeof(info));
  info.license = license;
  info.version = nr_strdup("my_version");
  info.lang = nr_strdup("my_language");
  info.appname = appname;
  info.settings = nro_create_from_json("[\"my_settings\"]");
  info.environment = nro_create_from_json("[\"my_environment\"]");
  info.labels = nro_create_from_json(TEST_LABELS_JSON);
  info.metadata = nro_create_from_json(TEST_METADATA_JSON);
  info.host_display_name = nr_strdup("my_host_display_name");
  info.high_security = 0;
  info.redirect_collector = nr_strdup("collector.newrelic.com");
  info.trace_observer_host = trace_observer_host;

  tlib_pass_if_int_equal("applist starts empty", 0, applist->num_apps);

  /*
   * Test : Bad Parameters
   */
  app = nr_app_find_or_add_app(0, 0);
  tlib_pass_if_null("zero params", app);
  app = nr_app_find_or_add_app(applist, 0);
  tlib_pass_if_null("zero info", app);
  app = nr_app_find_or_add_app(0, &info);
  tlib_pass_if_null("zero applist", app);

  /*
   * Fill up applist
   */
  for (i = 0; i < NR_APP_LIMIT; i++) {
    char entity_name[64];
    char app_name[80];
    char lic[64];

    /* License must be 40 characters for plicense creation. */
    snprintf(lic, 41, "12345%05d000000000000000000000000006789", i);
    snprintf(entity_name, 64, "appname%d", i);
    snprintf(app_name, 80, "%s;OtherApp", entity_name);

    info.appname = app_name;
    info.license = lic;

    app = nr_app_find_or_add_app(applist, &info);

    tlib_pass_if_not_null("new app", app);
    tlib_pass_if_int_equal("new app", i + 1, applist->num_apps);
    if (0 != app) {
      tlib_pass_if_str_equal("new app", app_name, app->info.appname);
      tlib_pass_if_str_equal("new app", entity_name, app->entity_name);
      tlib_pass_if_str_equal("new app", lic, app->info.license);
      tlib_pass_if_str_equal("new app", info.version, app->info.version);
      tlib_pass_if_str_equal("new app", info.lang, app->info.lang);
      tlib_pass_if_str_equal("new app", NR_EXPECTED_PRINTABLE_LICENSE,
                             app->plicense);
      tlib_pass_if_int_equal("new app", (int)NR_APP_UNKNOWN, (int)app->state);
      test_obj_as_json("new app", app->info.settings, "[\"my_settings\"]");
      test_obj_as_json("new app", app->info.environment,
                       "[\"my_environment\"]");
      test_obj_as_json("new app", app->info.labels, TEST_LABELS_JSON);
      test_obj_as_json("new app", app->info.metadata, TEST_METADATA_JSON);
      tlib_pass_if_str_equal("new app", info.host_display_name,
                             app->info.host_display_name);
      tlib_pass_if_str_equal("new app", info.redirect_collector,
                             app->info.redirect_collector);
      tlib_pass_if_str_equal("new app", system_host_name, app->host_name);
      tlib_pass_if_str_equal("new app", info.trace_observer_host,
                             app->info.trace_observer_host);
      nrt_mutex_unlock(&app->app_lock);
    }
  }

  /*
   * Adding app to full applist fails
   */
  info.appname = appname;
  info.license = license;
  app = nr_app_find_or_add_app(applist, &info);
  tlib_pass_if_null("full applist", app);

  /*
   * Adding an app with a different trace observer fails, since it's a "new"
   * app.
   */
  info.trace_observer_host = NULL;
  tlib_pass_if_null("full, non-matching app",
                    nr_app_find_or_add_app(applist, &info));

  /*
   * Find those apps
   */
  for (i = 0; i < NR_APP_LIMIT; i++) {
    char entity_name[64];
    char app_name[80];
    char lic[64];

    snprintf(lic, 41, "12345%05d000000000000000000000000006789", i);
    snprintf(entity_name, 64, "appname%d", i);
    snprintf(app_name, 80, "%s;OtherApp", entity_name);

    info.appname = app_name;
    info.license = lic;
    info.trace_observer_host = trace_observer_host;

    app = nr_app_find_or_add_app(applist, &info);

    tlib_pass_if_not_null("find app", app);
    if (0 != app) {
      tlib_pass_if_str_equal("new app", app_name, app->info.appname);
      tlib_pass_if_str_equal("new app", entity_name, app->entity_name);
      tlib_pass_if_str_equal("find app", lic, app->info.license);
      tlib_pass_if_str_equal("find app", info.version, app->info.version);
      tlib_pass_if_str_equal("find app", info.lang, app->info.lang);
      tlib_pass_if_str_equal("find app", NR_EXPECTED_PRINTABLE_LICENSE,
                             app->plicense);
      tlib_pass_if_int_equal("find app", (int)NR_APP_UNKNOWN, (int)app->state);
      test_obj_as_json("find app", app->info.settings, "[\"my_settings\"]");
      test_obj_as_json("find app", app->info.environment,
                       "[\"my_environment\"]");
      test_obj_as_json("find app", app->info.labels, TEST_LABELS_JSON);
      test_obj_as_json("find app", app->info.metadata, TEST_METADATA_JSON);
      tlib_pass_if_str_equal("find app", info.host_display_name,
                             app->info.host_display_name);
      tlib_pass_if_str_equal("new app", info.redirect_collector,
                             app->info.redirect_collector);
      tlib_pass_if_str_equal("new app", system_host_name, app->host_name);
      tlib_pass_if_str_equal("new app", info.trace_observer_host,
                             app->info.trace_observer_host);
      nrt_mutex_unlock(&app->app_lock);
    }
  }

  info.appname = appname;
  info.license = license;
  appname = NULL;
  license = NULL;
  nr_free(system_host_name);
  nr_app_info_destroy_fields(&info);
  nr_applist_destroy(&applist);
}

static void test_find_or_add_app_high_security_mismatch(void) {
  nrapp_t* app;
  nr_app_info_t info;
  nrapplist_t* applist = nr_applist_create();

  nr_memset(&info, 0, sizeof(info));
  info.license = nr_strdup("1234500000000000000000000000000000006789");
  info.version = nr_strdup("my_version");
  info.lang = nr_strdup("my_language");
  info.appname = nr_strdup("test-app");
  info.settings = nro_create_from_json("[\"my_settings\"]");
  info.environment = nro_create_from_json("[\"my_environment\"]");
  info.labels = nro_create_from_json(TEST_LABELS_JSON);
  info.metadata = nro_create_from_json(TEST_METADATA_JSON);
  info.high_security = 0;
  info.redirect_collector = nr_strdup("collector.newrelic.com");

  tlib_pass_if_int_equal("applist starts empty", 0, applist->num_apps);

  /*
   * Add the app without high security.
   */
  app = nr_app_find_or_add_app(applist, &info);
  tlib_pass_if_not_null("app added", app);
  if (app) {
    tlib_pass_if_int_equal("app has high security off", 0,
                           app->info.high_security);
    nrt_mutex_unlock(&app->app_lock);
  }

  /*
   * Find the same app without high security.
   */
  app = nr_app_find_or_add_app(applist, &info);
  tlib_pass_if_not_null("app found", app);
  if (app) {
    tlib_pass_if_int_equal("app has high security off", 0,
                           app->info.high_security);
    nrt_mutex_unlock(&app->app_lock);
  }

  /*
   * Looking for the same app with high security on fails.
   */
  info.high_security = 1;
  app = nr_app_find_or_add_app(applist, &info);
  tlib_pass_if_null("app added", app);

  nr_applist_destroy(&applist);

  applist = nr_applist_create();

  /*
   * Perform the same tests, but this time with high security being true on the
   * app that was first added.
   */
  info.high_security = 1;
  app = nr_app_find_or_add_app(applist, &info);
  tlib_pass_if_not_null("app added", app);
  if (app) {
    tlib_pass_if_int_equal("app has high security on", 1,
                           app->info.high_security);
    nrt_mutex_unlock(&app->app_lock);
  }
  app = nr_app_find_or_add_app(applist, &info);
  tlib_pass_if_not_null("app found", app);
  if (app) {
    tlib_pass_if_int_equal("app has high security on", 1,
                           app->info.high_security);
    nrt_mutex_unlock(&app->app_lock);
  }

  info.high_security = 0;
  app = nr_app_find_or_add_app(applist, &info);
  tlib_pass_if_null("app added", app);

  nr_applist_destroy(&applist);
  nr_app_info_destroy_fields(&info);
}

/*
 * This global variable allows us to control the app state
 * set by the local nr_cmd_appinfo_tx mock function.
 */
nrapptype_t nr_cmd_appinfo_tx_state = NR_APP_OK;

nr_status_t nr_cmd_appinfo_tx(int daemon_fd NRUNUSED, nrapp_t* app) {
  test_app_state_t* p = (test_app_state_t*)tlib_getspecific();

  p->cmd_appinfo_called += 1;

  if (p->last_daemon_query_reset) {
    app->last_daemon_query = 0;
  }

  if (p->cmd_appinfo_succeed) {
    app->state = (int)nr_cmd_appinfo_tx_state;
    return NR_SUCCESS;
  }

  return NR_FAILURE;
}

static void test_agent_should_do_app_daemon_query(void) {
  int do_query;
  nrapp_t app;
  time_t now = time(0);

  do_query = nr_agent_should_do_app_daemon_query(0, now);
  tlib_pass_if_int_equal("null app", 0, do_query);

  /*
   * Test : Application Unknown
   */
  app.state = NR_APP_UNKNOWN;
  app.failed_daemon_query_count = 0;
  app.last_daemon_query = now - (NR_APP_UNKNOWN_QUERY_BACKOFF_SECONDS - 1);
  do_query = nr_agent_should_do_app_daemon_query(&app, now);
  tlib_pass_if_int_equal("app unknown no failed queries do query", 1, do_query);

  app.state = NR_APP_UNKNOWN;
  app.failed_daemon_query_count = 0;
  app.last_daemon_query = now - (NR_APP_UNKNOWN_QUERY_BACKOFF_SECONDS + 1);
  do_query = nr_agent_should_do_app_daemon_query(&app, now);
  tlib_pass_if_int_equal("app unknown no failed queries do query", 1, do_query);

  app.state = NR_APP_UNKNOWN;
  app.failed_daemon_query_count = 999;
  app.last_daemon_query
      = now - (NR_APP_UNKNOWN_QUERY_BACKOFF_LIMIT_SECONDS - 1);
  do_query = nr_agent_should_do_app_daemon_query(&app, now);
  tlib_pass_if_int_equal("app unknown max backoff too soon", 0, do_query);

  app.state = NR_APP_UNKNOWN;
  app.failed_daemon_query_count = 999;
  app.last_daemon_query
      = now - (NR_APP_UNKNOWN_QUERY_BACKOFF_LIMIT_SECONDS + 1);
  do_query = nr_agent_should_do_app_daemon_query(&app, now);
  tlib_pass_if_int_equal("app unknown max backoff do query", 1, do_query);

  /*
   * Test : Application OK
   */
  app.state = NR_APP_OK;
  app.last_daemon_query = now - (NR_APP_REFRESH_QUERY_PERIOD_SECONDS - 1);
  do_query = nr_agent_should_do_app_daemon_query(&app, now);
  tlib_pass_if_int_equal("app ok too soon", 0, do_query);

  app.state = NR_APP_OK;
  app.last_daemon_query = now - (NR_APP_REFRESH_QUERY_PERIOD_SECONDS + 1);
  do_query = nr_agent_should_do_app_daemon_query(&app, now);
  tlib_pass_if_int_equal("app ok do query", 1, do_query);

  app.state = NR_APP_INVALID;
  app.last_daemon_query = now - (NR_APP_REFRESH_QUERY_PERIOD_SECONDS + 1);
  do_query = nr_agent_should_do_app_daemon_query(&app, now);
  tlib_pass_if_int_equal("invalid app", 0, do_query);
}

static void test_agent_find_or_add_app(void) {
  test_app_state_t* p = (test_app_state_t*)tlib_getspecific();
  nrapp_t* app;
  nr_app_info_t info;
  nrapplist_t* applist = nr_applist_create();
  nrapptype_t original_state;
  char* system_host_name = nr_system_get_hostname();

  nr_memset(&info, 0, sizeof(info));
  info.version = nr_strdup("my_version");
  info.lang = nr_strdup("my_language");
  info.license = nr_strdup("1234500000000000000000000000000000006789");
  info.appname = nr_strdup("my_appname");
  info.settings = NULL;
  info.environment = nro_create_from_json("[\"my_environment\"]");
  info.labels = nro_create_from_json(TEST_LABELS_JSON);
  info.metadata = nro_create_from_json(TEST_METADATA_JSON);
  info.high_security = 555;
  info.redirect_collector = nr_strdup("collector.newrelic.com");
  info.security_policies_token = nr_strdup("");

  tlib_pass_if_int_equal("applist starts empty", 0, applist->num_apps);

  /*
   * Test : Bad Parameters
   */
  app = nr_agent_find_or_add_app(NULL, NULL, settings_callback_fn, 0);
  tlib_pass_if_null("zero params", app)
      tlib_pass_if_int_equal("zero params", 0, applist->num_apps);

  app = nr_agent_find_or_add_app(applist, NULL, settings_callback_fn, 0);
  tlib_pass_if_null("NULL info", app)
      tlib_pass_if_int_equal("NULL info", 0, applist->num_apps);

  app = nr_agent_find_or_add_app(NULL, &info, settings_callback_fn, 0);
  tlib_pass_if_null("NULL applist", app)
      tlib_pass_if_int_equal("NULL applist", 0, applist->num_apps);

  /*
   * Test : Application added, queried, but unknown and not returned
   */
  p->cmd_appinfo_succeed = false;
  p->cmd_appinfo_called = 0;
  app = nr_agent_find_or_add_app(applist, &info, NULL, 0);
  tlib_pass_if_null("new app", app);
  tlib_pass_if_int_equal("new app", 1, applist->num_apps);
  tlib_pass_if_int_equal("new app", 1, p->cmd_appinfo_called);
  app = applist->apps[0];
  tlib_pass_if_not_null("new app", app);
  if (0 != app) {
    tlib_pass_if_int_equal("new app", info.high_security,
                           app->info.high_security);
    tlib_pass_if_str_equal("new app", info.appname, app->info.appname);
    tlib_pass_if_str_equal("new app", info.license, app->info.license);
    tlib_pass_if_str_equal("new app", info.version, app->info.version);
    tlib_pass_if_str_equal("new app", info.lang, app->info.lang);
    tlib_pass_if_str_equal("new app", NR_EXPECTED_PRINTABLE_LICENSE,
                           app->plicense);
    tlib_pass_if_int_equal("new app", (int)NR_APP_UNKNOWN, (int)app->state);
    tlib_pass_if_null("new app", app->info.settings);
    test_obj_as_json("new app", app->info.environment, "[\"my_environment\"]");
    test_obj_as_json("new app", app->info.labels, TEST_LABELS_JSON);
    test_obj_as_json("new app", app->info.metadata, TEST_METADATA_JSON);
    tlib_pass_if_str_equal("new app", info.redirect_collector,
                           app->info.redirect_collector);
    tlib_pass_if_str_equal("new app", system_host_name, app->host_name);

    /* No unlock here because the app actually came in unlocked from the
     * applist. */
  }

  /*
   * Test : Same app, but no cmd appinfo, since it is too soon. Settings added.
   */
  p->cmd_appinfo_succeed = false;
  p->cmd_appinfo_called = 0;
  app = nr_agent_find_or_add_app(applist, &info, settings_callback_fn, 0);
  tlib_pass_if_null("find app no appinfo", app);
  tlib_pass_if_int_equal("find app no appinfo", 1, applist->num_apps);
  tlib_pass_if_int_equal("find app no appinfo", 0, p->cmd_appinfo_called);
  app = applist->apps[0];
  if (0 != app) {
    tlib_pass_if_int_equal("find app no appinfo", 1,
                           app->failed_daemon_query_count);
    test_obj_as_json("settings added from callback", app->info.settings,
                     "[\"my_settings\"]");

    /* No unlock here because the app actually came in unlocked from the
     * applist. */
  }

  /*
   * Test : No multiple appinfo calls on failure, despite timeout
   */

  original_state = nr_cmd_appinfo_tx_state;
  nr_cmd_appinfo_tx_state = NR_APP_INVALID;

  p->cmd_appinfo_succeed = true;
  p->cmd_appinfo_called = 0;
  p->last_daemon_query_reset = true;
  app = applist->apps[0];
  if (0 != app) {
    app->last_daemon_query = 0;
    app->failed_daemon_query_count = 0;
  }
  app = nr_agent_find_or_add_app(applist, &info, settings_callback_fn,
                                 100 * NR_TIME_DIVISOR_MS);
  tlib_pass_if_not_null("no multiple calls on invalid app", app);
  tlib_pass_if_int_equal("no multiple calls on invalid app",
                         p->cmd_appinfo_called, 1);

  nr_cmd_appinfo_tx_state = original_state;

  /*
   * Test : Timeout enforces multiple appinfo calls
   */
  nr_free(info.appname);
  info.appname = nr_strdup("appname_multiple_calls");
  p->cmd_appinfo_succeed = false;
  p->cmd_appinfo_called = 0;
  p->last_daemon_query_reset = true;
  app = nr_agent_find_or_add_app(applist, &info, settings_callback_fn,
                                 100 * NR_TIME_DIVISOR_MS);
  tlib_pass_if_null("fail after timeout", app);
  tlib_pass_if_true("fail after timeout", p->cmd_appinfo_called > 1,
                    "multiple appinfo calls expected, got %d",
                    p->cmd_appinfo_called);

  /*
   * Test : Command appinfo succeeds
   */
  p->cmd_appinfo_succeed = true;
  p->cmd_appinfo_called = 0;
  app = applist->apps[1];
  if (0 != app) {
    app->last_daemon_query = 0;
    app->failed_daemon_query_count = 1;
  }
  app = nr_agent_find_or_add_app(applist, &info, settings_callback_fn, 0);
  tlib_pass_if_not_null("app with appinfo", app);
  tlib_pass_if_int_equal("app with appinfo", 2, applist->num_apps);
  tlib_pass_if_int_equal("app with appinfo", 1, p->cmd_appinfo_called);
  if (0 != app) {
    tlib_pass_if_int_equal("app with appinfo", (int)NR_APP_OK, (int)app->state);
    tlib_pass_if_int_equal("app with appinfo", 0,
                           app->failed_daemon_query_count);
    nrt_mutex_unlock(&app->app_lock);
  }

  /*
   * Test : New app, but null labels
   */
  p->cmd_appinfo_succeed = false;
  p->cmd_appinfo_called = 0;
  nr_free(info.appname);
  info.appname = nr_strdup("appname_null_labels");
  nro_delete(info.labels);
  app = nr_agent_find_or_add_app(applist, &info, settings_callback_fn, 0);
  tlib_pass_if_null("new app NULL labels", app);
  tlib_pass_if_int_equal("new app NULL labels", 3, applist->num_apps);
  tlib_pass_if_int_equal("new app NULL labels", 1, p->cmd_appinfo_called);
  app = applist->apps[2];
  tlib_pass_if_not_null("new app NULL labels", app);
  if (0 != app) {
    test_obj_as_json("new app NULL labels", app->info.labels, "null");

    /* No unlock here because the app actually came in unlocked from the
     * applist. */
  }

  /*
   * Test : New app, but null metadata
   */
  p->cmd_appinfo_succeed = false;
  p->cmd_appinfo_called = 0;
  nr_free(info.appname);
  info.appname = nr_strdup("appname_null_metadata");
  nro_delete(info.metadata);
  app = nr_agent_find_or_add_app(applist, &info, settings_callback_fn, 0);
  tlib_pass_if_null("new app NULL metadata", app);
  tlib_pass_if_int_equal("new app NULL metadata", 4, applist->num_apps);
  tlib_pass_if_int_equal("new app NULL metadata", 1, p->cmd_appinfo_called);
  app = applist->apps[3];
  tlib_pass_if_not_null("new app NULL metadata", app);
  if (0 != app) {
    test_obj_as_json("new app NULL metadata", app->info.metadata, "null");

    /* No unlock here because the app actually came in unlocked from the
     * applist. */
  }

  /*
   * Test : New app, but empty metadata
   */
  p->cmd_appinfo_succeed = false;
  p->cmd_appinfo_called = 0;
  nr_free(info.appname);
  info.appname = nr_strdup("appname_empty_metadata");
  nro_delete(info.metadata);
  info.metadata = nro_create_from_json("{}");
  app = nr_agent_find_or_add_app(applist, &info, settings_callback_fn, 0);
  tlib_pass_if_null("new app empty metadata", app);
  tlib_pass_if_int_equal("new app empty metadata", 5, applist->num_apps);
  tlib_pass_if_int_equal("new app empty metadata", 1, p->cmd_appinfo_called);
  app = applist->apps[4];
  tlib_pass_if_not_null("new app empty metadata", app);
  if (0 != app) {
    test_obj_as_json("new app empty metadata", app->info.metadata, "{}");

    /* No unlock here because the app actually came in unlocked from the
     * applist. */
  }

  /*
   * Test : Unable to add application due to full applist.
   */
  p->cmd_appinfo_succeed = false;
  p->cmd_appinfo_called = 0;
  applist->num_apps = NR_APP_LIMIT;
  nr_free(info.appname);
  info.appname = nr_strdup("other_appname");
  app = nr_agent_find_or_add_app(applist, &info, settings_callback_fn, 0);
  tlib_pass_if_null("full applist", app);
  tlib_pass_if_int_equal("full applist", 0, p->cmd_appinfo_called);

  /*
   * Test : HSM and Language Agent Security Policy (LASP) are both set.
   * First try to add app when both HSM and LASP are set, expect
   * failure. Then turn off HSM and try again, expecting success.
   */
  p->cmd_appinfo_succeed = true;
  p->cmd_appinfo_called = 0;
  applist->num_apps = 5;
  nr_free(info.appname);
  info.appname = nr_strdup("appname_security");
  info.high_security = 1;
  nr_free(info.security_policies_token);
  info.security_policies_token = nr_strdup("any_token");
  app = nr_agent_find_or_add_app(applist, &info, settings_callback_fn, 0);
  tlib_pass_if_null("new app test HSM and LASP", app);
  tlib_pass_if_int_equal("new app test HSM and LASP", 0, p->cmd_appinfo_called);
  // Turn HSM off and try again, expecting a success
  info.high_security = 0;
  app = nr_agent_find_or_add_app(applist, &info, settings_callback_fn, 0);
  tlib_pass_if_not_null("new app test HSM and LASP", app);
  tlib_pass_if_int_equal("new app test HSM and LASP", 1, p->cmd_appinfo_called);
  tlib_pass_if_int_equal("new app test HSM and LASP", app->info.high_security,
                         0);
  tlib_pass_if_str_equal("new app test HSM and LASP",
                         app->info.security_policies_token, "any_token");
  tlib_pass_if_str_equal("new app test HSM and LASP", system_host_name,
                         app->host_name);

  nrt_mutex_unlock(&app->app_lock);

  nr_free(info.appname);
  nr_free(info.security_policies_token);
  nr_free(system_host_name);
  nr_app_info_destroy_fields(&info);
  nr_applist_destroy(&applist);
}

static void test_verify_id(void) {
  nrapp_t* app;
  nrapplist_t* applist = nr_applist_create();
  nr_app_info_t info;

  nr_memset(&info, 0, sizeof(info));
  info.version = nr_strdup("my_version");
  info.lang = nr_strdup("my_language");
  info.license = nr_strdup("1234500000000000000000000000000000006789");
  info.appname = nr_strdup("my_appname");
  info.settings = NULL;
  info.environment = nro_create_from_json("[\"my_environment\"]");
  info.labels = nro_create_from_json(TEST_LABELS_JSON);
  info.metadata = nro_create_from_json(TEST_METADATA_JSON);
  info.high_security = 0;
  info.redirect_collector = nr_strdup("collector.newrelic.com");

  tlib_pass_if_int_equal("applist starts empty", 0, applist->num_apps);

  app = nr_app_verify_id(applist, TEST_AGENT_RUN_ID);
  tlib_pass_if_null("empty applist", app);

  /*
   * Add an app and connect it.
   */
  app = nr_agent_find_or_add_app(applist, &info, settings_callback_fn, 0);
  tlib_fail_if_null("new app", app);
  applist->apps[0]->state = NR_APP_OK;
  applist->apps[0]->agent_run_id = nr_strdup(TEST_AGENT_RUN_ID);
  nrt_mutex_unlock(&app->app_lock);

  app = nr_app_verify_id(applist, NULL);
  tlib_pass_if_null("zero agent run id", app);

  app = nr_app_verify_id(applist, "foo" TEST_AGENT_RUN_ID);
  tlib_pass_if_null("wrong run id", app);

  applist->apps[0]->state = NR_APP_UNKNOWN;
  app = nr_app_verify_id(applist, TEST_AGENT_RUN_ID);
  tlib_pass_if_null("app not ok", app);
  applist->apps[0]->state = NR_APP_OK;

  app = nr_app_verify_id(0, TEST_AGENT_RUN_ID);
  tlib_pass_if_null("null applist", app);

  app = nr_app_verify_id(applist, TEST_AGENT_RUN_ID);
  tlib_pass_if_not_null("verify daemon id success", app);
  if (app) {
    nrt_mutex_unlock(&app->app_lock);
  }

  /* Do it again to ensure no locking problems */
  app = nr_app_verify_id(applist, TEST_AGENT_RUN_ID);
  tlib_pass_if_not_null("verify daemon id success", app);
  if (app) {
    nrt_mutex_unlock(&app->app_lock);
  }

  nr_applist_destroy(&applist);
  nr_app_info_destroy_fields(&info);
}

// clang-format off

static void test_app_consider_appinfo_first_txn(void) {
  test_app_state_t* p = (test_app_state_t*)tlib_getspecific();
  nrapp_t app = {0};
  time_t now = time(0);
  
  // first transaction with uninitialized app should query appinfo
  app.state = NR_APP_UNKNOWN;
  app.failed_daemon_query_count = 0; // as set by create_new_app()
  app.last_daemon_query = 0; // as set by create_new_app()
  nr_memset(p, 0, sizeof(test_app_state_t));
  p->cmd_appinfo_succeed = false; // first appinfo query fails
  nr_app_consider_appinfo(&app, now);

  tlib_pass_if_true("first transaction with uninitialized app should query appinfo", 1 == p->cmd_appinfo_called,
    "Expected cmd_appinfo_called to be 1, but it was %d", p->cmd_appinfo_called);
  tlib_pass_if_true("first transaction with uninitialized app should query appinfo", now == app.last_daemon_query,
    "Expected last_daemon_query to be updated, but it was not");
  tlib_pass_if_true("first transaction with uninitialized app should query appinfo", 1 == app.failed_daemon_query_count,
    "Expected failed_daemon_query_count to be 1, but it was %d", app.failed_daemon_query_count);
}

static void test_app_consider_appinfo_backoff(void) {
  test_app_state_t* p = (test_app_state_t*)tlib_getspecific();
  nrapp_t app = {0};
  time_t now = time(0);

  for (int failed_daemon_query_count = 1; failed_daemon_query_count < 10; failed_daemon_query_count++) {
    for (int time_since_last_query = 0; time_since_last_query <= failed_daemon_query_count * NR_APP_UNKNOWN_QUERY_BACKOFF_SECONDS + 1; time_since_last_query++) {
      // Setup app state before each test case
      app.state = NR_APP_UNKNOWN;
      app.failed_daemon_query_count = failed_daemon_query_count;
      app.last_daemon_query = now - time_since_last_query;
      // Setup mock state
      nr_memset(p, 0, sizeof(test_app_state_t));
      p->cmd_appinfo_succeed = false;
      // Simulate appinfo consideration at the specified query time
      nr_app_consider_appinfo(&app, now);
      if (time_since_last_query >= failed_daemon_query_count * NR_APP_UNKNOWN_QUERY_BACKOFF_SECONDS ||
          time_since_last_query >= NR_APP_UNKNOWN_QUERY_BACKOFF_LIMIT_SECONDS) {
        tlib_pass_if_true("expected to query appinfo",
          1 == p->cmd_appinfo_called,
          "failed_daemon_query_count=%d, now=%ld, last_query=%ld, expected cmd_appinfo_called to be 1, but it was %d", 
          failed_daemon_query_count, now, now - time_since_last_query, p->cmd_appinfo_called);
        tlib_pass_if_true("expected to query appinfo",
          now == app.last_daemon_query,
          "failed_daemon_query_count=%d, now=%ld, last_query=%ld, expected last_daemon_query to be %ld, but it was %ld", 
          failed_daemon_query_count, now, now - time_since_last_query, now, app.last_daemon_query);
        tlib_pass_if_true("expected to query appinfo",
          failed_daemon_query_count + 1 == app.failed_daemon_query_count,
          "failed_daemon_query_count=%d, now=%ld, last_query=%ld, expected failed_daemon_query_count to be %d, but it was %d", 
          failed_daemon_query_count, now, now - time_since_last_query, failed_daemon_query_count + 1, app.failed_daemon_query_count);
      } else {
        tlib_pass_if_true("expected NOT to query appinfo",
          0 == p->cmd_appinfo_called,
          "failed_daemon_query_count=%d, now=%ld, last_query=%ld, expected cmd_appinfo_called to be 0, but it was %d", 
          failed_daemon_query_count, now, now - time_since_last_query, p->cmd_appinfo_called);
        tlib_pass_if_true("expected NOT to query appinfo",
          now - time_since_last_query == app.last_daemon_query,
          "failed_daemon_query_count=%d, now=%ld, last_query=%ld, expected last_daemon_query to be %ld, but it was %ld", 
          failed_daemon_query_count, now, now - time_since_last_query, now - time_since_last_query, app.last_daemon_query);
        tlib_pass_if_true("expected NOT to query appinfo",
          failed_daemon_query_count == app.failed_daemon_query_count,
          "failed_daemon_query_count=%d, now=%ld, last_query=%ld, expected failed_daemon_query_count to be %d, but it was %d", 
          failed_daemon_query_count, now, now - time_since_last_query, failed_daemon_query_count, app.failed_daemon_query_count);
      }
    }
  }
}

static void test_app_consider_appinfo_refresh(void) {
  test_app_state_t* p = (test_app_state_t*)tlib_getspecific();
  nrapp_t app = {0};
  time_t last_daemon_query = time(0);
  struct test_case {
    const char* description;
    time_t elapsed_time;
    int cmd_appinfo_called;
  } test_cases[] = {
    {"too soon should not query appinfo", NR_APP_REFRESH_QUERY_PERIOD_SECONDS - 1, 0},
    {"right on time should query appinfo", NR_APP_REFRESH_QUERY_PERIOD_SECONDS, 1},
    {"enough time should query appinfo", NR_APP_REFRESH_QUERY_PERIOD_SECONDS + 1, 1}
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
    struct test_case* tc = &test_cases[i];
    time_t query_time = last_daemon_query + tc->elapsed_time;

    // Set app state before each test case
    app.state = NR_APP_OK;
    app.last_daemon_query = last_daemon_query;

    // Simulate appinfo consideration at the specified query time
    nr_memset(p, 0, sizeof(test_app_state_t));
    p->cmd_appinfo_succeed = true;
    nr_app_consider_appinfo(&app, query_time);

    tlib_pass_if_int_equal(tc->description, tc->cmd_appinfo_called, p->cmd_appinfo_called);
    if (tc->cmd_appinfo_called) {
      tlib_pass_if_true(tc->description, query_time == app.last_daemon_query,
        "Expected last_daemon_query to be updated, but it was not");
    } else {
      tlib_pass_if_true(tc->description, last_daemon_query == app.last_daemon_query,
        "Expected last_daemon_query to remain unchanged, but it was updated");
    }
  }
}

// clang-format on

static void test_app_consider_appinfo(void) {
  nrapp_t app;
  time_t now = time(0);
  // null checks
  tlib_pass_if_false("nr_app_consider_appinfo: null check",
                     nr_app_consider_appinfo(NULL, time(0)),
                     "Expected false, got true");

  app.state = NR_APP_OK;
  app.failed_daemon_query_count = 0;
  app.last_daemon_query = (int)now - 1;
  tlib_pass_if_false("nr_app_consider_appinfo: one second ago",
                     nr_app_consider_appinfo(&app, now),
                     "Expected false, got true");

  app.state = NR_APP_OK;
  app.failed_daemon_query_count = 0;
  app.last_daemon_query = (int)now - 60;
  tlib_pass_if_true("nr_app_consider_appinfo: one minute ago",
                    nr_app_consider_appinfo(&app, now),
                    "Expected true, got false");

  tlib_pass_if_equal("nr_app_consider_appinfo: state", NR_APP_OK, app.state,
                     nrapptype_t, "%d");

  tlib_pass_if_int_equal("nr_app_consider_appinfo: failed_daemon_query_count",
                         app.failed_daemon_query_count, 0);

  // If the real time clock (RTC) was adjusted by hand
  // and the time's far enough in the future, appinfo updates
  app.state = NR_APP_OK;
  app.last_daemon_query = (int)now + 60;
  app.failed_daemon_query_count = 0;
  tlib_pass_if_true("nr_app_consider_appinfo: one minute in to the future",
                    nr_app_consider_appinfo(&app, now),
                    "Expected true, got false");

  tlib_pass_if_equal("nr_app_consider_appinfo: state", NR_APP_OK, app.state,
                     nrapptype_t, "%d");

  tlib_pass_if_int_equal("nr_app_consider_appinfo: failed_daemon_query_count",
                         app.failed_daemon_query_count, 0);

  tlib_pass_if_true("nr_app_consider_appinfo: last_daemon_query",
                    app.last_daemon_query < (int)now + 60,
                    "Expected updated last_daemon_query");

  // If the real time clock (RTC) was adjusted by hand
  // and the time's NOT far enough in the future, appinfo does not update
  app.state = NR_APP_OK;
  app.last_daemon_query = (int)now + NR_APP_REFRESH_QUERY_PERIOD_SECONDS - 1;
  app.failed_daemon_query_count = 0;
  tlib_pass_if_false("nr_app_consider_appinfo: one minute in to the future",
                     nr_app_consider_appinfo(&app, now),
                     "Expected true, got false");

  tlib_pass_if_equal("nr_app_consider_appinfo: state", NR_APP_OK, app.state,
                     nrapptype_t, "%d");

  tlib_pass_if_int_equal("nr_app_consider_appinfo: failed_daemon_query_count",
                         app.failed_daemon_query_count, 0);
}

static void test_app_consider_appinfo_failure(void) {
  nrapp_t app;
  time_t now = time(0);
  nrapptype_t original_state;

  // grab the original return value of the mocked nr_cmd_appinfo
  original_state = nr_cmd_appinfo_tx_state;

  // mock the status we want nr_cmd_appinfo to return
  nr_cmd_appinfo_tx_state = NR_APP_UNKNOWN;

  // tests that failed_daemon_query_count is set on a failure
  app.state = NR_APP_OK;
  app.last_daemon_query = (int)now - 60;
  app.failed_daemon_query_count = 0;

  tlib_pass_if_true("nr_app_consider_appinfo: one minute ago",
                    nr_app_consider_appinfo(&app, now),
                    "Expected true, got false");

  tlib_pass_if_equal("nr_app_consider_appinfo: state", NR_APP_UNKNOWN,
                     app.state, nrapptype_t, "%d");

  tlib_pass_if_int_equal("nr_app_consider_appinfo: failed_daemon_query_count ",
                         app.failed_daemon_query_count, 1);

  // tests that failed_daemon_query_count is _incremented_ on a failure
  app.state = NR_APP_OK;
  app.last_daemon_query = (int)now - 60;
  app.failed_daemon_query_count = 1;
  tlib_pass_if_true("nr_app_consider_appinfo: one minute ago",
                    nr_app_consider_appinfo(&app, now),
                    "Expected true, got false");

  tlib_pass_if_equal("nr_app_consider_appinfo: state", NR_APP_UNKNOWN,
                     app.state, nrapptype_t, "%d");

  tlib_pass_if_int_equal("nr_app_consider_appinfo: failed_daemon_query_count ",
                         app.failed_daemon_query_count, 2);

  // restore the original return value of the mocked nr_cmd_appinfo
  nr_cmd_appinfo_tx_state = original_state;
}

static void test_get_primary_app_name(void) {
  char* result;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL appname", nr_app_get_primary_app_name(NULL));
  tlib_pass_if_null("empty appname", nr_app_get_primary_app_name(""));

  /*
   * Test : No rollup.
   */
  result = nr_app_get_primary_app_name("App Name");
  tlib_pass_if_str_equal("no rollup", "App Name", result);
  nr_free(result);

  /*
   * Test : Rollup.
   */
  result = nr_app_get_primary_app_name("App Name;Foo;Bar");
  tlib_pass_if_str_equal("rollup", "App Name", result);
  nr_free(result);
}

static void test_app_entity_type_get(void) {
  nrapp_t app;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL app", nr_app_get_entity_type(NULL));

  /*
   * Test : Constant string "SERVICE" returned
   */
  tlib_pass_if_str_equal("static entity type", "SERVICE",
                         nr_app_get_entity_type(&app));
}

static void test_app_entity_name_get(void) {
  nrapp_t app;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL app", nr_app_get_entity_name(NULL));

  /*
   * Test : Entity name (primary app name) returned.
   *
   * Correct initialization of entity_name is tested in
   * test_find_or_add_app.
   */
  app.entity_name = "A";
  tlib_pass_if_str_equal("entity name", "A", nr_app_get_entity_name(&app));
}

static void test_app_host_name_get(void) {
  nrapp_t app;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL app", nr_app_get_host_name(NULL));

  /*
   * Test : Host name returned
   *
   * Correct initialization of host_name is tested in
   * test_find_or_add_app.
   */
  app.host_name = "host.com";
  tlib_pass_if_str_equal("entity name", "host.com", nr_app_get_host_name(&app));
}

static void test_app_entity_guid_get(void) {
  nrapp_t app;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL app", nr_app_get_entity_guid(NULL));

  /*
   * Test : Entity guid returned
   *
   * Correct initialization of the entity guid is tested in
   * test_cmd_appinfo.c/test_process_connected_app.
   */
  app.entity_guid = "00112233445566778899aa";
  tlib_pass_if_str_equal("entity name", "00112233445566778899aa",
                         nr_app_get_entity_guid(&app));
}

static void test_get_or_create_thread_harvest(void) {
  nrapp_t app = {0};
  nrapp_t empty = {0};
  nr_app_harvest_stats_t* h1;
  nr_app_harvest_stats_t* h2;

  nrt_mutex_init(&app.app_lock, 0);
  app.harvest_map
      = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_app_harvest_stats_dtor);
  nr_app_update_harvest_config(&app, 1000 * NR_TIME_DIVISOR,
                               60 * NR_TIME_DIVISOR, 10);

  /* NULL app returns NULL. */
  tlib_pass_if_null("NULL app", nr_app_get_or_create_thread_harvest(NULL, 1));

  /* NULL harvest_map returns NULL. */
  tlib_pass_if_null("NULL harvest_map",
                    nr_app_get_or_create_thread_harvest(&empty, 1));

  /* First call for key 1 creates an entry. */
  h1 = nr_app_get_or_create_thread_harvest(&app, 1);
  tlib_pass_if_not_null("first call returns non-NULL", h1);

  /* Stats start zeroed and next_harvest set (non-zero means init ran). */
  tlib_pass_if_true("next_harvest initialised", h1->next_harvest > 0,
                    "next_harvest=%" PRIu64, h1->next_harvest);

  /* Stats counters start zeroed. */
  tlib_pass_if_uint64_t_equal("transactions_seen zeroed", 0,
                              h1->transactions_seen);
  tlib_pass_if_uint64_t_equal("transactions_sampled zeroed", 0,
                              h1->transactions_sampled);

  /* Second call for key 1 returns the same pointer. */
  h2 = nr_app_get_or_create_thread_harvest(&app, 1);
  tlib_pass_if_true("same pointer on second call", h1 == h2,
                    "h1=%p h2=%p", (void*)h1, (void*)h2);

  /* Different key returns a different pointer. */
  h2 = nr_app_get_or_create_thread_harvest(&app, 2);
  tlib_pass_if_not_null("key 2 non-NULL", h2);
  tlib_pass_if_true("different pointer for different key", h1 != h2,
                    "h1=%p h2=%p", (void*)h1, (void*)h2);

  nr_hashmap_destroy(&app.harvest_map);
  nrt_mutex_destroy(&app.app_lock);
}

static void test_sync_harvest_config(void) {
  nrapp_t app = {0};
  nr_app_harvest_stats_t* h1;
  nr_app_harvest_stats_t* h2;

  nrt_mutex_init(&app.app_lock, 0);
  app.harvest_map
      = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_app_harvest_stats_dtor);

  /* Initial config: connect_timestamp=100, frequency=60, target=10. */
  nr_app_update_harvest_config(&app, 100, 60, 10);

  /* Create two per-thread entries and advance their stats. */
  h1 = nr_app_get_or_create_thread_harvest(&app, 1);
  h2 = nr_app_get_or_create_thread_harvest(&app, 2);
  h1->transactions_seen = 5;
  h2->transactions_seen = 7;

  /* NULL app does not crash. */
  nr_app_update_harvest_config(NULL, 100, 60, 10);

  /* Same config — stats should be preserved. */
  nr_app_update_harvest_config(&app, 100, 60, 10);
  tlib_pass_if_uint64_t_equal("key 1 stats preserved after no-change update", 5,
                              h1->transactions_seen);
  tlib_pass_if_uint64_t_equal("key 2 stats preserved after no-change update", 7,
                              h2->transactions_seen);

  /* New connect_timestamp — stats should be reset. */
  nr_app_update_harvest_config(&app, 200, 60, 10);
  tlib_pass_if_uint64_t_equal("key 1 stats reset after connect_timestamp change",
                              0, h1->transactions_seen);
  tlib_pass_if_uint64_t_equal("key 2 stats reset after connect_timestamp change",
                              0, h2->transactions_seen);

  /* Advance counters and zero next_harvest to verify both are reset on the
   * next config change. */
  h1->transactions_seen = 3;
  h2->transactions_seen = 8;
  h1->next_harvest = 0;
  h2->next_harvest = 0;

  /* Frequency-only change — stats reset and next_harvest recalculated. */
  nr_app_update_harvest_config(&app, 200, 30, 10);
  tlib_pass_if_uint64_t_equal("key 1 stats reset after frequency change", 0,
                              h1->transactions_seen);
  tlib_pass_if_uint64_t_equal("key 2 stats reset after frequency change", 0,
                              h2->transactions_seen);
  tlib_pass_if_true("key 1 next_harvest recalculated after frequency change",
                    h1->next_harvest > 0,
                    "next_harvest=%" PRIu64, h1->next_harvest);
  tlib_pass_if_true("key 2 next_harvest recalculated after frequency change",
                    h2->next_harvest > 0,
                    "next_harvest=%" PRIu64, h2->next_harvest);

  nr_hashmap_destroy(&app.harvest_map);
  nrt_mutex_destroy(&app.app_lock);
}

static void test_get_or_create_thread_rnd(void) {
  nrapp_t app = {0};
  nr_random_t* r1;
  nr_random_t* r2;

  /* NULL app returns NULL. */
  tlib_pass_if_null("NULL app", nr_app_get_or_create_thread_rnd(NULL, 1));

  /* NULL rnd_map returns NULL. */
  tlib_pass_if_null("NULL rnd_map",
                    nr_app_get_or_create_thread_rnd(&app, 1));

  app.rnd_map = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_app_rnd_dtor);

  /* First call for key 1 creates a seeded entry. */
  r1 = nr_app_get_or_create_thread_rnd(&app, 1);
  tlib_pass_if_not_null("first call returns non-NULL", r1);

  /* Verify rnd is seeded and functional (not all-zero xsubi). */
  tlib_pass_if_true("rnd produces in-range value",
                    nr_random_range(r1, 1000) < 1000,
                    "rnd=%p", (void*)r1);

  /* Second call for key 1 returns the same pointer (no re-seed). */
  r2 = nr_app_get_or_create_thread_rnd(&app, 1);
  tlib_pass_if_true("same pointer on second call", r1 == r2,
                    "r1=%p r2=%p", (void*)r1, (void*)r2);

  /* Different key returns a different pointer. */
  r2 = nr_app_get_or_create_thread_rnd(&app, 2);
  tlib_pass_if_not_null("key 2 non-NULL", r2);
  tlib_pass_if_true("different pointer for different key", r1 != r2,
                    "r1=%p r2=%p", (void*)r1, (void*)r2);

  nr_hashmap_destroy(&app.rnd_map);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = 4, .state_size = sizeof(test_app_state_t)};

void test_main(void* p NRUNUSED) {
  test_applist_create_destroy();

  test_app_match();
  test_find_or_add_app();
  test_find_or_add_app_high_security_mismatch();
  test_agent_should_do_app_daemon_query();
  test_agent_find_or_add_app();
  test_verify_id();
  test_app_consider_appinfo();
  test_app_consider_appinfo_failure();
  test_get_primary_app_name();
  test_app_entity_name_get();
  test_app_entity_type_get();
  test_app_host_name_get();
  test_app_entity_guid_get();
  test_app_consider_appinfo_first_txn();
  test_app_consider_appinfo_backoff();
  test_app_consider_appinfo_refresh();
  test_get_or_create_thread_harvest();
  test_sync_harvest_config();
  test_get_or_create_thread_rnd();
}
