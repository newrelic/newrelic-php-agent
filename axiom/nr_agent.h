/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions to manage the agent's connection to the daemon.
 */
#ifndef NR_AGENT_HDR
#define NR_AGENT_HDR

#include "nr_axiom.h"
#include "nr_app.h"

#define NR_PHP_AGENT_EXT_DOCS_URL "https://docs.newrelic.com/docs/apm/agents/php-agent/"

/*
 * The means by which the agent and the daemon connect.
 */
typedef enum _nr_agent_daemon_conn_t {
  NR_AGENT_CONN_UNKNOWN = 0,
  NR_AGENT_CONN_UNIX_DOMAIN_SOCKET = 1,
  NR_AGENT_CONN_ABSTRACT_SOCKET = 2,
  NR_AGENT_CONN_TCP_LOOPBACK = 3,
  NR_AGENT_CONN_TCP_HOST_PORT = 4
} nr_agent_daemon_conn_t;

/*
 * To represent the means by which the agent and the daemon connect,
 * there's a type along with a single field that represents the
 * particular kind of daemon address */
typedef struct _nr_conn_params_t {
  nr_agent_daemon_conn_t type;
  int port;

  /*
   * Type specific fields.
   *
   * The union type can only hold one field at a time. This ensures that we
   * will not reserve memory for fields that are not applicable for this type
   * of connection. Example: An NR_AGENT_CONN_TCP_HOST_PORT connection only sets
   * the address field.
   *
   * You must check the nr_agent_daemon_conn_t to determine which field is being
   * used.   */
  union _location {
    char* udspath; /* NR_AGENT_CONN_UNIX_DOMAIN_SOCKET
                      NR_AGENT_CONN_ABSTRACT_SOCKET */
    int port;      /* NR_AGENT_CONN_TCP_LOOPBACK */
    struct {
      char* host;
      int port;
    } address; /* NR_AGENT_CONN_TCP_HOST_PORT */
  } location;
} nr_conn_params_t;

/*
 * Purpose   : Using the supplied daemon_address, parse the string and
 *             initialize a nr_conn_params_t structure to prepare for
 *             connecting with the daemon.
 *
 * Parameter : A string representing a daemon's address any of an absolute path
 *             for a Unix-domain socket, an atted path for an abstract socket
 *             or a numeric port.
 *
 * Returns   : A newly allocated nr_conns_param_t.  When the string is not
 *             a well-formed location, the nr_conns_param_t type is
 *             NR_AGENT_CONN_UNKNOWN.
 */
nr_conn_params_t* nr_conn_params_init(const char* daemon_address);

/*
 * Purpose   : Free an nr_conns_params_t
 *
 * Parameter : An allocated nr_conns_params_t
 *
 */
void nr_conn_params_free(nr_conn_params_t* params);

/*
 * Purpose : This is the agent's global applist.
 *
 * Note    : There is no locking around this application list.  Therefore
 *           it should be created before and destroyed after multiple threads
 *           have access to it.
 */
extern nrapplist_t* nr_agent_applist;

/*
 * Purpose : Using a configuration value representing the daemon location
 *           derive the intended communication connection for the agent
 *           and daemon.
 *
 * Params  : 1. The daemon's address: a string representing
 *              a Unix domain socket, abstract socket, or port.
 *
 * Returns : The type of connection, one of nr_agent_daemon_conn_t enum
 *           including NR_AGENT_CONN_UNKNOWN for ill-formed parameters.
 *
 */
extern nr_agent_daemon_conn_t nr_agent_derive_connection_type(
    const char* daemon_address);

/*
 * Purpose : Using a string representing the daemon's address
 *           initialize the communication structures necessary to
 *           establish a channel of communication to the daemon.
 *
 * Params  : 1. The daemon's connection parameters comprising
 *              the connection type and daemon address.
 */
nr_status_t nr_agent_initialize_daemon_connection_parameters(
    nr_conn_params_t* conn_params);

/*
 * Purpose : Using previously initialized daemon tcp connection information,
 *           reinitialize the communication structures necessary to
 *           establish a channel of TCP communication to the daemon.
 *           Sometimes a server can go down and be replaced with a new one with
 *           the same name, but a different IP.  This function will resolve the
 *           IP again when necessary.
 *
 * Params  : 1. use_ttl to indicate whether the tcp time to live mechanism
 *              should be used.
 *
 * Returns : NR_SUCCESS when it either changes the TCP daemon connection
 *           parameters or verifies they are the most up to date. Returns
 *           NR_FAILURE if it does not attempt to reinitialize due to any of the
 *           following reasons:
 *           1. It is not a TCP connection
 *           2. It is TCP connection, but is a TCP loopback connection.
 *           3. It is TCP connection, but the time to live before reinitializing
 *              a connection has not yet elapsed.
 */
nr_status_t nr_agent_reinitialize_daemon_tcp_connection_parameters(
    bool use_ttl);

/*
 * Purpose : To get the global variable `nr_get_agent_daemon_sa`.  This is only
 *           to be used for verification in unit tests.
 *
 * Returns : The global variable `nr_agent_daemon_sa` containing the socket
 *           address information.
 *
 */
struct sockaddr* nr_get_agent_daemon_sa(void);

/*
 * Purpose : Get the file descriptor for the connection to the daemon.
 *
 * Returns : NR_SUCCESS and sets the file descriptor for the daemon connection
 *           in the provided pointer if successful.
 *           NR_FAILURE otherwise.
 *
 * Notes   : To ensure thread safety nr_agent_daemon_mutex must be locked
 *           before calling this function.
 */
