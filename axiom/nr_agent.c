/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "nr_agent.h"
#include "util_errno.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_number_converter.h"
#include "util_sleep.h"
#include "util_strings.h"
#include "util_syscalls.h"
#include "util_time.h"

typedef enum _nr_socket_type_t {
  NR_LISTEN_TYPE_TCP,
  NR_LISTEN_TYPE_TCP6,
  NR_LISTEN_TYPE_UNIX
} nr_socket_type_t;

nrapplist_t* nr_agent_applist = 0;

static nrthread_mutex_t nr_agent_daemon_mutex = NRTHREAD_MUTEX_INITIALIZER;

static int nr_agent_daemon_fd = -1;

static struct sockaddr_in nr_agent_daemon_inaddr;
static struct sockaddr_in6 nr_agent_daemon_inaddr6;
static struct sockaddr_un nr_agent_daemon_unaddr;
static struct sockaddr* nr_agent_daemon_sa = 0;
static socklen_t nr_agent_daemon_sl = 0;

static nr_socket_type_t nr_agent_desired_type;
static char nr_agent_desired_uds[sizeof(nr_agent_daemon_unaddr.sun_path)];
static const int nr_agent_desired_uds_max
    = sizeof(nr_agent_daemon_unaddr.sun_path) - 1;

static char nr_agent_connect_tcp_daemon_address[512] = {'\0'};

static char nr_agent_connect_method_msg[512];

#define NR_AGENT_CANT_CONNECT_WARNING_BACKOFF_SECONDS 20
static time_t nr_agent_last_cant_connect_warning = 0;

/*
 * TCP TTL (time to live) specifies how long to wait (in seconds) before trying
 * to re-resolve an IP address. The duration is measured based on the difference
 * between the current time and the last known good time which contains the
 * latest of the last successful connection time or the last successful address
 * resolution.
 */
#define NR_AGENT_TCP_DAEMON_CONNECTION_TTL_SECONDS 45 * NR_TIME_DIVISOR
static nrtime_t nr_agent_last_checked_tcp_connection = 0;

typedef enum _nr_agent_connection_state_t {
  NR_AGENT_CONNECTION_STATE_START,
  NR_AGENT_CONNECTION_STATE_IN_PROGRESS,
  NR_AGENT_CONNECTION_STATE_CONNECTED,
} nr_agent_connection_state_t;

nr_agent_connection_state_t nr_agent_connection_state
    = NR_AGENT_CONNECTION_STATE_START;

#define NR_AGENT_MAX_PORT_VALUE (65536)
static bool nr_agent_is_port_out_of_bounds(int port) {
  if ((port <= 0) || (port >= NR_AGENT_MAX_PORT_VALUE)) {
    return true;
  }
  return false;
}

static int nr_parse_port(const char* strport) {
  int port;
  char* endptr;

  /* Parse the port and affirm it is a sensible value */
  port = (int)strtol(strport, &endptr, 10);

  if ((strport + nr_strlen(strport)) != endptr) {
    nrl_error(NRL_DAEMON, "invalid daemon port setting: '%s' is not a number",
              strport);
    return -1;
  }

  if (nr_agent_is_port_out_of_bounds(port)) {
    nrl_error(NRL_DAEMON,
              "invalid daemon port setting: port must be between 0 and 65536"
              "inclusive");
    return -1;
  }

  return port;
}

static bool nr_parse_address_port(const char* address, char** host, int* port) {
  size_t colon_idx;
  size_t address_len;
  int tcp_port;

  if (NULL == address) {
    return false;
  }

  address_len = nr_strlen(address);
  colon_idx = nr_strncaseidx_last_match(address, ":", address_len);

  if (0 == colon_idx) {
    nrl_error(NRL_DAEMON,
              "invalid daemon host:port specification: host is missing");
    return false;
  }

  if ((address_len - 1) == colon_idx) {
    nrl_error(NRL_DAEMON,
              "invalid daemon host:port specification: port is missing");
    return false;
  }

  /* Parse the port and affirm it is a sensible value */
  tcp_port = nr_parse_port(&address[colon_idx + 1]);

  if (-1 == tcp_port) {
    return false;
  }

  if ('[' == address[0] && ']' == address[colon_idx - 1]) {
    /* IPv6 */
    *host = nr_strndup(address + 1, colon_idx - 2);
  } else {
    /* IPv4 or host name */
    *host = nr_strndup(address, colon_idx);
  }
  *port = tcp_port;

  return true;
}

