/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <netinet/in.h>
#include <netinet/tcp.h>

#include "nr_axiom.h"
#include "nr_agent.h"

#include "tlib_main.h"

static void test_conn_params_init(void) {
  nr_conn_params_t* params;
  /*
   * Test : Bad parameters
   */
  params = nr_conn_params_init(NULL);
  tlib_pass_if_int_equal("Supplying a NULL path yields unknown connection type",
                         params->type, NR_AGENT_CONN_UNKNOWN);
  nr_conn_params_free(params);

  params = nr_conn_params_init("1234567890");
  tlib_pass_if_int_equal(
      "Supplying a too-big port yields an unknown connection type",
      params->type, NR_AGENT_CONN_UNKNOWN);
  nr_conn_params_free(params);

  params = nr_conn_params_init("host:1234567890");
  tlib_pass_if_int_equal(
      "Supplying a too-big port yields an unknown connection type",
      params->type, NR_AGENT_CONN_UNKNOWN);
  nr_conn_params_free(params);

  params = nr_conn_params_init("-1");
  tlib_pass_if_int_equal(
      "Supplying a too-small port yields an unknown connection type",
      params->type, NR_AGENT_CONN_UNKNOWN);
  nr_conn_params_free(params);

  params = nr_conn_params_init("../not/absolute.txt");
  tlib_pass_if_int_equal(
      "Supplying a relative path yields an unknown connection type",
      params->type, NR_AGENT_CONN_UNKNOWN);
  nr_conn_params_free(params);

  params = nr_conn_params_init(
      "/this/is/a/very/long/absolute/path/this/is/a/very/"
      "long/absolute/path/this/is/a/very/long/absolute/"
      "path/this/is/a/very/long/absolute/path/this/is/a/"
      "very/long/absolute/path/absolute.txt");
  tlib_pass_if_int_equal(
      "Supplying a too-long path yields an unknown connection type",
      params->type, NR_AGENT_CONN_UNKNOWN);
  nr_conn_params_free(params);

  params = nr_conn_params_init("127.0.0.1:");
  tlib_pass_if_int_equal(
      "Supplying a only a host: yields an unknown connection type",
      params->type, NR_AGENT_CONN_UNKNOWN);
  nr_conn_params_free(params);

  params = nr_conn_params_init(":9000");
  tlib_pass_if_int_equal(
      "Supplying a only a :port yields an unknown connection type",
      params->type, NR_AGENT_CONN_UNKNOWN);
  nr_conn_params_free(params);

  params = nr_conn_params_init(":90x");
  tlib_pass_if_int_equal(
      "Supplying an invalid :port yields an unknown connection type",
      params->type, NR_AGENT_CONN_UNKNOWN);
  nr_conn_params_free(params);

#if NR_SYSTEM_LINUX
  params = nr_conn_params_init("@");
  tlib_pass_if_int_equal(
      "Supplying only an at must yield an unknown connection type",
      params->type, NR_AGENT_CONN_UNKNOWN);
  nr_conn_params_free(params);
#endif

  /*
   * Test : Well-formed inputs
   */
  params = nr_conn_params_init("1");
  tlib_pass_if_int_equal(
      "Supplying a well-formed port must yield a loopback connection type",
      params->type, NR_AGENT_CONN_TCP_LOOPBACK);
  tlib_pass_if_int_equal("Supplying a well-formed port must yield a port field",
                         1, params->location.port);
  nr_conn_params_free(params);

  params = nr_conn_params_init("/this/is/absolute.txt");
  tlib_pass_if_int_equal(
      "Supplying an absolute path yields an unix domain socket connection type",
      params->type, NR_AGENT_CONN_UNIX_DOMAIN_SOCKET);
  tlib_pass_if_str_equal(
      "Supplying an absolute path must yield a udspath field",
      "/this/is/absolute.txt", params->location.udspath);
  nr_conn_params_free(params);

#if NR_SYSTEM_LINUX
  params = nr_conn_params_init("@newrelic");
  tlib_pass_if_int_equal(
      "Supplying an atted path must yield a abstract socket connection type",
      params->type, NR_AGENT_CONN_ABSTRACT_SOCKET);
  tlib_pass_if_str_equal("Supplying an atted path must yield a udspath field",
                         "@newrelic", params->location.udspath);
  nr_conn_params_free(params);

  params = nr_conn_params_init("@/path/to/newrelic");
  tlib_pass_if_int_equal(
      "Supplying an atted path must yield a abstract socket connection type",
      params->type, NR_AGENT_CONN_ABSTRACT_SOCKET);
  tlib_pass_if_str_equal("Supplying an atted path must yield a udspath field",
                         "@/path/to/newrelic", params->location.udspath);
  nr_conn_params_free(params);
#endif

  params = nr_conn_params_init("127.0.0.1:9000");
  tlib_pass_if_int_equal(
      "Supplying a host:port must yield a TCP host + port connection type",
      params->type, NR_AGENT_CONN_TCP_HOST_PORT);
  tlib_pass_if_str_equal("Supplying host:port must yield an address field",
                         "127.0.0.1", params->location.address.host);
  tlib_pass_if_int_equal("Supplying host:port must yield a port field", 9000,
                         params->location.address.port);
  nr_conn_params_free(params);

  params = nr_conn_params_init("[2001:2001:2001:11]:9000");
  tlib_pass_if_int_equal(
      "Supplying a host:port must yield a TCP host + port connection type",
      params->type, NR_AGENT_CONN_TCP_HOST_PORT);
  tlib_pass_if_str_equal("Supplying host:port must yield an address field",
                         "2001:2001:2001:11", params->location.address.host);
  tlib_pass_if_int_equal("Supplying host:port must yield a port field", 9000,
                         params->location.address.port);
  nr_conn_params_free(params);
}
static void test_agent_reinitialize_daemon_tcp_connection_parameters_bad_params(
    void) {
  nr_conn_params_t* params = NULL;
  struct sockaddr* daemon_sa = NULL;

  /*
   * Test : Bad parameters
   */
  daemon_sa = nr_get_agent_daemon_sa();
  tlib_pass_if_null("nr_agent_daemon_sa viewable", daemon_sa);

  tlib_pass_if_status_failure(
      "nr_agent_reinitialize_daemon_tcp_connection_parameters should not do "
      "anything if nr_agent_daemon_sa is not initiaolized",
      nr_agent_reinitialize_daemon_tcp_connection_parameters(false));
  tlib_pass_if_null(
      "A call to reinitialize cxn parameters when it hasn't been initialized "
      "yet should not modify cxn parameters.",
      daemon_sa);

  params = nr_conn_params_init(NULL);
  tlib_pass_if_int_equal("Supplying a NULL path yields unknown connection type",
                         params->type, NR_AGENT_CONN_UNKNOWN);
  tlib_pass_if_status_failure(
      "Don't initialize with bad connection parameters.",
      nr_agent_initialize_daemon_connection_parameters(params));
  tlib_pass_if_status_failure(
      "Don't reinitialize if initialize failed due to bad cxn parameters.",
      nr_agent_reinitialize_daemon_tcp_connection_parameters(false));
  daemon_sa = nr_get_agent_daemon_sa();
  tlib_pass_if_null(
      "Don't reinitialize if initialize failed due to bad cxn parameters and "
      "should not modify cxn parameters.",
      daemon_sa);
  daemon_sa = NULL;
  nr_conn_params_free(params);
}

