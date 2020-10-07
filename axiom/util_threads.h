/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains a very thin abstraction of the POSIX threads API.
 */
#ifndef UTIL_THREADS_HDR
#define UTIL_THREADS_HDR

#include <sys/types.h>

#include <pthread.h>
#include <signal.h>

#include "nr_axiom.h"

typedef pthread_mutex_t nrthread_mutex_t;
typedef pthread_t nrthread_t;
typedef pthread_attr_t nrthread_attr_t;
typedef pthread_mutexattr_t nrthread_mutexattr_t;

#define NRTHREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

typedef void*(nrt_start_routine_t)(void*);

/*
 * Purpose : Create a new thread.
 * Returns : NR_SUCCESS or NR_FAILURE.
 * See     :
 * http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_create.html
 */
extern nr_status_t nrt_create_f(nrthread_t* thread,
                                const nrthread_attr_t* attr,
                                void*(start_routine)(void*),
                                void* arg,
                                const char* file,
                                int line);

/*
 * Purpose : Initializes or destroys a mutex.
 * Returns : NR_SUCCESS or NR_FAILURE.
 * See     :
 * http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_mutex_init.html
 */
extern nr_status_t nrt_mutex_init_f(nrthread_mutex_t* mutex,
                                    const nrthread_mutexattr_t* attr,
                                    const char* file,
                                    int line);

extern nr_status_t nrt_mutex_destroy_f(nrthread_mutex_t* mutex,
                                       const char* file,
                                       int line);

/*
 * Purpose : Lock or unlock a mutex.
 * Returns : NR_SUCCESS or NR_FAILURE.
 * See     :
 * http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_mutex_lock.html
 */
extern nr_status_t nrt_mutex_lock_f(nrthread_mutex_t* mutex,
                                    const char* file,
                                    int line);
extern nr_status_t nrt_mutex_unlock_f(nrthread_mutex_t* mutex,
                                      const char* file,
                                      int line);

/*
 * Purpose : Wait for thread termination.
 * Returns : NR_SUCCESS or NR_FAILURE.
 * See     :
 * http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_join.html
 */
extern nr_status_t nrt_join_f(nrthread_t thread,
                              void** valptr,
                              const char* file,
                              int line);

/* Wrap each nrt_* function with a macro to insert the file and line info. */
#define nrt_create(T, A, S, P) \
  nrt_create_f((T), (A), (S), (P), __FILE__, __LINE__)
#define nrt_mutex_init(T, A) nrt_mutex_init_f((T), (A), __FILE__, __LINE__)
#define nrt_mutex_lock(T) nrt_mutex_lock_f((T), __FILE__, __LINE__)
#define nrt_mutex_unlock(T) nrt_mutex_unlock_f((T), __FILE__, __LINE__)
#define nrt_mutex_destroy(T) nrt_mutex_destroy_f((T), __FILE__, __LINE__)
#define nrt_join(T, V) nrt_join_f((T), (V), __FILE__, __LINE__)

/*
 * Set up a nrt_thread_local storage class for thread local variables.
 *
 * This is essentially cribbed from
 * https://stackoverflow.com/questions/18298280/how-to-declare-a-variable-as-thread-local-portably,
 * just without the compilers we don't care about.
 *
 * If we're adding support for a new OS, let's try really hard to use a C11
 * compiler so that we can just use _Thread_local and move on with our lives.
 */
#if __STDC_VERSION__ >= 201112 && !defined(__STDC_NO_THREADS__)
#define nrt_thread_local _Thread_local
#elif defined(__GNUC__)
#define nrt_thread_local __thread
#else
#error "Unsupported compiler: don't know how to define thread local variables."
#endif

#endif /* UTIL_THREADS_HDR */
