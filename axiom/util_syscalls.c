/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>

#if NR_SYSTEM_LINUX
#include <sys/syscall.h>
#endif

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "util_syscalls.h"

int nr_accept(int sock, struct sockaddr* address, socklen_t* address_len_p) {
  return (accept)(sock, address, address_len_p);
}

int nr_access(const char* path, int amode) {
  return (access)(path, amode);
}

int nr_bind(int sock, const struct sockaddr* address, socklen_t address_len) {
  return (bind)(sock, address, address_len);
}

int nr_close(int fd) {
  return (close)(fd);
}

int nr_connect(int sock,
               const struct sockaddr* address,
               socklen_t address_len) {
  return (connect)(sock, address, address_len);
}

int nr_dup(int filedes) {
  return (dup)(filedes);
}

int nr_dup2(int a, int b) {
  return (dup2)(a, b);
}

int nr_fcntl(int fd, int cntl, int code) {
  return (fcntl)(fd, cntl, code);
}

int nr_fcntl_p(int fd, int cntl, void* ptr) {
  return (fcntl)(fd, cntl, ptr);
}

int nr_ftruncate(int fildes, off_t length) {
  return (ftruncate)(fildes, length);
}

int nr_getegid(void) {
  return (int)(getegid)();
}

int nr_geteuid(void) {
  return (int)(geteuid)();
}

int nr_getgid(void) {
  return (int)(getgid)();
}

int nr_getpid(void) {
  return (int)(getpid)();
}

/*
 * Return the thread id.
 *
 * For sanity's sake when debugging, this should match the thread id
 * reported by gdb when gdb reports the creation of new threads, or when
 * gdb switches to other threads, and in the information printed for the
 * "info threads" command.
 *
 * The threadids and how gdb deals with them are very platform specific.
 */
int nr_gettid(void) {
#if defined(__linux__)
  /*
   * See, conveniently:
   *   http://man7.org/linux/man-pages/man2/syscall.2.html
   */
  return syscall(SYS_gettid);

#elif defined(__APPLE__)                                      \
    && defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) \
    && (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060)
  /*
   * See:
   *  http://elliotth.blogspot.com/2012/04/gettid-on-mac-os.html
   *
   * Alas, what pthread_threadid_np returns does not match what gdb prints
   * in messages like
   *   [Switching to process 5977 thread 0x1103]
   * (Note that the the gdbs from xcode and brew have major version differences
   * in the way they report threadids.)
   */
  uint64_t tid = 0;
  pthread_threadid_np(NULL, &tid);
  return (int)tid;

#elif defined(__FreeBSD__)
  /*
   * On FreeBSD, empirically, pthread_self returns a pointer.
   * The low 32 bits of that pointer match the threadid reported by gdb.
   */
  pthread_t my_id = pthread_self(); /* a pointer */
  long my_id_long = (long)my_id;
  return (int)(my_id_long & 0xFFFFFFFF);

#else
# error Unsupported OS: please add the expected mechanism for nr_gettid to this file.
  return -1;
#endif
}

int nr_getppid(void) {
  return (int)(getppid)();
}

int nr_getrusage(int who, struct rusage* r_usage) {
  return (getrusage)(who, r_usage);
}

int nr_getuid(void) {
  return (int)(getuid)();
}

int nr_listen(int fd, int backlog) {
  return (listen)(fd, backlog);
}

int nr_open(const char* path, int openflag, int modeflag) {
  return (open)(path, openflag, modeflag);
}

int nr_pipe(int fds[2]) {
  return (pipe)(fds);
}

int nr_poll(struct pollfd* pfds2, nfds_t nfds, int timeout) {
  return (poll)(pfds2, nfds, timeout);
}

ssize_t nr_read(int fd, void* buf, size_t buflen) {
  return (read)(fd, buf, buflen);
}

ssize_t nr_recv(int sock, void* buf, size_t buflen, int flags) {
  return (recv)(sock, buf, buflen, flags);
}

int nr_setsockopt(int sock,
                  int level,
                  int option_name,
                  const void* option_value,
                  socklen_t option_len) {
  return (setsockopt)(sock, level, option_name, option_value, option_len);
}

int nr_socket(int dmn, int stp, int prt) {
  return (socket)(dmn, stp, prt);
}

int nr_stat(const char* path, struct stat* statbuf) {
  return (stat)(path, statbuf);
}

int nr_unlink(const char* name) {
  return (unlink)(name);
}

ssize_t nr_write(int fd, const void* buf, size_t count) {
  return (write)(fd, buf, count);
}

ssize_t nr_writev(int fd, const struct iovec* iovec, int ioveclen) {
  return (writev)(fd, iovec, ioveclen);
}