nr_conn_params_t* nr_conn_params_init(const char* daemon_address) {
  int tcp_port;
  nr_conn_params_t* new_conn_params
      = (nr_conn_params_t*)nr_zalloc(sizeof(nr_conn_params_t));
  new_conn_params->type = NR_AGENT_CONN_UNKNOWN;

  if (nrunlikely(NULL == daemon_address)) {
    nrl_error(NRL_DAEMON,
              "invalid daemon connection parameters: the daemon address "
              "and port are both NULL");
    return new_conn_params;
  }

#if NR_SYSTEM_LINUX
  /*
   * Linux Abstract Socket
   *   There's a '@' in the address
   */
  if ('@' == daemon_address[0]) {
    if (0 == daemon_address[1]) {
      nrl_error(NRL_DAEMON,
                "invalid daemon abstract domain socket: name is missing");
      return new_conn_params;
    }

    new_conn_params->type = NR_AGENT_CONN_ABSTRACT_SOCKET;
    new_conn_params->location.udspath = nr_strdup(daemon_address);
    return new_conn_params;
  }
#endif
  /*
   * IP Address, <host>:<port>
   *    There's a ':' in the address, indicating the user has supplied host:port
   */
  if (-1 != nr_stridx(daemon_address, ":")) {
    bool status = nr_parse_address_port(
        daemon_address, &new_conn_params->location.address.host,
        &new_conn_params->location.address.port);

    if (!status) {
      return new_conn_params;
    }

    new_conn_params->type = NR_AGENT_CONN_TCP_HOST_PORT;

    return new_conn_params;
  }
  /*
   * Unix-Domain Socket
   *   There's a '/' in the address, indicating the user has supplied a path.
   */
  if (-1 != nr_stridx(daemon_address, "/")) {
    if ('/' != daemon_address[0]) {
      nrl_error(NRL_DAEMON,
                "invalid daemon UNIX-domain socket: path must be absolute");
      return new_conn_params;
    } else if (nr_strlen(daemon_address) > nr_agent_desired_uds_max) {
      nrl_error(NRL_DAEMON, "invalid daemon UNIX-domain socket: too long");
      return new_conn_params;
    }

    new_conn_params->type = NR_AGENT_CONN_UNIX_DOMAIN_SOCKET;
    new_conn_params->location.udspath = nr_strdup(daemon_address);
    return new_conn_params;
  }

  /*
   * Loopback Socket
   *   This point is reached because the string is not an abstract socket
   *   or a Unix-domain socket.  Treat the incoming parameter as a numeric
   *   value representing a port and validate accordingly.
   */
  tcp_port = nr_parse_port(daemon_address);

  if (-1 == tcp_port) {
    return new_conn_params;
  }

  new_conn_params->type = NR_AGENT_CONN_TCP_LOOPBACK;
  new_conn_params->location.port = tcp_port;
  return new_conn_params;
}

void nr_conn_params_free(nr_conn_params_t* params) {
  if (nrlikely(NULL != params)) {
    if (NR_AGENT_CONN_ABSTRACT_SOCKET == params->type
        || NR_AGENT_CONN_UNIX_DOMAIN_SOCKET == params->type) {
      nr_free(params->location.udspath);
    }
    if (NR_AGENT_CONN_TCP_HOST_PORT == params->type) {
      nr_free(params->location.address.host);
    }
    nr_free(params);
  }
}
struct sockaddr* nr_get_agent_daemon_sa(void) {
  return nr_agent_daemon_sa;
};

