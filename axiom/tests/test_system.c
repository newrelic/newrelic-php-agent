/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_memory.h"
#include "util_strings.h"
#include "util_system.h"

#include "tlib_main.h"

#if !defined(REFERENCE_DIR)
#error "REFERENCE_DIR not defined"
#endif

static void test_system_get_hostname(void) {
  char* hostname;

  hostname = nr_system_get_hostname();
  tlib_pass_if_true("hostname not null", 0 != hostname, "hostname=%p",
                    hostname);
  nr_free(hostname);
}

static void test_get_system_info_from_osrelease(void) {
  nr_system_t* sys;
  sys = (nr_system_t*)nr_zalloc(sizeof(nr_system_t));

#define FREE_SYS_DISTRO_VALUES \
  nr_free(sys->distro_id);     \
  nr_free(sys->distro_version_id);

  tlib_pass_if_true(
      "sys should have been allocated, something unrelated to this test is "
      "wrong with allocations",
      NULL != sys, "sys=%p", sys);

  if (NULL == sys) {
    return;
  }

  /* Sending a NULL sys shouldn't leak memory, crash, or segfault.*/
  nr_system_get_system_info_from_osrelease(
      NULL, REFERENCE_DIR "/osrelease_valid_ubuntu_24.04");

  /* Sending a NULL filename shouldn't modify sys*/
  nr_system_get_system_info_from_osrelease(sys, NULL);
  tlib_pass_if_null("if NULL filename sys->distro_id should be NULL",
                    sys->distro_id);
  tlib_pass_if_null("if NULL filename sys->distro_version_id should be NULL",
                    sys->distro_version_id);

  /* Sending a valid filename missing VERSION_ID and ID shouldn't modify sys*/
  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_missingid_missingversionid");
  tlib_pass_if_null(
      "if valid filename but missing VERSION_ID and ID sys->distro_id should "
      "be "
      "NULL",
      sys->distro_id);
  tlib_pass_if_null(
      "if valid filename but missing VERSION_ID and ID sys->distro_version_id "
      "should be NULL",
      sys->distro_version_id);

  /* Sending a valid sys, valid filename but no VERSION_ID should set distro_id
   * but not distro_version_id*/
  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_ubuntu_missingversionid");
  tlib_pass_if_str_equal(
      "if valid filename but no VERSION_ID sys->distro_id should be set",
      sys->distro_id, "ubuntu");
  tlib_pass_if_null(
      "if valid filename but no VERSION_ID sys->distro_version_id should be "
      "NULL",
      sys->distro_version_id);
  FREE_SYS_DISTRO_VALUES;

  /* Unexpected Cases */

  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_twoversionid_ubuntu_24.04");
  tlib_pass_if_str_equal(
      "if valid filename but two versionIds should be last one, VERSION_ID "
      "sys->distro_version_id should be set",
      sys->distro_version_id, "24.04");
  tlib_pass_if_str_equal(
      "if valid filename  ID sys->distro_id should be "
      "set",
      sys->distro_id, "ubuntu");
  FREE_SYS_DISTRO_VALUES;

  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_twoids_ubuntu_24.04");
  tlib_pass_if_str_equal(
      "if valid filename, VERSION_ID "
      "sys->distro_version_id should be set",
      sys->distro_version_id, "24.04");
  tlib_pass_if_str_equal(
      "if valid filename but 2 IDs, ID should be set to last one, ID "
      "sys->distro_id should be "
      "set",
      sys->distro_id, "ubuntu");
  FREE_SYS_DISTRO_VALUES;

  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_leadingwhitespace_ubuntu_24.04");
  tlib_pass_if_str_equal(
      "if valid filename but leading whitespace, VERSION_ID "
      "sys->distro_version_id should be set",
      sys->distro_version_id, "24.04");
  tlib_pass_if_str_equal(
      "if valid filename but leading whitespace, ID sys->distro_id should be "
      "set",
      sys->distro_id, "ubuntu");
  FREE_SYS_DISTRO_VALUES;

  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_emptyid_ubuntu_24.04");
  tlib_pass_if_str_equal(
      "if valid filename and VERSION_ID sys->distro_version_id should be set",
      sys->distro_version_id, "24.04");
  tlib_pass_if_null(
      "if valid filename empty version_id, string sys->distro_id should be "
      "NULL",
      sys->distro_id);
  FREE_SYS_DISTRO_VALUES;

  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_emptyquotes_ubuntu_24.04");
  tlib_pass_if_str_equal(
      "if valid filename and VERSION_ID sys->distro_version_id should be set",
      sys->distro_version_id, "24.04");
  tlib_pass_if_null(
      "if valid filename, version_id has empty quotes sys->distro_id should be "
      "NULL",
      sys->distro_id);
  FREE_SYS_DISTRO_VALUES;

  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_rightquote_ubuntu_24.04");
  tlib_pass_if_str_equal(
      "if valid filename and VERSION_ID sys->distro_version_id should be set",
      sys->distro_version_id, "24.04");
  tlib_pass_if_str_equal(
      "if valid filename, version_id has only right quote sys->distro_id "
      "should be set",
      sys->distro_id, "ubuntu");
  FREE_SYS_DISTRO_VALUES;

  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_leftquote_ubuntu_24.04");
  tlib_pass_if_str_equal(
      "if valid filename and VERSION_ID sys->distro_version_id should be set",
      sys->distro_version_id, "24.04");
  tlib_pass_if_str_equal(
      "if valid filename, version_id has only left quote sys->distro_id should "
      "be set",
      sys->distro_id, "ubuntu");
  FREE_SYS_DISTRO_VALUES;

  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_quoteonly_ubuntu_24.04");
  tlib_pass_if_str_equal(
      "if valid filename and VERSION_ID sys->distro_version_id should be set",
      sys->distro_version_id, "24.04");
  tlib_pass_if_null(
      "if valid filename, version_id has just a quote no value sys->distro_id "
      "should be NULL",
      sys->distro_id);
  FREE_SYS_DISTRO_VALUES;

  /* Sending a valid sys, valid filename but no ID should set distro_version_id
   * but not distro_id*/
  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_missingid_24.04");
  tlib_pass_if_str_equal(
      "if valid filename but no ID sys->distro_version_id should be set",
      sys->distro_version_id, "24.04");
  tlib_pass_if_null("if valid filename but no ID sys->distro_id should be NULL",
                    sys->distro_id);
  FREE_SYS_DISTRO_VALUES;

  /* Sending a valid sys, valid filename should set distro_id and
   * distro_version_id*/

  /* alpine uses unquoted VERSION_ID unquoted ID*/
  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_valid_alpine_3.23.0");
  tlib_pass_if_str_equal("if valid filename sys->distro_id should be set",
                         sys->distro_id, "alpine");
  tlib_pass_if_str_equal(
      "if valid filename sys->distro_version_id should be set",
      sys->distro_version_id, "3.23.0");
  FREE_SYS_DISTRO_VALUES;

  /* amzn uses quoted VERSION_ID quoted ID*/
  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_valid_amzn_2022");
  tlib_pass_if_str_equal("if valid filename sys->distro_id should be set",
                         sys->distro_id, "amzn");
  tlib_pass_if_str_equal(
      "if valid filename sys->distro_version_id should be set",
      sys->distro_version_id, "2022");
  FREE_SYS_DISTRO_VALUES;

  /* centos uses quoted VERSION_ID quoted ID*/
  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_valid_centos_9");
  tlib_pass_if_str_equal("if valid filename sys->distro_id should be set",
                         sys->distro_id, "centos");
  tlib_pass_if_str_equal(
      "if valid filename sys->distro_version_id should be set",
      sys->distro_version_id, "9");
  FREE_SYS_DISTRO_VALUES;

  /* debian uses quoted VERSION_ID unquoted ID*/
  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_valid_debian_11");
  tlib_pass_if_str_equal("if valid filename sys->distro_id should be set",
                         sys->distro_id, "debian");
  tlib_pass_if_str_equal(
      "if valid filename sys->distro_version_id should be set",
      sys->distro_version_id, "11");
  FREE_SYS_DISTRO_VALUES;

  /* ubuntu uses quoted VERSION_ID unquoted ID*/
  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_valid_ubuntu_24.04");
  tlib_pass_if_str_equal("if valid filename sys->distro_id should be set",
                         sys->distro_id, "ubuntu");
  tlib_pass_if_str_equal(
      "if valid filename sys->distro_version_id should be set",
      sys->distro_version_id, "24.04");
  FREE_SYS_DISTRO_VALUES;

  /* debian uses quoted VERSION_ID unquoted ID*/
  nr_system_get_system_info_from_osrelease(
      sys, REFERENCE_DIR "/osrelease_valid_debian_12");
  tlib_pass_if_str_equal("if valid filename sys->distro_id should be set",
                         sys->distro_id, "debian");
  tlib_pass_if_str_equal(
      "if valid filename sys->distro_version_id should be set",
      sys->distro_version_id, "12");
  FREE_SYS_DISTRO_VALUES;

  nr_system_destroy(&sys);
  tlib_pass_if_true("sys destroyed", 0 == sys, "sys=%p", sys);
}

