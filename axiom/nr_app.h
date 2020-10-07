/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions to manage applications.
 *
 * Every transaction reports data to one application structure. If this
 * application has multiple names (eg "app1;app2;app3") this data may be split
 * across multiple applications within the New Relic backend, but the agent and
 * daemon are oblivious to this. Applications are used in the agent and daemon.
 * This header defines both, as the same structure is used.
 */
#ifndef NR_APP_HDR
#define NR_APP_HDR

#include <stdint.h>
#include <sys/types.h>

#include "nr_app_harvest.h"
#include "nr_rules.h"
#include "nr_segment_terms.h"
#include "util_random.h"
#include "util_threads.h"

/*
 * License size and formatters to print externally visible licenses.
 */
#define NR_LICENSE_SIZE 40
#define NR_PRINTABLE_LICENSE_WINDOW_SIZE 2
#define NR_PRINTABLE_LICENSE_WINDOW \
  "%." NR_STR2(NR_PRINTABLE_LICENSE_WINDOW_SIZE) "s"
#define NR_PRINTABLE_LICENSE_FMT \
  NR_PRINTABLE_LICENSE_WINDOW "..." NR_PRINTABLE_LICENSE_WINDOW
#define NR_PRINTABLE_LICENSE_PREFIX_START 0
#define NR_PRINTABLE_LICENSE_SUFFIX_START \
  (NR_LICENSE_SIZE - NR_PRINTABLE_LICENSE_WINDOW_SIZE)

/*
 * Application Locking
 *
 * At no time should a thread hold a pointer to an unlocked application.
 * Therefore, all app pointer function parameters and return values must be
 * locked. When a thread wants to acquire a locked application, it must use one
 * of the functions below. This is to ensure that no thread tries to lock an
 * app which has been reclaimed. Threads that wish to hold a reference to an
 * unlocked application should instead hold an agent_run_id.
 *
 * NOTE: This app limit should match the daemon's app limit set in limits.go.
 */
#define NR_APP_LIMIT 250

/*
 * The fields in nr_app_info_t come from local configuration.  This is the
 * information which is sent up to the collector during the connect command.
 */
typedef struct _nr_app_info_t {
  int high_security;    /* Indicates if high security been set locally for this
                           application */
  char* license;        /* License key provided */
  nrobj_t* settings;    /* New Relic settings */
  nrobj_t* environment; /* Application environment */
  nrobj_t* labels;      /* Labels for Language Agents */
  char* host_display_name;  /* Optional user-provided host name for UI */
  char* lang;               /* Language */
  char* version;            /* Version */
  char* appname;            /* Application name */
  char* redirect_collector; /* Collector proxy used for redirect command */
  char*
      security_policies_token; /* LASP (Language Agent Security Policy) token */
  nrobj_t*
      supported_security_policies; /* list of supported security policies */
  char* trace_observer_host;       /* 8T trace observer host */
  uint16_t trace_observer_port;    /* 8T trace observer port */
  uint64_t span_queue_size;        /* 8T span queue size (for the daemon) */
} nr_app_info_t;

/*
 * Calculated limits for event types.
 */
typedef struct _nr_app_limits_t {
  int analytics_events;
  int custom_events;
  int error_events;
  int span_events;
} nr_app_limits_t;

typedef struct _nrapp_t {
  nr_app_info_t info;
  nr_random_t* rnd;         /* Random number generator */
  int state;                /* Connection state */
  char* plicense;           /* Printable license (abbreviated for security) */
  char* agent_run_id;       /* The collector's agent run ID; assigned from the
                               New Relic backend */
  char* host_name;          /* Local host name reported to the daemon */
  char* entity_name;        /* Entity name related to this application */
  char* entity_guid;        /* Entity guid related to this application */
  time_t last_daemon_query; /* Used by agent: Last time we queried daemon about
                               this app */
  int failed_daemon_query_count; /* Used by agent: Number of times daemon query
                                    has not returned valid */
  nrrules_t* url_rules; /* From New Relic backend - rules for txn path. Only
                           used by agent. */
  nrrules_t* txn_rules; /* From New Relic backend - rules for full txn metric
                           name. Only used by agent. */
  nr_segment_terms_t*
      segment_terms; /* From New Relic backend - rules for transaction segment
                        terms. Only used by agent. */
  nrobj_t*
      connect_reply; /* From New Relic backend - Full connect command reply */
  nrobj_t* security_policies; /* from Daemon - full security policies map
                                 obtained from Preconnect */
  nrthread_mutex_t app_lock;  /* Serialization lock */
  nr_app_harvest_t harvest;   /* Harvest timing and sampling data */

  /* The limits are set based on the event harvest configuration provided in
   * the connect reply. They do not reflect any agent side configuration. */
  nr_app_limits_t limits;
} nrapp_t;