nr_status_t nr_agent_reinitialize_daemon_tcp_connection_parameters(
    bool use_ttl) {
  nr_status_t ret_val = NR_SUCCESS;
  nr_conn_params_t* conn_params = NULL;
  nrtime_t now = 0;

  if (0 == nr_strlen(nr_agent_connect_tcp_daemon_address)) {
    /* Either not a TCP connection or a TCP but loopback, so no need to try to
     * resolve. */

    return NR_FAILURE;
  }

  /*
   * If the duration between the since we last successfully connected and the
   * current time is greater than our TCP address resolution time to live, then
   * try to resolve the IP again.
   */
  now = nr_get_time();
  if ((use_ttl)
      && (nr_time_duration(nr_agent_last_checked_tcp_connection, now)
          < NR_AGENT_TCP_DAEMON_CONNECTION_TTL_SECONDS)) {
    nrl_verbosedebug(NRL_DAEMON,
                     "Waiting for TTL to elapse to resolve IP address for a "
                     "TCP connection: %s",
                     nr_agent_connect_method_msg);
    return NR_FAILURE;
  }
  nr_agent_last_checked_tcp_connection = now;

  nrl_verbosedebug(NRL_DAEMON,
                   "Attempting to resolve IP address for a TCP connection: %s",
                   nr_agent_connect_method_msg);

  conn_params = nr_conn_params_init(nr_agent_connect_tcp_daemon_address);
  ret_val = nr_agent_initialize_daemon_connection_parameters(conn_params);

  nr_conn_params_free(conn_params);

  return ret_val;
}