static void test_agent_reinitialize_daemon_tcp_connection_parameters_loopback(
    void) {
  nr_conn_params_t* params = NULL;
  struct sockaddr* daemon_sa = NULL;
  /*
   * Test : Well-formed inputs
   *
   * This test will:
   * 1) Send valid ip address and host params to nr_conn_params_init.
   * 2) Initialize daemon cxn parameters
   * 3) Try to reinitialize, but should fail since it is not a TCP loopback cxn.
   *
   */
  params = nr_conn_params_init("1");
  tlib_pass_if_int_equal(
      "Supplying a well-formed port must yield a loopback connection type",
      params->type, NR_AGENT_CONN_TCP_LOOPBACK);
  tlib_pass_if_status_success(
      "Initialize with good loopback connection parameters.",
      nr_agent_initialize_daemon_connection_parameters(params));
  daemon_sa = nr_get_agent_daemon_sa();
  tlib_pass_if_not_null("Initialize should populate daemon sockaddress",
                        daemon_sa);
  tlib_pass_if_status_failure(
      "Don't reinitialize if it is a loopback cxn.",
      nr_agent_reinitialize_daemon_tcp_connection_parameters(false));

  daemon_sa = NULL;
  nr_conn_params_free(params);
}

static void test_agent_reinitialize_daemon_tcp_connection_parameters_udp(void) {
  nr_conn_params_t* params = NULL;
  struct sockaddr* daemon_sa = NULL;
  struct sockaddr* daemon_sa2 = NULL;

  /*
   * Test : Well-formed inputs
   *
   * This test will:
   * 1) Send valid ip address and host params to nr_conn_params_init.
   * 2) Initialize daemon cxn parameters
   * 3) Try to reinitialize, but should fail since it is not a TCP cxn.
   * 4) Verify that the agent_daemon_sa pointer didn't get pointed to the TCP
   * struct.
   *
   */
  params = nr_conn_params_init("/this/is/absolute.txt");
  tlib_pass_if_int_equal(
      "Supplying an absolute path yields an unix domain socket connection type",
      params->type, NR_AGENT_CONN_UNIX_DOMAIN_SOCKET);
  tlib_pass_if_status_success(
      "Initialize with unix domain socket connection parameters.",
      nr_agent_initialize_daemon_connection_parameters(params));
  daemon_sa = nr_get_agent_daemon_sa();
  tlib_pass_if_not_null(
      "Initialize should populate unix domain socket daemon sockaddress",
      daemon_sa);
  tlib_pass_if_status_failure(
      "Don't reinitialize if it is a unix socket cxn.",
      nr_agent_reinitialize_daemon_tcp_connection_parameters(false));
  daemon_sa2 = nr_get_agent_daemon_sa();
  tlib_pass_if_equal("Don't modify sockaddr if it is not a tcp cxn.", daemon_sa,
                     daemon_sa2, const void*, "%p") daemon_sa
      = NULL;
  daemon_sa2 = NULL;
  nr_conn_params_free(params);
}
static void
test_agent_reinitialize_daemon_tcp_connection_parameters_abstract_socket(void) {
  /*
   * Test : Well-formed inputs
   *
   * This test will:
   * 1) Send valid ip address and host params to nr_conn_params_init.
   * 2) Initialize daemon cxn parameters
   * 3) Try to reinitialize, but should fail since it is not a TCP cxn.
   * 4) Verify that the agent_daemon_sa pointer didn't get pointed to the TCP
   * struct.
   *
   */

#if NR_SYSTEM_LINUX
  nr_conn_params_t* params = NULL;
  struct sockaddr* daemon_sa = NULL;
  struct sockaddr* daemon_sa2 = NULL;

  params = nr_conn_params_init("@newrelic");
  tlib_pass_if_int_equal(
      "Supplying an atted path must yield a abstract socket connection type",
      params->type, NR_AGENT_CONN_ABSTRACT_SOCKET);
  tlib_pass_if_status_success(
      "Initialize with abstract socket connection parameters.",
      nr_agent_initialize_daemon_connection_parameters(params));
  daemon_sa = nr_get_agent_daemon_sa();
  tlib_pass_if_not_null(
      "Initialize should populate abstract socket daemon sockaddress",
      daemon_sa);
  tlib_pass_if_status_failure(
      "Don't reinitialize if it is an abstract socket cxn.",
      nr_agent_reinitialize_daemon_tcp_connection_parameters(false));
  daemon_sa2 = nr_get_agent_daemon_sa();
  tlib_pass_if_equal(
      "Don't modify sockaddr if it is not a tcp cxn. It should be pointing at "
      "a `sockaddr_un` not sockeraddr_in(6).",
      daemon_sa, daemon_sa2, const void*, "%p") daemon_sa
      = NULL;
  daemon_sa2 = NULL;
  nr_conn_params_free(params);

  params = nr_conn_params_init("@/path/to/newrelic");
  tlib_pass_if_int_equal(
      "Supplying an atted path must yield a abstract socket connection type",
      params->type, NR_AGENT_CONN_ABSTRACT_SOCKET);
  tlib_pass_if_status_success(
      "Initialize with abstract socket connection parameters.",
      nr_agent_initialize_daemon_connection_parameters(params));
  daemon_sa = nr_get_agent_daemon_sa();
  tlib_pass_if_not_null(
      "Initialize should populate abstract socket daemon sockaddress",
      daemon_sa);
  tlib_pass_if_status_failure(
      "Don't reinitialize if it is an abstract socket cxn.",
      nr_agent_reinitialize_daemon_tcp_connection_parameters(false));
  daemon_sa2 = nr_get_agent_daemon_sa();
  tlib_pass_if_equal(
      "Don't modify sockaddr if it is not a tcp cxn. It should be pointing at "
      "a `sockaddr_un` not sockaddr_in or sockaddr_in6.",
      daemon_sa, daemon_sa2, const void*, "%p") daemon_sa
      = NULL;
  daemon_sa2 = NULL;
  nr_conn_params_free(params);
#endif
}