static void test_get_system(void) {
  nr_system_t* sys = nr_system_get_system_information();

  tlib_pass_if_true("sys not null", 0 != sys, "sys=%p", sys);

  if (0 == sys) {
    return;
  }

#if defined(__linux__)
  tlib_pass_if_true("sys->sysname", 0 == nr_strcmp(sys->sysname, "Linux"),
                    "expected sysname=Linux result=%s", sys->sysname);
#elif defined(__APPLE__) && defined(__MACH__)
  tlib_pass_if_true("sys->sysname", 0 == nr_strcmp(sys->sysname, "Darwin"),
                    "expected sysname=Darwin result=%s", sys->sysname);
#elif defined(__sun__) || defined(__sun)
  tlib_pass_if_true("sys->sysname",
                    (0 == nr_strcmp(sys->sysname, "SunOS"))
                        || (0 == nr_strcmp(sys->sysname, "SmartOS")),
                    "expected sysname=SunOS/SmartOS result=%s", sys->sysname);
#elif defined(__FreeBSD__)
  tlib_pass_if_true("sys->sysname", 0 == nr_strcmp(sys->sysname, "FreeBSD"),
                    "expected sysname=FreeBSD result=%s", sys->sysname);
#else
#error Unsupported OS: please add the expected uname to this file.
#endif

  tlib_pass_if_true("sys value nodename not null", 0 != sys->nodename,
                    "sys->nodename=%p", sys->nodename);
  tlib_pass_if_true("sys value release not null", 0 != sys->release,
                    "sys->release=%p", sys->release);
  tlib_pass_if_true("sys value version not null", 0 != sys->version,
                    "sys->version=%p", sys->version);
  tlib_pass_if_true("sys value machine not null", 0 != sys->machine,
                    "sys->machine=%p", sys->machine);
  tlib_pass_if_true("sys value distro_id not null", 0 != sys->distro_id,
                    "sys->distro_id=%p", sys->distro_id);
  tlib_pass_if_true("sys value distro_version_id not null",
                    0 != sys->distro_version_id, "sys->distro_version_id=%p",
                    sys->distro_version_id);
  tlib_pass_if_true("sys value libc_version not null", 0 != sys->libc_version,
                    "sys->libc_version=%p", sys->libc_version);
  tlib_pass_if_true(
      "correct libc name is set",
      sys->libc_version == nr_strstr(sys->libc_version, LIBC_NAME),
      "unexpected libc_version=%s", sys->libc_version);

  nr_system_destroy(&sys);
  tlib_pass_if_true("sys destroyed", 0 == sys, "sys=%p", sys);
}

static void test_system_destroy_bad_params(void) {
  nr_system_t* sys;

  /* Don't blow up! */
  nr_system_destroy(0);
  sys = 0;
  nr_system_destroy(&sys);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_system_get_hostname();
  test_get_system();
  test_system_destroy_bad_params();
  test_get_system_info_from_osrelease();
}