nr_status_t nr_agent_initialize_daemon_connection_parameters(
    nr_conn_params_t* conn_params) {
  if (NULL == conn_params || NR_AGENT_CONN_UNKNOWN == conn_params->type) {
    return NR_FAILURE;
  }

  nrt_mutex_lock(&nr_agent_daemon_mutex);

  if (conn_params->type == NR_AGENT_CONN_UNIX_DOMAIN_SOCKET
      || conn_params->type == NR_AGENT_CONN_ABSTRACT_SOCKET) {
    /*
     * Unix Domain Socket (see unix(7))
     */
    nr_agent_desired_type = NR_LISTEN_TYPE_UNIX;
    nr_strlcpy(nr_agent_desired_uds, conn_params->location.udspath,
               nr_agent_desired_uds_max);

    nr_agent_daemon_sa = (struct sockaddr*)&nr_agent_daemon_unaddr;
    nr_agent_daemon_sl = offsetof(struct sockaddr_un, sun_path)
                         + nr_strlen(conn_params->location.udspath) + 1;
    nr_memset(nr_agent_daemon_sa, 0, sizeof(nr_agent_daemon_sa));

    nr_agent_daemon_unaddr.sun_family = AF_UNIX;
    nr_strlcpy(nr_agent_daemon_unaddr.sun_path, conn_params->location.udspath,
               sizeof(nr_agent_daemon_unaddr.sun_path));
    if (NR_AGENT_CONN_ABSTRACT_SOCKET == conn_params->type) {
      /* A leading zero specifies an abstract socket to the kernel. */
      nr_agent_daemon_unaddr.sun_path[0] = '\0';

      /* Exclude the trailing zero to match the behavior of Go. */
      nr_agent_daemon_sl--;
    }

    nr_agent_connect_method_msg[0] = '\0';
    snprintf(nr_agent_connect_method_msg, sizeof(nr_agent_connect_method_msg),
             "uds=%s", conn_params->location.udspath);
  } else if (NR_AGENT_CONN_TCP_LOOPBACK == conn_params->type) {
    /*
     * Use a loopback TCP connection.
     */
    nr_agent_desired_type = NR_LISTEN_TYPE_TCP;

    nr_agent_daemon_sa = (struct sockaddr*)&nr_agent_daemon_inaddr;
    nr_agent_daemon_sl = sizeof(nr_agent_daemon_inaddr);
    nr_memset(nr_agent_daemon_sa, 0, (int)nr_agent_daemon_sl);

    nr_agent_daemon_inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    nr_agent_daemon_inaddr.sin_port
        = htons((uint16_t)conn_params->location.port);
    nr_agent_daemon_inaddr.sin_family = AF_INET;

    nr_agent_connect_method_msg[0] = '\0';
    snprintf(nr_agent_connect_method_msg, sizeof(nr_agent_connect_method_msg),
             "port=%d", conn_params->location.port);
  } else {
    /*
     * Use a TCP connection
     */
    struct addrinfo hints, *addr_res;
    int addr_status;
    char port[6];

    nr_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    nr_agent_connect_tcp_daemon_address[0] = '\0';
    snprintf(nr_agent_connect_tcp_daemon_address,
             sizeof(nr_agent_connect_tcp_daemon_address), "%s:%d",
             conn_params->location.address.host,
             conn_params->location.address.port);

    nr_itoa(port, sizeof(port), conn_params->location.address.port);
    addr_status = getaddrinfo(conn_params->location.address.host, port, &hints,
                              &addr_res);
    if (addr_status != 0 || addr_res == NULL) {
      nrl_error(NRL_DAEMON,
                "could not resolve daemon address [host=%s, port=%d]: %s",
                conn_params->location.address.host,
                conn_params->location.address.port, gai_strerror(addr_status));
      /*If we exit through this code path, unlock the mutex first. */
      nrt_mutex_unlock(&nr_agent_daemon_mutex);
      /*If this occurs on reinit, the previous information is still valid.*/
      return NR_FAILURE;
    }

    /*
     * Check is nr_agent_daemon_sa exists, as this may be an update to
     * re-resolve the TCP connection IP address.
     */
    if ((nr_agent_daemon_sa)
        && (0
            != nr_memcmp(nr_agent_daemon_sa, addr_res->ai_addr,
                         nr_agent_daemon_sl))) {
      nrl_info(NRL_DAEMON, "Resolved new IP for daemon: %s.",
               nr_agent_connect_method_msg);
      /*
       * For completeness clean everything out in the very small chance we have
       * resolved a different IP type than we originally resolved.
       */
      nr_memset(nr_agent_daemon_sa, 0, nr_agent_daemon_sl);
    }

    if (AF_INET6 == addr_res->ai_family) {
      nr_agent_desired_type = NR_LISTEN_TYPE_TCP6;
      nr_agent_daemon_sa = (struct sockaddr*)&nr_agent_daemon_inaddr6;
      nr_agent_daemon_sl = sizeof(nr_agent_daemon_inaddr6);
    } else {
      nr_agent_desired_type = NR_LISTEN_TYPE_TCP;
      nr_agent_daemon_sa = (struct sockaddr*)&nr_agent_daemon_inaddr;
      nr_agent_daemon_sl = sizeof(nr_agent_daemon_inaddr);
    }

    nr_memcpy(nr_agent_daemon_sa, addr_res->ai_addr, nr_agent_daemon_sl);
    freeaddrinfo(addr_res);

    nr_agent_connect_method_msg[0] = '\0';
    snprintf(nr_agent_connect_method_msg, sizeof(nr_agent_connect_method_msg),
             "host=%s, port=%d", conn_params->location.address.host,
             conn_params->location.address.port);
  }

  nrt_mutex_unlock(&nr_agent_daemon_mutex);

  return NR_SUCCESS;
}