static void test_agent_reinitialize_daemon_tcp_connection_parameters_ipv4(
    void) {
  nr_conn_params_t* params = NULL;
  struct sockaddr* daemon_sa = NULL;
  struct sockaddr_in* daemon_sa_in = NULL;
  uint64_t address_as_ulong = 0;

  /*
   * Test : Well-formed inputs
   *
   * This test will:
   * 1) Send valid ip address and host params to nr_conn_params_init.
   * 2) Initialize daemon cxn parameters
   * 3) Try to reinitialize, but should fail since addresses should resolve to
   * the same. 4) Manually change the address information. 5) Try to
   * reinitialize, and should succeed since addresses are "different".
   *
   */

  params = nr_conn_params_init("127.1.1.1:9000");
  tlib_pass_if_int_equal(
      "Supplying an ipv4 host:port must yield an ipv4 TCP host + port "
      "connection type",
      params->type, NR_AGENT_CONN_TCP_HOST_PORT);
  tlib_pass_if_status_success(
      "Initialize with TCP IPv4 socket connection parameters.",
      nr_agent_initialize_daemon_connection_parameters(params));
  daemon_sa = nr_get_agent_daemon_sa();
  tlib_pass_if_not_null("Initialize should populate TCP IPv4 sockaddress.",
                        daemon_sa);
  daemon_sa_in = ((struct sockaddr_in*)(daemon_sa));
  address_as_ulong = (uint64_t)daemon_sa_in->sin_addr.s_addr;
  tlib_pass_if_status_success(
      "Don't reinitialize if we resolve to the same IPv4 address as the "
      "previous one.",
      nr_agent_reinitialize_daemon_tcp_connection_parameters(false));
  tlib_pass_if_uint64_t_equal(
      "Reinitialize should keep same IP if if the IPv4 addresses are "
      "the same.",
      address_as_ulong, (uint64_t)daemon_sa_in->sin_addr.s_addr);

  /* Manually change the s_addr so it looks different when we try to
   * reinitialize */
  daemon_sa_in->sin_addr.s_addr = 0;
  tlib_pass_if_status_success(
      "Reinitialize if we resolve to a different IPv4 address than the "
      "previous one.",
      nr_agent_reinitialize_daemon_tcp_connection_parameters(false));
  tlib_pass_if_uint64_t_equal(
      "Reinitialize should modify the sockaddr if the IPv4 addresses are "
      "different.",
      address_as_ulong, (uint64_t)daemon_sa_in->sin_addr.s_addr);
  address_as_ulong = 0;
  nr_conn_params_free(params);

  /*
   * Test : Well-formed inputs
   *
   * This test will:
   * 1) Send valid host name (not ip address) and host params to
   * nr_conn_params_init. 2) Initialize daemon cxn parameters 3) We won't try to
   * reinitialize without changing params because it is a live host and might
   * give us a different address anyway. 4) Manually change the address
   * information. 5) Try to reinitialize, and should succeed since addresses are
   * "different".
   *
   */

  params = nr_conn_params_init("google.com:80");
  tlib_pass_if_int_equal(
      "Supplying an ipv4 host:port must yield an ipv4 TCP host + port "
      "connection type",
      params->type, NR_AGENT_CONN_TCP_HOST_PORT);
  tlib_pass_if_status_success(
      "Initialize with TCP IPv4 socket connection parameters.",
      nr_agent_initialize_daemon_connection_parameters(params));
  daemon_sa = nr_get_agent_daemon_sa();
  tlib_pass_if_not_null("Initialize should populate TCP IPv4 sockaddress.",
                        daemon_sa);
  daemon_sa_in = ((struct sockaddr_in*)(daemon_sa));
  address_as_ulong = (uint64_t)daemon_sa_in->sin_addr.s_addr;

  daemon_sa_in->sin_addr.s_addr = 0;
  tlib_pass_if_status_success(
      "Reinitialize if we resolve to a different IPv4 address than the "
      "previous one.",
      nr_agent_reinitialize_daemon_tcp_connection_parameters(false));
  tlib_pass_if_true(
      "Reinitialize should modify the sockaddr if the IPv4 addresses are "
      "different.",
      0 != (uint64_t)daemon_sa_in->sin_addr.s_addr,
      "Address should not equal zero.");
  address_as_ulong = 0;
  nr_conn_params_free(params);
}

