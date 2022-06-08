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
  tlib_pass_if_int_equal("app unknown no failed queries too soon", 0, do_query);

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
  applist->num_apps = 4;
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
}