static int nr_agent_create_socket(nr_socket_type_t listen_type) {
  int fd;
  int fl;
  int err;

  if (NR_LISTEN_TYPE_TCP == listen_type) {
    int on = 1;

    fd = nr_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    nr_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  } else if (NR_LISTEN_TYPE_TCP6 == listen_type) {
    int on = 1;

    fd = nr_socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
    nr_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  } else {
    fd = nr_socket(PF_UNIX, SOCK_STREAM, 0);
  }

  if (-1 == fd) {
    err = errno;
    nrl_warning(NRL_DAEMON, "daemon socket() returned %.16s", nr_errno(err));
    return -1;
  }

  fl = nr_fcntl(fd, F_GETFL, 0);
  if (-1 == fl) {
    err = errno;
    nrl_warning(NRL_DAEMON, "daemon fcntl(GET) returned %.16s", nr_errno(err));
    nr_close(fd);
    return -1;
  }

  fl |= O_NONBLOCK;
  if (0 != nr_fcntl(fd, F_SETFL, fl)) {
    err = errno;
    nrl_warning(NRL_DAEMON, "daemon fcntl(SET) returned %.16s", nr_errno(err));
    nr_close(fd);
    return -1;
  }

  return fd;
}

static void nr_agent_warn_connect_failure(int connect_fd,
                                          int connect_rv,
                                          int connect_err) {
  time_t now = time(0);

  if ((now - nr_agent_last_cant_connect_warning)
      < NR_AGENT_CANT_CONNECT_WARNING_BACKOFF_SECONDS) {
    return;
  }

  nr_agent_last_cant_connect_warning = now;

  nrl_warning(
      NRL_DAEMON | NRL_IPC,
      "daemon connect(fd=%d %.256s) returned %d errno=%.16s. "
      "Failed to connect to the newrelic-daemon. Please make sure that there "
      "is a properly configured newrelic-daemon running. "
      "For additional assistance, please see: "
      "https://docs.newrelic.com/docs/apm/agents/php-agent/"
      "advanced-installation/starting-php-daemon-advanced/",
      connect_fd, nr_agent_connect_method_msg, connect_rv,
      nr_errno(connect_err));
}

static int nr_get_daemon_fd_internal(int log_warning_on_connect_failure) {
  int err;
  int fl;
  nr_agent_connection_state_t state_before_connect;

  if (NR_AGENT_CONNECTION_STATE_CONNECTED == nr_agent_connection_state) {
    return nr_agent_daemon_fd;
  }

  if (-1 == nr_agent_daemon_fd) {
    nr_agent_daemon_fd = nr_agent_create_socket(nr_agent_desired_type);
    if (-1 == nr_agent_daemon_fd) {
      return -1;
    }
  }

  state_before_connect = nr_agent_connection_state;

  do {
    fl = nr_connect(nr_agent_daemon_fd, nr_agent_daemon_sa, nr_agent_daemon_sl);
    err = errno;
  } while ((-1 == fl) && (EINTR == err));

  if (0 == fl) {
    nrl_verbosedebug(NRL_DAEMON | NRL_IPC,
                     "daemon connect(fd=%d %.256s) succeeded",
                     nr_agent_daemon_fd, nr_agent_connect_method_msg);
  } else {
    nrl_verbosedebug(NRL_DAEMON | NRL_IPC,
                     "daemon connect(fd=%d %.256s) returned %d errno=%.16s",
                     nr_agent_daemon_fd, nr_agent_connect_method_msg, fl,
                     nr_errno(err));
  }

  if ((0 == fl) || (EISCONN == err)) {
    /*
     * Since the file descriptor is non-blocking, the connect call may return
     * EINPROGRESS.  If this happens, we need to determine when the connection
     * has completed.  We do this by, repeating the connect call.  Once the
     * connection succeeded, EISCONN will be returned.  This also has the
     * advantage that we can treat first attempt connects the same as
     * in-progress connects.
     */
    nr_agent_connection_state = NR_AGENT_CONNECTION_STATE_CONNECTED;
    return nr_agent_daemon_fd;
  }

  if ((EALREADY == err) || (EINPROGRESS == err)) {
    /*
     * The connection is in progress.
     * This is not unexpected the first time this function is called.
     * However, if this is not the first time, a log warning message
     * should be generated.
     */
    nr_agent_connection_state = NR_AGENT_CONNECTION_STATE_IN_PROGRESS;
    if (log_warning_on_connect_failure
        && (NR_AGENT_CONNECTION_STATE_IN_PROGRESS == state_before_connect)) {
      nr_agent_warn_connect_failure(nr_agent_daemon_fd, fl, err);
    }

    return -1;
  }

  /*
   * The connect call failed for an unknown reason.
   */
  if (log_warning_on_connect_failure) {
    nr_agent_warn_connect_failure(nr_agent_daemon_fd, fl, err);
  }

  nr_close(nr_agent_daemon_fd);

  nr_agent_daemon_fd = -1;
  nr_agent_connection_state = NR_AGENT_CONNECTION_STATE_START;
  return -1;
}

