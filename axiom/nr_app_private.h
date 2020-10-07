/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains internal application functions.
 */
#ifndef NR_APP_PRIVATE_HDR
#define NR_APP_PRIVATE_HDR

#include <sys/types.h>

#include "nr_axiom.h"
#include "nr_app.h"

/*
 * This header file exposes internal functions that are only made visible for
 * unit testing. Other clients are forbidden.
 */

/*
 * Period the agent should query the daemon about unknown applications.
 * If the daemon is unable to connect the application, then we want to avoid
 * frequent daemon queries to avoid impacting performance. At the same time, we
 * want the first two queries to happen quickly so that data collection can
 * occur as soon as possible. To reconcile these two goals, a linear backoff
 * is used.
 *
 * Note that this logic also affects invalid applications: Currently
 * there is no mechanism for the daemon to tell the agent that an application
 * is invalid. Instead it replies valid or unknown.
 */
#define NR_APP_UNKNOWN_QUERY_BACKOFF_SECONDS 2
#define NR_APP_UNKNOWN_QUERY_BACKOFF_LIMIT_SECONDS 10

/*
 * Period the agent should query the daemon about known applications. These
 * 'refresh' queries are done in case application information has changed.
 *
 * If this constant is changed, the matching constant in stressor/main.go
 * should also be changed.
 */
#define NR_APP_REFRESH_QUERY_PERIOD_SECONDS 20

/*
 * These backoff period defines are
 * used to prevent spamming the logs with log messages.
 */
#define NR_LOG_BACKOFF_UNIQUE_FIRST_APPNAME_SECONDS 20
#define NR_LOG_BACKOFF_INVALID_APP_SECONDS 20
#define NR_LOG_BACKOFF_MAX_APPS_SECONDS 20

/*
 * Purpose: White box testing.  Do not use except for unit tests.
 */
extern void nr_app_destroy(nrapp_t** app_ptr);

extern int nr_agent_should_do_app_daemon_query(const nrapp_t* app, time_t now);

extern nrapp_t* nr_app_find_or_add_app(nrapplist_t* applist,
                                       const nr_app_info_t* info);

extern nr_status_t nr_app_match(const nrapp_t* app, const nr_app_info_t* info);

#endif /* NR_APP_PRIVATE_HDR */
