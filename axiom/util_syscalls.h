/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file shims system calls to control failure for tests.
 */

/*
 * This header defines the wrapper functions that must be used for all of
 * the critical system calls. These wrappers are designed to be optimal for
 * the normal use case, and to have special "tweaks and knobs" for the unit
 * tests.
 *
 * If you want to add a wrapper around a new system call, you must:
 * 1. Declare the wrapper function below with the same prototype and "nr_"
 *    prefix.
 * 2. Implement the wrapper in util_syscalls.c.
 */

#ifndef UTIL_SYSCALLS_HDR
#define UTIL_SYSCALLS_HDR

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <poll.h>

/*
 * These prototypes conform to the Open Group's Base Single UNIX Specification
 * Issue 6 (IEEE 1003.1), 2004 Edition.
 * See http://pubs.opengroup.org/onlinepubs/009695399/ for details.
 *
 * The two exception to this rule is the declaration of nr_fcntl() and
 * nr_open(). Both of these functions take a variable number of arguments.
 * Since passing around the varargs portion of a functions arguments is
 * highly unportable, we use a declaration that matches our usage - making
 * them always take 3 arguments.
 */
extern int nr_accept(int sock,
                     struct sockaddr* address,
                     socklen_t* address_len_p);
extern int nr_access(const char* path, int amode);
extern int nr_bind(int sock,
                   const struct sockaddr* address,
                   socklen_t address_len);
extern int nr_close(int fd);
extern int nr_connect(int sock,
                      const struct sockaddr* address,
                      socklen_t address_len);
extern int nr_dup(int filedes);
extern int nr_dup2(int a, int b);
extern int nr_fcntl(int fd, int cntl, int code);
extern int nr_fcntl_p(int fd, int cntl, void* ptr);
extern int nr_ftruncate(int fildes, off_t length);
extern int nr_getegid(void);
extern int nr_geteuid(void);
extern int nr_getgid(void);
extern int nr_getpid(void);
extern int nr_getppid(void);
extern int nr_getrusage(int who, struct rusage* r_usage);
extern int nr_gettid(void);
extern int nr_getuid(void);
extern int nr_listen(int fd, int backlog);
extern int nr_open(const char* path, int openflag, int modeflag);
extern int nr_pipe(int fds[2]);
extern int nr_poll(struct pollfd* pfds2, nfds_t nfds, int timeout);
extern ssize_t nr_read(int fd, void* buf, size_t buflen);
extern ssize_t nr_recv(int sock, void* buf, size_t buflen, int flags);
extern int nr_setsockopt(int sock,
                         int level,
                         int option_name,
                         const void* option_value,
                         socklen_t option_len);
extern int nr_socket(int dmn, int stp, int prt);
extern int nr_stat(const char* path, struct stat* statbuf);
extern int nr_unlink(const char* name);
extern ssize_t nr_write(int fd, const void* buf, size_t count);
extern ssize_t nr_writev(int fd, const struct iovec* iovec, int ioveclen);

#endif /* UTIL_SYSCALLS_HDR */