int nr_get_daemon_fd(void) {
  int fd;

  nrt_mutex_lock(&nr_agent_daemon_mutex);

  fd = nr_get_daemon_fd_internal(1);

  nrt_mutex_unlock(&nr_agent_daemon_mutex);

  if (-1 == fd) {
    /*
     * Still not connected.  If the connection method is from a resolved IP
     * address, the agent should make a call to see if we should resolve it
     * again.
     */
    if (NR_SUCCESS
        == nr_agent_reinitialize_daemon_tcp_connection_parameters(true)) {
      nrl_verbosedebug(NRL_DAEMON | NRL_IPC,
                       "Daemon (%.256s) has the most up to date TCP "
                       "information for the next connection attempt.",
                       nr_agent_connect_method_msg);
    }
  }

  return fd;
}

int nr_agent_try_daemon_connect(int time_limit_ms) {
  int fd;
  int did_connect = 0;

  nrt_mutex_lock(&nr_agent_daemon_mutex);

  fd = nr_get_daemon_fd_internal(0);
  if (-1 != fd) {
    did_connect = 1;
  } else if (NR_AGENT_CONNECTION_STATE_IN_PROGRESS
             == nr_agent_connection_state) {
    nr_msleep(time_limit_ms);
    fd = nr_get_daemon_fd_internal(0);
    if (-1 != fd) {
      did_connect = 1;
    }
  }

  nrt_mutex_unlock(&nr_agent_daemon_mutex);

  if (0 == did_connect) {
    /*
     * Still not connected.  If the connection method is from a resolved IP
     * address, the agent should make a call to see if we should resolve it
     * again.
     */
    if (NR_SUCCESS
        == nr_agent_reinitialize_daemon_tcp_connection_parameters(true)) {
      nrl_verbosedebug(NRL_DAEMON | NRL_IPC,
                       "Daemon connect(%.256s) has the most up to date "
                       "TCP information for the next connection attempt.",
                       nr_agent_connect_method_msg);
    }
  }

  return did_connect;
}

void nr_set_daemon_fd(int fd) {
  nrt_mutex_lock(&nr_agent_daemon_mutex);

  if (-1 != nr_agent_daemon_fd) {
    nrl_debug(NRL_DAEMON, "closed daemon connection fd=%d", nr_agent_daemon_fd);
    nr_close(nr_agent_daemon_fd);
    nr_agent_daemon_fd = -1;
  }

  nr_agent_daemon_fd = fd;
  nr_agent_last_cant_connect_warning = 0;
  nr_agent_connection_state = NR_AGENT_CONNECTION_STATE_START;

  if (-1 != nr_agent_daemon_fd) {
    nr_agent_connection_state = NR_AGENT_CONNECTION_STATE_CONNECTED;
  }

  nrt_mutex_unlock(&nr_agent_daemon_mutex);
}

void nr_agent_close_daemon_connection(void) {
  nr_set_daemon_fd(-1);
}

nr_status_t nr_agent_lock_daemon_mutex(void) {
  return nrt_mutex_lock(&nr_agent_daemon_mutex);
}

nr_status_t nr_agent_unlock_daemon_mutex(void) {
  return nrt_mutex_unlock(&nr_agent_daemon_mutex);
}
