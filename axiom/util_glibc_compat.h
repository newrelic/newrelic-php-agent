/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef UTIL_GLIBC_COMPAT_HDR
#define UTIL_GLIBC_COMPAT_HDR

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#if defined(__GLIBC__)
#if (((__GLIBC__) > 2) || (((__GLIBC__) == 2) && ((__GLIBC_MINOR__) > 17)))
#define NR_GLIBC_COMPAT 1
#if defined(__x86_64__)
#define NR_XSTAT_VER 1
__asm__(".symver pow,pow@GLIBC_2.2.5");
__asm__(".symver __xstat,__xstat@GLIBC_2.2.5");
__asm__(".symver sscanf,__isoc99_sscanf@GLIBC_2.7");
__asm__(".symver strtol,strtol@GLIBC_2.2.5");
__asm__(".symver __isoc23_strtol,strtol@GLIBC_2.2.5");
__asm__(".symver strtoll,strtoll@GLIBC_2.2.5");
__asm__(".symver __isoc23_strtoll,strtoll@GLIBC_2.2.5");
__asm__(".symver strtoul,strtoul@GLIBC_2.2.5");
__asm__(".symver __isoc23_strtoul,strtoul@GLIBC_2.2.5");
__asm__(".symver strtoull,strtoull@GLIBC_2.2.5");
__asm__(".symver __isoc23_strtoull,strtoull@GLIBC_2.2.5");
__asm__(".symver strtoimax,strtoimax@GLIBC_2.2.5");
__asm__(".symver __isoc23_strtoimax,strtoimax@GLIBC_2.2.5");
__asm__(".symver dlopen,dlopen@GLIBC_2.2.5");
__asm__(".symver dlsym,dlsym@GLIBC_2.2.5");
__asm__(".symver dlclose,dlclose@GLIBC_2.2.5");
__asm__(".symver dladdr,dladdr@GLIBC_2.2.5");
__asm__(".symver pthread_join,pthread_join@GLIBC_2.2.5");
__asm__(".symver pthread_create,pthread_create@GLIBC_2.2.5");
__asm__(".symver pthread_mutexattr_init,pthread_mutexattr_init@GLIBC_2.2.5");
__asm__(
    ".symver pthread_mutexattr_settype,pthread_mutexattr_settype@GLIBC_2.2.5");
__asm__(
    ".symver pthread_mutexattr_destroy,pthread_mutexattr_destroy@GLIBC_2.2.5");
#elif defined(__aarch64__)
#define NR_XSTAT_VER 0
__asm__(".symver pow,pow@GLIBC_2.17");
__asm__(".symver __xstat,__xstat@GLIBC_2.17");
__asm__(".symver sscanf,__isoc99_sscanf@GLIBC_2.17");
__asm__(".symver strtol,strtol@GLIBC_2.17");
__asm__(".symver __isoc23_strtol,strtol@GLIBC_2.17");
__asm__(".symver strtoll,strtoll@GLIBC_2.17");
__asm__(".symver __isoc23_strtoll,strtoll@GLIBC_2.17");
__asm__(".symver strtoul,strtoul@GLIBC_2.17");
__asm__(".symver __isoc23_strtoul,strtoul@GLIBC_2.17");
__asm__(".symver strtoull,strtoull@GLIBC_2.17");
__asm__(".symver __isoc23_strtoull,strtoull@GLIBC_2.17");
__asm__(".symver strtoimax,strtoimax@GLIBC_2.17");
__asm__(".symver __isoc23_strtoimax,strtoimax@GLIBC_2.17");
__asm__(".symver dlopen,dlopen@GLIBC_2.17");
__asm__(".symver dlsym,dlsym@GLIBC_2.17");
__asm__(".symver dlclose,dlclose@GLIBC_2.17");
__asm__(".symver dladdr,dladdr@GLIBC_2.17");
__asm__(".symver pthread_join,pthread_join@GLIBC_2.17");
__asm__(".symver pthread_create,pthread_create@GLIBC_2.17");
__asm__(".symver pthread_mutexattr_init,pthread_mutexattr_init@GLIBC_2.17");
__asm__(
    ".symver pthread_mutexattr_settype,pthread_mutexattr_settype@GLIBC_2.17");
__asm__(
    ".symver pthread_mutexattr_destroy,pthread_mutexattr_destroy@GLIBC_2.17");
#endif
#endif
#endif

#ifdef NR_XSTAT_VER
extern int __xstat(int ver, const char* path, struct stat* buf);
#define stat(p, b) __xstat(NR_XSTAT_VER, (p), (b))
#endif

#ifdef NR_GLIBC_COMPAT
extern int __isoc99_sscanf(const char* __restrict __s,
                           const char* __restrict __format,
                           ...);
#define sscanf __isoc99_sscanf
#undef NR_GLIBC_COMPAT
#endif

#endif