static void test_agent_reinitialize_daemon_tcp_connection_parameters_ipv6(
    void) {
  nr_conn_params_t* params = NULL;
  struct sockaddr* daemon_sa = NULL;
  struct sockaddr_in6* daemon_sa_in6 = NULL;
  nr_status_t bound_to_ipv6;
  uint8_t address_as_array[16] = {0};

  /*
   * Test : Well-formed inputs
   */
  params = nr_conn_params_init("[2001:2001:2001:11]:9000");
  tlib_pass_if_int_equal(
      "Supplying a host:port must yield an IPv6 TCP host + port connection "
      "type",
      params->type, NR_AGENT_CONN_TCP_HOST_PORT);
  bound_to_ipv6 = nr_agent_initialize_daemon_connection_parameters(params);

  if (NR_FAILURE == bound_to_ipv6) {
    /* Since we can't bind to ipv6, don't continue the ipv6 tests that assume we
     * did. */
    nr_conn_params_free(params);
    return;
  }

  /*
   * Test : Well-formed inputs
   *
   * This test will:
   * 1) Valid ip address and host params have already been passed to
   * nr_conn_params_init and we've verified the agent can bind to ipv6 to
   * continue the remaining tests. 2) Initialize daemon cxn parameters 3) Try to
   * reinitialize, but should fail since addresses should resolve to the same.
   * 4) Manually change the address information.
   * 5) Try to reinitialize, and should succeed since addresses are "different".
   *
   */

  tlib_pass_if_status_success("Initialize with TCP IPv6 connection parameters.",
                              bound_to_ipv6);
  daemon_sa = nr_get_agent_daemon_sa();
  tlib_pass_if_not_null("Initialize should populate TCP IPv6 sockaddress.",
                        daemon_sa);
  daemon_sa_in6 = ((struct sockaddr_in6*)(daemon_sa));
  tlib_pass_if_status_success(
      "Reinitialize should be true if we resolve to the same IPv6 address as "
      "the previous one.",
      nr_agent_reinitialize_daemon_tcp_connection_parameters(false));
  nr_memcpy(address_as_array, daemon_sa_in6->sin6_addr.s6_addr,
            sizeof(daemon_sa_in6->sin6_addr.s6_addr));
  tlib_pass_if_true(
      "Reinitialize should have the same the IPv6 sockaddr.",
      0
          == nr_memcmp(address_as_array, daemon_sa_in6->sin6_addr.s6_addr,
                       sizeof(daemon_sa_in6->sin6_addr.s6_addr)),
      "memcpm!=0");
  /* Manually change the s6_addr so it looks different when we try to
   * reinitialize */
  nr_memset(daemon_sa_in6->sin6_addr.s6_addr, 0,
            sizeof(daemon_sa_in6->sin6_addr.s6_addr));
  tlib_pass_if_status_success(
      "Reinitialize if we resolve to a different IPv6 address than the "
      "previous one.",
      nr_agent_reinitialize_daemon_tcp_connection_parameters(false));
  tlib_pass_if_true(
      "Reinitialized the IPv6 sockaddr.",
      0
          == nr_memcmp(address_as_array, daemon_sa_in6->sin6_addr.s6_addr,
                       sizeof(daemon_sa_in6->sin6_addr.s6_addr)),
      "memcpm!=0");
  nr_conn_params_free(params);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_conn_params_init();
  test_agent_reinitialize_daemon_tcp_connection_parameters_bad_params();
  test_agent_reinitialize_daemon_tcp_connection_parameters_loopback();
  test_agent_reinitialize_daemon_tcp_connection_parameters_udp();
  test_agent_reinitialize_daemon_tcp_connection_parameters_abstract_socket();
  test_agent_reinitialize_daemon_tcp_connection_parameters_ipv6();
  test_agent_reinitialize_daemon_tcp_connection_parameters_ipv4();
}