extern nr_status_t nr_agent_get_daemon_fd_locked(int* fdp);

/*
 * Purpose : This call will attempt to ensure we are connected to the daemon.
 *           It is non-blocking so it is pretty quick. If we had no connection
 *           and the daemon has since been brought back up, this will start the
 *           process of connecting to it.
 *
 * Returns : NR_SUCCESS if connection to the daemon is established.
 *           NR_FAILURE otherwise.
 *
 * Notes   : After this function is called, this process must call
 *           nr_agent_close_daemon_connection before forking.  This must
 *           be done even if NR_FAILURE is returned, as the connection
 *           may be in progress.
 */
extern nr_status_t nr_agent_probe_daemon_connection(void);

/*
 * Purpose : Set the connection to use for daemon communication.
 *
 * Params  : 1. An established connection to a daemon process.
 */
extern void nr_set_daemon_fd(int fd);

/*
 * Purpose : Close the connection between an agent process and the daemon.
 *
 * Params  : None.
 *
 * Returns : Nothing.
 *
 * Notes   : Only called from within a agent process. This is called when an
 *           error has been detected by the agent when trying to communicate
 *           with the daemon.
 */
extern void nr_agent_close_daemon_connection(void);

/*
 * Purpose : Close the connection between an agent process and the daemon,
 *           for a caller that already holds nr_agent_daemon_mutex.
 *
 * Params  : None.
 *
 * Returns : NR_SUCCESS, or NR_FAILURE if the calling thread does not hold
 *           nr_agent_daemon_mutex.
 *
 * Notes   : Unlike nr_agent_close_daemon_connection, this does not lock
 *           nr_agent_daemon_mutex itself. Calling it without already
 *           holding the mutex (via nr_agent_lock_daemon_mutex) is an error.
 */
extern nr_status_t nr_agent_close_daemon_connection_locked(void);

/*
 * Purpose : Determine if a connection to the daemon is possible by creating
 *           one.  This differs from nr_agent_probe_daemon_connection in two
 *           ways: If the connection attempt fails, no warning messages will
 *           be printed, and if the connection attempt fails then it will be
 *           retried after a time_limit_ms delay.
 *
 * Returns : 1 if a connection to the daemon succeeded, and 0 otherwise.
 */
extern int nr_agent_try_daemon_connect(int time_limit_ms);

/*
 * Purpose : Lock or unlock access to the daemon from within an agent process.
 *           This is used to ensure that only one thread within an agent can
 *           ever be communicating with the daemon at 1 time, in order to
 *           prevent data interleaving and trying to multiplex commands and
 *           their replies.
 *
 * Params  : None.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 *
 * Notes   : Only used within child processes.
 */
extern nr_status_t nr_agent_lock_daemon_mutex(void);
extern nr_status_t nr_agent_unlock_daemon_mutex(void);

/*
 * Purpose : Bracket a block of code with the daemon mutex lock/unlock,
 *           so callers that talk to the daemon don't have to repeat the
 *           lock/unlock/close-on-failure sequence around their own
 *           send/receive logic.
 *
 *           op_name is a short string (e.g. "APPINFO", "SPAN_BATCH",
 *           "TXNDATA") used only to identify the caller in log messages
 *           about lock/fd/unlock failures.
 *
 *           io_status reflects only the outcome of body: body is
 *           responsible for setting it, and the macro never writes to it.
 *           Callers must initialize io_status to a failure value before
 *           the macro runs, since it is left untouched if body never runs
 *           (i.e. locking or fetching the daemon fd failed).
 *
 *           If body runs and leaves io_status as a failure, the daemon
 *           connection is closed while still holding the lock, avoiding a
 *           race against a concurrent reconnect that a close performed
 *           after unlocking could hit. Lock, fd-fetch, and unlock failures
 *           are logged but are a distinct concern from io_status: e.g. an
 *           unlock failure can happen after body already succeeded, and
 *           does not by itself mean the connection is bad.
 *
 * Usage   : nr_status_t st = NR_FAILURE;
 *           NR_AGENT_WITH_DAEMON_FD("APPINFO", st, {
 *             st = nr_write_message(daemon_fd, ..., deadline);
 *           });
 */
#define NR_AGENT_WITH_DAEMON_FD(op_name, io_status, body)                      \
  do {                                                                         \
    if (NR_SUCCESS != nr_agent_lock_daemon_mutex()) {                          \
      nrl_error(NRL_DAEMON, "%s: failed to lock daemon mutex", (op_name));     \
    } else {                                                                   \
      int daemon_fd = -1;                                                      \
      if (NR_SUCCESS != nr_agent_get_daemon_fd_locked(&daemon_fd)) {           \
        nrl_error(NRL_DAEMON, "%s: failed to get daemon fd", (op_name));       \
      } else {                                                                 \
        body if (NR_SUCCESS != (io_status)) {                                  \
          nr_agent_close_daemon_connection_locked();                           \
        }                                                                      \
      }                                                                        \
      if (NR_SUCCESS != nr_agent_unlock_daemon_mutex()) {                      \
        nrl_error(NRL_DAEMON, "%s: failed to unlock daemon mutex", (op_name)); \
      }                                                                        \
    }                                                                          \
  } while (0)

#endif /* NR_AGENT_HDR */