typedef enum _nrapptype_t {
  NR_APP_INVALID = -1, /* The app has an invalid license key */
  NR_APP_UNKNOWN = 0,  /* The app has not yet been connected to the New Relic
                          backend */
  NR_APP_OK = 1        /* The app is connected and valid */
} nrapptype_t;

typedef struct _nrapplist_t {
  int num_apps;
  nrapp_t** apps;
  nrthread_mutex_t applist_lock;
} nrapplist_t;

/*
 * Purpose : Create a new application list.
 *
 * Returns : A pointer to an unlocked newly allocated app list on success, and
 *           NULL on failure.
 */
extern nrapplist_t* nr_applist_create(void);

/*
 * Purpose : Destroy of the global application list, destroying all of
 *           the applications stored within.
 */
extern void nr_applist_destroy(nrapplist_t** applist_ptr);

/*
 * Purpose : Determine if the given agent run ID refers to a valid
 *           application.
 *
 * Params  : 1. The list of applications unlocked.
 *           2. The agent run ID.
 *
 * Returns : A locked application on success and 0 otherwise.
 *
 * Locking : Returns the application locked, if one is returned.
 *
 * Note    : For this function to return an application, two conditions must be
 *           met:  The agent_run_id must be valid and refer to an application
 *           AND that application must be valid (connected).
 */
extern nrapp_t* nr_app_verify_id(nrapplist_t* applist,
                                 const char* agent_run_id);

/*
 * Purpose : Search for an application within the agent. If the application
 *           does not yet exist within the account, add it, and query the
 *           daemon, which in turn will either return the known application
 *           information (if the daemon previously knew about the application)
 *           or return unknown, and connect the application with the New Relic
 *           backend.
 *
 * Params  : 1. The application list unlocked.
 *           2. The application information.
 *           3. An agent-specific callback function whose purpose it is to
 *              provide the settings hash upon app creation.
 *           4. Specifies the maximum time to wait for a connection to be
 *              established; a value of 0 causes the method to make only one
 *              attempt at connecting to the daemon.
 *
 * Returns : A pointer to the locked valid application if it, or NULL if the
 *           application is unknown or invalid, or if there was any form of
 *           error.
 *
 * Notes   : This function is called by the agent when a transaction starts and
 *           the desired application is known. It may ultimately result in an
 *           APPINFO command being sent to the daemon, which in turn will
 *           either return the known application info from its cache or begin
 *           the process of querying the application from the New Relic backend.
 */
extern nrapp_t* nr_agent_find_or_add_app(nrapplist_t* applist,
                                         const nr_app_info_t* info,
                                         nrobj_t* (*settings_callback_fn)(void),
                                         nrtime_t timeout);

/*
 * Purpose : Create and return a sanitized/obfuscated version of the license
 *           for use in the phpinfo and log files.
 *
 * Params  : 1. The raw license.
 *
 * Returns : A newly allocated string containing the printable license.
 */
extern char* nr_app_create_printable_license(const char* license);

/*
 * Purpose : Free all app info structure fields.
 */
extern void nr_app_info_destroy_fields(nr_app_info_t* info);

/*
 * Purpose : Return the primary app name, given an app name string that may
 *           include rollups.
 *
 * Params  : 1. The app name(s) string.
 *
 * Returns : A newly allocated string containing the primary app name.
 */
extern char* nr_app_get_primary_app_name(const char* appname);

/*
 * Purpose : Decides whether the daemon should be queried for appinfo,
 *           and if so, does the work of querying the daemon.  Function may
 *           change app->state.
 *
 *           Used by agents to ensure they have the latest "state of the world"
 *           from the daemon (has daemon disconnected, etc.)
 *
 * Params  : 1. The application
 *           2. The current time
 *
 * Returns : Returns true is appinfo was queried, false if it was not
 */
bool nr_app_consider_appinfo(nrapp_t* app, time_t now);

/*
 * Purpose : Return the entity name related to the given application.
 *
 * Params  : 1. The application
 *
 * Returns : An entity name. A string bound to the lifetime of the given
 *           application.
 */
extern const char* nr_app_get_entity_name(const nrapp_t* app);

/*
 * Purpose : Return the entity type of the given application.
 *
 *           For agents, this always is the string "SERVICE".
 *
 * Params  : 1. The application
 *
 * Returns : An entity type. A string bound to the lifetime of the given
 *           application.
 */
extern const char* nr_app_get_entity_type(const nrapp_t* app);

/*
 * Purpose : Return the entity guid related to the given application.
 *
 * Params  : 1. The application
 *
 * Returns : An entity guid. A string bound to the lifetime of the given
 *           application.
 */
extern const char* nr_app_get_entity_guid(const nrapp_t* app);

/*
 * Purpose : Return the host name related to the given application.
 *
 * Params  : 1. The application
 *
 * Returns : A host name. A string bound to the lifetime of the given
 *           application.
 */
extern const char* nr_app_get_host_name(const nrapp_t* app);

#endif /* NR_APP_HDR */
