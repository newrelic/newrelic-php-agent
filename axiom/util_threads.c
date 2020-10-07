/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "util_errno.h"
#include "util_logging.h"
#include "util_threads.h"

nr_status_t nrt_create_f(nrthread_t* thread,
                         const nrthread_attr_t* attr,
                         void*(start_routine)(void*),
                         void* arg,
                         const char* file,
                         int line) {
  int ret;

  if (0 == thread) {
    return NR_FAILURE;
  }

  ret = pthread_create((pthread_t*)thread, (const pthread_attr_t*)attr,
                       start_routine, arg);
  if (0 != ret) {
    nrl_error(NRL_THREADS, "nrt_create failed: %.16s [%.150s:%d]",
              nr_errno(ret), file, line);
    return NR_FAILURE;
  }
  return NR_SUCCESS;
}

/*
 * When initializing a mutex, if no thread attributes are defined we define
 * our own to use PTHREAD_MUTEX_ERRORCHECK. This allows us to check for
 * deadlocks and other thread errors.
 */
nr_status_t nrt_mutex_init_f(nrthread_mutex_t* mutex,
                             const nrthread_mutexattr_t* attr,
                             const char* file,
                             int line) {
  int ret;
  pthread_mutexattr_t ourattr;

  if (0 == mutex) {
    return NR_FAILURE;
  }

  (void)pthread_mutexattr_init(&ourattr);
#if defined(PTHREAD_MUTEX_ERRORCHECK_NP) || defined(_BITS_PTHREADTYPES_H)
  (void)pthread_mutexattr_settype(&ourattr, PTHREAD_MUTEX_ERRORCHECK_NP);
#elif defined(PTHREAD_MUTEX_ERRORCHECK) \
    || defined(HAVE_PTHREAD_MUTEX_ERRORCHECK)
  (void)pthread_mutexattr_settype(&ourattr, PTHREAD_MUTEX_ERRORCHECK);
#endif

  if (0 == attr) {
    attr = (const nrthread_mutexattr_t*)&ourattr;
  }

  ret = pthread_mutex_init((pthread_mutex_t*)mutex,
                           (const pthread_mutexattr_t*)attr);
  (void)pthread_mutexattr_destroy(&ourattr);

  if (0 != ret) {
    nrl_error(NRL_THREADS, "nrt_mutex_init failed: %.16s [%.150s:%d]",
              nr_errno(ret), file, line);
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

nr_status_t nrt_mutex_lock_f(nrthread_mutex_t* mutex,
                             const char* file,
                             int line) {
  int ret;

  if (0 == mutex) {
    return NR_FAILURE;
  }

  ret = pthread_mutex_lock((pthread_mutex_t*)mutex);
  if (0 != ret) {
    nrl_error(NRL_THREADS, "nrt_mutex_lock failed: %.16s [%.150s:%d]",
              nr_errno(ret), file, line);
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

nr_status_t nrt_mutex_unlock_f(nrthread_mutex_t* mutex,
                               const char* file,
                               int line) {
  int ret;

  if (0 == mutex) {
    return NR_FAILURE;
  }

  ret = pthread_mutex_unlock((pthread_mutex_t*)mutex);
  if (0 != ret) {
    nrl_error(NRL_THREADS, "nrt_mutex_unlock failed: %.16s [%.150s:%d]",
              nr_errno(ret), file, line);
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

nr_status_t nrt_mutex_destroy_f(nrthread_mutex_t* mutex,
                                const char* file,
                                int line) {
  int ret;

  if (0 == mutex) {
    return NR_FAILURE;
  }

  ret = pthread_mutex_destroy((pthread_mutex_t*)mutex);
  if (0 != ret) {
    nrl_error(NRL_THREADS, "nrt_mutex_destroy failed: %.16s [%.150s:%d]",
              nr_errno(ret), file, line);
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

nr_status_t nrt_join_f(nrthread_t thread,
                       void** valptr,
                       const char* file,
                       int line) {
  int ret;

  ret = pthread_join((pthread_t)thread, valptr);
  if ((0 != ret) && (ESRCH != ret)) {
    nrl_error(NRL_THREADS, "nrt_join failed: %.16s [%.150s:%d]", nr_errno(ret),
              file, line);
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}
