/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This header exposes php_execute.c internals for unit testing.
 */
#ifndef PHP_EXECUTE_PRIVATE_HDR
#define PHP_EXECUTE_PRIVATE_HDR

/*
 * Contract for magic-file callbacks
 * =================================
 *
 * nr_php_user_instrumentation_from_file() matches a filename against the magic
 * files listed in the tables below and invokes the matching callback: a
 * framework's enable() or special(), a library's or logging framework's
 * enable(), a vulnerability-management package's enable(), or
 * nr_composer_handle_autoload() for the fixed autoload magic file. Everything
 * in this section applies to all of them, and transitively to every function
 * they call.
 *
 * These callbacks do not all do the same kind of work - some register
 * instrumentation, some only extract a version, some only record telemetry,
 * some only write agent-internal state. What they have in common is the
 * operating conditions described here, which are looser than they may appear
 * from any single call site.
 *
 * When they run
 * -------------
 *
 * There are two independent call sites into
 * nr_php_user_instrumentation_from_file(), with different conditions. A
 * callback must be correct under both:
 *
 *   a) nr_php_execute_file(), from nr_php_fcall_register_handlers() - the
 *      Observer API's zend_observer_fcall_init callback (agent/php_observer.c).
 *      Zend calls that once per op_array, the first time the op_array is ever
 *      executed, i.e. whenever userland happens to require() the magic file.
 *      The agent does not control that moment, and *there may be no
 *      transaction at it*: NRPRG(txn) can legitimately be NULL.
 *
 *   b) nr_php_user_instrumentation_from_opcache(), from nr_php_txn_begin()
 *      (agent/php_txn.c). When opcache.preload is in use, this walks the
 *      opcache script list against the same tables as transactions start, so a
 *      given magic file may be offered again on later transaction starts, over
 *      the life of the process. Here NRPRG(txn) is always non-NULL and freshly
 *      created.
 *
 * So a callback may run:
 *
 *   - with NRPRG(txn) == NULL (call site (a) only). This is not an error path.
 *     It happens when the magic file is first executed before any transaction
 *     exists for the current PHP request - e.g. during a persistent worker's
 *     bootstrap/warm-up before its first logical request (FrankenPHP worker
 *     mode serves many HTTP requests inside one PHP request) - or after
 *     newrelic_end_transaction() has destroyed the request's transaction
 *     (nr_php_txn_end() frees it and leaves NRPRG(txn) NULL);
 *   - with a transaction that exists but is not recording
 *     (nr_php_recording() == 0), which is what newrelic_ignore_transaction()
 *     leaves behind: nr_txn_ignore() clears status.recording but keeps the
 *     transaction, so NRPRG(txn) stays non-NULL;
 *   - more than once per process for the same file (call site (b)).
 *
 * Classify your effects by scope
 * ------------------------------
 *
 * The single question that resolves nearly every case: at what scope does this
 * effect live? Each row below is expanded in the correspondingly-numbered note
 * that follows.
 */
/* clang-format off */
/*
 * | #  | Scope       | Examples                                           | Needs txn? | If NRPRG(txn) is NULL   |
 * | -- | ----------- | -------------------------------------------------- | ---------- | ----------------------- |
 * | S1 | Process     | wraprec registration, NR_PHP_PROCESS_GLOBALS(...)  | No         | Must still happen       |
 * | S2 | Request     | NRPRG(...), NRPRG_SHARED(...)                      | No         | Must still happen       |
 * | S3 | Transaction | nr_fw_support_add_library_supportability_metric(), | Yes        | Attempt it; let it drop |
 * |    |             | nr_fw_support_add_logging_supportability_metric(), |            |                         |
 * |    |             | nr_txn_add_php_package(),                          |            |                         |
 * |    |             | nr_txn_suggest_package_supportability_metric(),    |            |                         |
 * |    |             | nr_txn_set_path()                                  |            |                         |
 */
