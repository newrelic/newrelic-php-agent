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
 * Purpose : Returns the file descriptor used to communicate with the daemon.
 *           If the daemon failed to initialize or the connection has been lost
 *           or closed, will return -1.
 *
 * Returns : The daemon file descriptor or -1.
 *
 * Notes   : After this function is called, this process must call
 *           nr_agent_close_daemon_connection before forking.  This must
 *           be done even if nr_get_daemon_fd does not return a valid
 *           fd, as the connection may be in progress.
 *
 *           This approach is unsafe for threaded processes:
 *           Any thread which gets a file descriptor using this function
 *           can not guarantee that another thread does not close the fd.
 */
extern int nr_get_daemon_fd(void);

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
 * Purpose : Determine if a connection to the daemon is possible by creating
 *           one.  This differs from nr_get_daemon_fd in two ways: If the
 *           connection attempt fails, no warning messages will be printed,
 *           and if the connection attempt fails then it will be retried
 *           after a time_limit_ms delay.
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

#endif /* NR_AGENT_HDR */
