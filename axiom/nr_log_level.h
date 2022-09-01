/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_LOG_LEVEL_HDR
#define NR_LOG_LEVEL_HDR

#include <stdbool.h>

/*
 *  Implementation Based on:
 *      https://www.php-fig.org/psr/psr-3/#5-psrlogloglevel
 *      https://datatracker.ietf.org/doc/html/rfc5424#section-6.2.1
 */

#define LOG_LEVEL_EMERGENCY (0)  // system is unusable
#define LOG_LEVEL_ALERT (1)      // action must be taken immediately
#define LOG_LEVEL_CRITICAL (2)   // critical conditions
#define LOG_LEVEL_ERROR (3)      // error conditions
#define LOG_LEVEL_WARNING (4)    // warning conditions
#define LOG_LEVEL_NOTICE (5)     // normal but significant conditions
#define LOG_LEVEL_INFO (6)       // informational messages
#define LOG_LEVEL_DEBUG (7)      // debug-level messages
#define LOG_LEVEL_UNKNOWN (8)    // NON PSR- Unknown/Undefined log level
#define LOG_LEVEL_DEFAULT (LOG_LEVEL_WARNING)

#define LL_EMER_STR ("EMERGENCY")
#define LL_ALER_STR ("ALERT")
#define LL_CRIT_STR ("CRITICAL")
#define LL_ERRO_STR ("ERROR")
#define LL_WARN_STR ("WARNING")
#define LL_NOTI_STR ("NOTICE")
#define LL_INFO_STR ("INFO")
#define LL_DEBU_STR ("DEBUG")
#define LL_UNKN_STR ("UNKNOWN")

/*
 * @brief       Convert PSR-3 string log level to RFC5424 represenation.
 *
 * @param       str     String Log Level
 * @return      int     RFC5424 Log Level numerical code
 */
extern int nr_log_level_str_to_int(const char* str);

/*
 * @brief       Convert RFC5424 log level to PSR-3 string represenation.
 *
 * @param       level   RFC5424 Log Level
 * @return      char*   PSR-3 String Log Level
 */
extern char* nr_log_level_rfc_to_psr(int level);

#endif /* NR_LOG_LEVEL_HDR */