/* clang-format on */
/*
 * S1. Process-scoped effects outlive the request that triggered them, and the
 *     callback may not be invoked again at a more convenient time - so
 *     skipping them is a permanent loss, not a deferral. Registering
 *     instrumentation is the clearest example: nr_php_wrap_user_function() and
 *     friends write to the process-global wraprec hashmap, need no
 *     transaction, and are what *later* requests rely on to instrument
 *     anything at all. This is the main reason these callbacks must work with
 *     a NULL txn rather than bail out.
 *
 * S2. Request-scoped effects are valid with no transaction too, but mind that
 *     RINIT resets some of them: NRPRG_SHARED(current_framework) goes back to
 *     NR_FW_UNSET on every request (agent/php_rinit.c), so a framework
 *     detected in one request is forgotten by the next, and only its
 *     process-scoped effects persist.
 *
 * S3. Transaction-scoped effects are the only ones contingent on NRPRG(txn),
 *     and the correct handling is to attempt them anyway and let them drop.
 *     Every helper listed above no-ops on a NULL txn by design; pass
 *     NRPRG(txn) and rely on that. A datum attributed to a transaction that
 *     does not exist has nowhere to go, and dropping it is the right outcome -
 *     do not stash it for "later" unless the state is genuinely process-scoped
 *     (S1), because the transaction that would have received it is never
 *     coming back. Any new helper taking a txn should get the same
 *     NULL-tolerant contract at its own top.
 *
 * Rules
 * -----
 *
 * Do not bail out early just because NRPRG(txn) is NULL. A missing transaction
 * says nothing about whether the process- and request-scoped work above should
 * happen, and a blanket early return sacrifices all of it to protect calls
 * that are already NULL-safe.
 *
 * Do not dereference NRPRG(txn), NRTXN(...) or NRTXNGLOBAL(...) directly. The
 * guard belongs at the call site or inside the callee, never as a blanket
 * early return in the callback. See nr_fw_*_add_*_supportability_metric(),
 * which guards inside the callee so no caller has to.
 *
 * Do not assume any class, interface, function or constant exists. Only the
 * magic file itself is known to have executed, and nothing else about the
 * library's load order is guaranteed. At call site (b) the filename comes from
 * the opcache script list, so nothing has necessarily executed in this request
 * at all - code that reaches into userland (zend_eval_string(), nr_php_call(),
 * class or global lookups, as *_version() callbacks do) must tolerate the
 * target being absent and degrade rather than fail.
 *
 * Tolerate being called more than once. Wraprec registration is already
 * idempotent (nr_php_user_instrument_wraprec_hashmap_add() reuses an existing
 * wraprec instead of adding a duplicate), so it needs no guard. Other work
 * does not get that for free: version extraction, package records and path
 * naming re-run on each invocation and must be correct and not unboundedly
 * expensive when repeated. Where work is genuinely once-per-process, gate it
 * on process-global state (NR_PHP_PROCESS_GLOBALS(...)), never on the presence
 * of a transaction.
 */

/*
 * Purpose: Enable monitoring on specific functions in the framework.
 */
typedef void (*nr_framework_enable_fn_t)(TSRMLS_D);

/*
 * Purpose: Enable monitoring on specific functions for a detected library.
 */
typedef void (*nr_library_enable_fn_t)(TSRMLS_D);

/*
 * Purpose: Enable monitoring on specific functions for a detected vulnerability
 *          management package.
 */
typedef void (*nr_vuln_mgmt_enable_fn_t)();

typedef struct _nr_framework_table_t {
  const char* framework_name;
  const char* config_name;
  const char* file_to_check;
  size_t file_to_check_len;
  nr_framework_special_fn_t special;
  nr_framework_enable_fn_t enable;
  nrframework_t detected;
} nr_framework_table_t;

typedef struct _nr_library_table_t {
  const char* library_name;
  const char* file_to_check;
  size_t file_to_check_len;
  nr_library_enable_fn_t enable;
} nr_library_table_t;

typedef struct _nr_vuln_mgmt_table_t {
  const char* package_name;
  const char* file_to_check;
  size_t file_to_check_len;
  nr_vuln_mgmt_enable_fn_t enable;
} nr_vuln_mgmt_table_t;

extern const nr_framework_table_t all_frameworks[];
extern const int num_all_frameworks;

extern const nr_library_table_t libraries[];
extern const size_t num_libraries;

extern const nr_library_table_t logging_frameworks[];
extern const size_t num_logging_frameworks;

extern const nr_vuln_mgmt_table_t vuln_mgmt_packages[];
extern const size_t num_packages;

/*
 * Purpose : ONLY for testing to verify library/framework/logging-framework
 *           detection behavior directly, without going through
 *           nr_php_execute_file (which also executes the file's op array).
 *
 *           Detect library and framework usage from a PHP file. Enables a
 *           library or framework if the passed file is defined as a key
 *           file for this library or framework.
 *
 * Params  : 1. Full name of a PHP file.
 *           2. Length of the file name.
 */
extern void nr_php_user_instrumentation_from_file(const char* filename,
                                                  const size_t filename_len);

#endif /* PHP_EXECUTE_PRIVATE_HDR */
