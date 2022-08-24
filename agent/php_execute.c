code_level_metrics(
    nr_php_execute_metadata_t* metadata,
    NR_EXECUTE_PROTO) {
#if ZEND_MODULE_API_NO < ZEND_7_0_X_API_NO /* PHP7+ */
  (void)metadata;
  NR_UNUSED_SPECIALFN;
  return;
#else
  const char* filepath = NULL;
  const char* namespace = NULL;
  const char* function = NULL;
  uint32_t lineno = 1;

  metadata->scope = NULL;
  metadata->function = NULL;
  metadata->function_name = NULL;
  metadata->function_filepath = NULL;
  metadata->function_namespace = NULL;

  if (NULL == execute_data) {
    return;
  }

  /*
   * Check if code level metrics are enabled in the ini.
   * If they aren't, exit and don't update CLM.
   */
  if (!NRINI(code_level_metrics_enabled)) {
    return;
  }
  /*
   * At a minimum, at least one of the following attribute combinations MUST be
   * implemented in order for customers to be able to accurately identify their
   * instrumented functions:
   *  - code.filepath AND code.function
   *  - code.namespace AND code.function
   *
   * If we don't have the minimum requirements, exit and don't add any
   * attributes.
   */

  filepath = nr_php_zend_execute_data_filename(execute_data);
  namespace = nr_php_zend_execute_data_scope_name(execute_data);
  function = nr_php_zend_execute_data_function_name(execute_data);
  lineno = nr_php_zend_execute_data_lineno(execute_data);

  /*
   * Check if we are getting CLM for a file.
   */
  if (nrunlikely(OP_ARRAY_IS_A_FILE(NR_OP_ARRAY))) {
    /*
     * If instrumenting a file, the filename is the "function" and the
     * lineno is 1 (i.e., start of the file).
     */
    function = filepath;
    lineno = 1;
  } else {
    /*
     * We are getting CLM for a function.
     */
    lineno = nr_php_zend_execute_data_lineno(execute_data);
  }
  if (nr_strempty(function)) {
    return;
  }
  if (nr_strempty(namespace) && nr_strempty(filepath)) {
    /*
     * CLM MUST have either function+namespace or function+filepath.
     */
    return;
  }

  metadata->function_lineno = lineno;
  metadata->function_name = nr_strdup(function);
  metadata->function_namespace = nr_strdup(namespace);
  metadata->function_filepath = nr_strdup(filepath);

#endif /* PHP7 */
}

/*
 * Purpose : Initialise a metadata structure from an op array.
 *
 * Params  : 1. A pointer to a metadata structure.
 *           2. The op array.
 *
 * Note    : It is the responsibility of the caller to allocate the metadata
 *           structure. In general, it's expected that this will be a pointer
 *           to a stack variable.
 */
static void nr_php_execute_metadata_init(nr_php_execute_metadata_t* metadata,
                                         zend_op_array* op_array) {
#ifdef PHP7
  if (op_array->scope && op_array->scope->name && op_array->scope->name->len) {
    metadata->scope = op_array->scope->name;
    zend_string_addref(metadata->scope);
  } else {
    metadata->scope = NULL;
  }

  if (op_array->function_name && op_array->function_name->len) {
    metadata->function = op_array->function_name;
    zend_string_addref(metadata->function);
  } else {
    metadata->function = NULL;
  }
#else
  metadata->op_array = op_array;
#endif /* PHP7 */
}

/*
 * Purpose : Create a metric name from the given metadata.
 *
 * Params  : 1. A pointer to the metadata.
 *           2. A pointer to an allocated buffer to place the name in.
 *           3. The size of the buffer, in bytes.
 *
 * Warning : No check is made whether buf is valid, as the normal use case for
 *           this involves alloca(), which doesn't signal errors via NULL (or
 *           any other useful return value). Similarly, metadata is unchecked.
 */
static void nr_php_execute_metadata_metric(
    const nr_php_execute_metadata_t* metadata,
    char* buf,
    size_t len) {
  const char* function_name;
  const char* scope_name;

#ifdef PHP7
  scope_name = metadata->scope ? ZSTR_VAL(metadata->scope) : NULL;
  function_name = metadata->function ? ZSTR_VAL(metadata->function) : NULL;
#else
  scope_name = nr_php_op_array_scope_name(metadata->op_array);
  function_name = nr_php_op_array_function_name(metadata->op_array);
#endif /* PHP7 */

  snprintf(buf, len, "Custom/%s%s%s", scope_name ? scope_name : "",
           scope_name ? "::" : "", function_name ? function_name : "<unknown>");
}

/*
 * Purpose : Release any cached op array metadata.
 *
 * Params  : 1. A pointer to the metadata.
 */
static void nr_php_execute_metadata_release(
    nr_php_execute_metadata_t* metadata) {
#ifdef PHP7
  if (NULL != metadata->scope) {
    zend_string_release(metadata->scope);
    metadata->scope = NULL;
  }

  if (NULL != metadata->function) {
    zend_string_release(metadata->function);
    metadata->function = NULL;
  }
  nr_free(metadata->function_name);
  nr_free(metadata->function_namespace);
  nr_free(metadata->function_filepath);
#else
  metadata->op_array = NULL;
#endif /* PHP7 */
}

static inline void nr_php_execute_segment_add_metric(
    nr_segment_t* segment,
    const nr_php_execute_metadata_t* metadata,
    bool create_metric) {
  char buf[METRIC_NAME_MAX_LEN];

  nr_php_execute_metadata_metric(metadata, buf, sizeof(buf));

  if (create_metric) {
    nr_segment_add_metric(segment, buf, true);
  }
  nr_segment_set_name(segment, buf);
}

/*
 * Purpose : Evaluate what the disposition of the given segment is: do we
 *           discard or keep it, and if the latter, do we need to create a
 *           custom metric?
 *
 * Params  : 1. The stacked segment to end.
 *           2. The function naming metadata.
 *           3. Whether to create a metric.
 */
static inline void nr_php_execute_segment_end(
    nr_segment_t* stacked,
    const nr_php_execute_metadata_t* metadata,
    bool create_metric TSRMLS_DC) {
  nrtime_t duration;

  if (NULL == stacked) {
    return;
  }

  stacked->stop_time = nr_txn_now_rel(NRPRG(txn));

  duration = nr_time_duration(stacked->start_time, stacked->stop_time);

  if (create_metric || (duration >= NR_PHP_PROCESS_GLOBALS(expensive_min))
      || nr_vector_size(stacked->metrics) || stacked->id || stacked->attributes
      || stacked->error) {
    nr_segment_t* s = nr_php_stacked_segment_move_to_heap(stacked TSRMLS_CC);
    nr_php_execute_segment_add_metric(s, metadata, create_metric);
    if (NULL == s->attributes) {
      s->attributes = nr_attributes_create(s->txn->attribute_config);
    }
    nr_php_txn_add_code_level_metrics(s->attributes, metadata);
    nr_segment_end(&s);
  } else {
    nr_php_stacked_segment_deinit(stacked TSRMLS_CC);
  }
}

/*
 * This is the user function execution hook. Hook the user-defined (PHP)
 * function execution. For speed, we have a pointer that we've installed in the
 * function record as a flag to indicate whether to instrument this function.
 * If the flag is NULL, then we've only added a couple of CPU instructions to
 * the call path and thus the overhead is (hopefully) very low.
 */
static void nr_php_execute_enabled(NR_EXECUTE_PROTO TSRMLS_DC) {
  int zcaught = 0;
  nrtime_t txn_start_time;
  nr_php_execute_metadata_t metadata;
  nr_segment_t stacked = {0};
  nr_segment_t* segment;
  nruserfn_t* wraprec;

  NRTXNGLOBAL(execute_count) += 1;

  nr_php_execute_metadata_add_code_level_metrics(&metadata,
                                                 NR_EXECUTE_ORIG_ARGS);

  if (nrunlikely(OP_ARRAY_IS_A_FILE(NR_OP_ARRAY))) {
    if (NRPRG(txn)) {
      nr_php_txn_add_code_level_metrics(NRPRG(txn)->attributes, &metadata);
    }
    nr_php_execute_file(NR_OP_ARRAY, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    nr_php_execute_metadata_release(&metadata);
    return;
  }

  /*
   * The function name needs to be checked before the NR_OP_ARRAY->fn_flags
   * since in PHP 5.1 fn_flags is not initialized for files.
   */

  wraprec = nr_php_op_array_get_wraprec(NR_OP_ARRAY TSRMLS_CC);

  if (NULL != wraprec) {
    /*
     * This is the case for specifically requested custom instrumentation.
     */
    bool create_metric = wraprec->create_metric;

    nr_php_execute_metadata_init(&metadata, NR_OP_ARRAY);

    nr_txn_force_single_count(NRPRG(txn), wraprec->supportability_metric);

    /*
     * Check for, and handle, frameworks.
     */

    if (wraprec->is_names_wt_simple) {
      nr_txn_name_from_function(NRPRG(txn), wraprec->funcname,
                                wraprec->classname);
    }

    /*
     * The nr_txn_should_create_span_events() check is there so we don't record
     * error attributes on the txn (and root segment) because it should already
     * be recorded on the span that exited unhandled.
     */
    if (wraprec->is_exception_handler
        && !nr_txn_should_create_span_events(NRPRG(txn))) {
      zval* exception
          = nr_php_get_user_func_arg(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

      /*
       * The choice of E_ERROR for the error level is basically arbitrary, but
       * matches the error level PHP uses if there isn't an exception handler,
       * so this should give more consistency for the user in terms of what
       * they'll see with and without an exception handler installed.
       */
      nr_php_error_record_exception(
          NRPRG(txn), exception, nr_php_error_get_priority(E_ERROR),
          "Uncaught exception ", &NRPRG(exception_filters) TSRMLS_CC);
    }

    txn_start_time = nr_txn_start_time(NRPRG(txn));

    segment = nr_php_stacked_segment_init(&stacked TSRMLS_CC);
    zcaught = nr_zend_call_orig_execute_special(wraprec, segment,
                                                NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

    /*
     * During this call, the transaction may have been ended and/or a new
     * transaction may have started.  To detect this, we compare the
     * currently active transaction's start time with the transaction
     * start time we saved before.
     *
     * Just comparing the transaction pointer is not enough, as a newly
     * started transaction might actually obtain the same address as a
     * transaction freed before.
     */
    if (nrunlikely(nr_txn_start_time(NRPRG(txn)) != txn_start_time)) {
      segment = NULL;
    }

    nr_php_execute_segment_end(segment, &metadata, create_metric TSRMLS_CC);

    if (nrunlikely(zcaught)) {
      zend_bailout();
    }
  } else if (NRINI(tt_detail) && NR_OP_ARRAY->function_name) {
    nr_php_execute_metadata_init(&metadata, NR_OP_ARRAY);

    /*
     * This is the case for transaction_tracer.detail >= 1 requested custom
     * instrumentation.
     */

    txn_start_time = nr_txn_start_time(NRPRG(txn));

    segment = nr_php_stacked_segment_init(&stacked TSRMLS_CC);

    zcaught = nr_zend_call_orig_execute_special(wraprec, &stacked,
                                                NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

    if (nr_txn_should_create_span_events(NRPRG(txn))) {
      if (EG(exception)) {
        zval* exception_zval = NULL;
        nr_status_t status;

#ifdef PHP7
        /*
         * On PHP 7, EG(exception) is stored as a zend_object, and is only
         * wrapped in a zval when it actually needs to be.
         */
        zval exception;

        ZVAL_OBJ(&exception, EG(exception));
        exception_zval = &exception;
#else
        /*
         * On PHP 5, the exception is just a regular old zval.
         */
        exception_zval = EG(exception);
#endif /* PHP7 */

        status = nr_php_error_record_exception_segment(
            NRPRG(txn), exception_zval, &NRPRG(exception_filters) TSRMLS_CC);

        if (NR_FAILURE == status) {
          nrl_verbosedebug(
              NRL_AGENT, "%s: unable to record exception on segment", __func__);
        }
      }
    }

    /*
     * During this call, the transaction may have been ended and/or a new
     * transaction may have started.  To detect this, we compare the
     * currently active transaction's start time with the transaction
     * start time we saved before.
     */
    if (nrunlikely(nr_txn_start_time(NRPRG(txn)) != txn_start_time)) {
      segment = NULL;
    }

    nr_php_execute_segment_end(segment, &metadata, false TSRMLS_CC);

    if (nrunlikely(zcaught)) {
      zend_bailout();
    }
  } else {
    /*
     * This is the case for New Relic is enabled, but we're not recording.
     */
    NR_PHP_PROCESS_GLOBALS(orig_execute)(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  }
  nr_php_execute_metadata_release(&metadata);
}

static void nr_php_execute_show(NR_EXECUTE_PROTO TSRMLS_DC) {
  if (nrunlikely(NR_PHP_PROCESS_GLOBALS(special_flags).show_executes)) {
    nr_php_show_exec(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  }

  nr_php_execute_enabled(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (nrunlikely(NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_returns)) {
    nr_php_show_exec_return(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  }
}

static void nr_php_max_nesting_level_reached(TSRMLS_D) {
  /*
   * Reset the stack depth to ensure that when php_error is done executing
   * longjmp to discard all of the C frames and PHP frames, that the stack
   * depth is correct. Execution will probably not continue after E_ERROR;
   * that decision may rest on the error handler(s) registered as callbacks.
   */
  NRPRG(php_cur_stack_depth) = 0;

  nrl_error(NRL_AGENT,
            "The New Relic imposed maximum PHP function nesting level of '%d' "
            "has been reached. "
            "If you think this limit is too small, adjust the value of the "
            "setting newrelic.special.max_nesting_level in the newrelic.ini "
            "file, and restart php.",
            (int)NRINI(max_nesting_level));

  php_error(E_ERROR,
            "Aborting! "
            "The New Relic imposed maximum PHP function nesting level of '%d' "
            "has been reached. "
            "This limit is to prevent the PHP execution from catastrophically "
            "running out of C-stack frames. "
            "If you think this limit is too small, adjust the value of the "
            "setting newrelic.special.max_nesting_level in the newrelic.ini "
            "file, and restart php. "
            "Please file a ticket at https://support.newrelic.com if you need "
            "further assistance. ",
            (int)NRINI(max_nesting_level));
}

/*
 * This function is single entry, single exit, so that we can keep track
 * of the PHP stack depth. NOTE: the stack depth is not maintained in
 * the presence of longjmp as from zend_bailout when processing zend internal
 * errors, as for example when calling php_error.
 */
void nr_php_execute(NR_EXECUTE_PROTO TSRMLS_DC) {
  /*
   * We do not use zend_try { ... } mechanisms here because zend_try
   * involves a setjmp, and so may be too expensive along this oft-used
   * path. We believe that the corresponding zend_catch will only be
   * taken when there's an internal zend error, and execution will some
   * come to a controlled premature end. The corresponding zend_catch
   * is NOT called when PHP exceptions are thrown, which happens
   * (relatively) frequently.
   *
   * The only reason for bracketing this with zend_try would be to
   * maintain the consistency of the php_cur_stack_depth counter, which
   * is only used for clamping the depth of PHP stack execution, or for
   * pretty printing PHP stack frames in nr_php_execute_show. Since the
   * zend_catch is called to avoid catastrophe on the way to a premature
   * exit, maintaining this counter perfectly is not a necessity.
   */

  NRPRG(php_cur_stack_depth) += 1;

  if (((int)NRINI(max_nesting_level) > 0)
      && (NRPRG(php_cur_stack_depth) >= (int)NRINI(max_nesting_level))) {
    nr_php_max_nesting_level_reached(TSRMLS_C);
  }

  if (nrunlikely(0 == nr_php_recording(TSRMLS_C))) {
    NR_PHP_PROCESS_GLOBALS(orig_execute)(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  } else {
    int show_executes
        = NR_PHP_PROCESS_GLOBALS(special_flags).show_executes
          || NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_returns;

    if (nrunlikely(show_executes)) {
      nr_php_execute_show(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    } else {
      nr_php_execute_enabled(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    }
  }
  NRPRG(php_cur_stack_depth) -= 1;

  return;
}

static void nr_php_show_exec_internal(NR_EXECUTE_PROTO,
                                      const zend_function* func TSRMLS_DC) {
  char argstr[NR_EXECUTE_DEBUG_STRBUFSZ] = {'\0'};
  const char* name = nr_php_function_debug_name(func);

  nr_show_execute_params(NR_EXECUTE_ORIG_ARGS, argstr TSRMLS_CC);
  nrl_verbosedebug(
      NRL_AGENT,
      "execute: %.*s function={" NRP_FMT_UQ "} params={" NRP_FMT_UQ "}",
      nr_php_show_exec_indentation(TSRMLS_C), nr_php_indentation_spaces,
      NRP_PHP(name ? name : "?"), NRP_ARGSTR(argstr));
}

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO
#define CALL_ORIGINAL \
  (NR_PHP_PROCESS_GLOBALS(orig_execute_internal)(execute_data, return_value))

void nr_php_execute_internal(zend_execute_data* execute_data,
                             zval* return_value NRUNUSED)
#elif ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
#define CALL_ORIGINAL                                               \
  (NR_PHP_PROCESS_GLOBALS(orig_execute_internal)(execute_data, fci, \
                                                 return_value_used TSRMLS_CC))

void nr_php_execute_internal(zend_execute_data* execute_data,
                             zend_fcall_info* fci,
                             int return_value_used TSRMLS_DC)
#else
#define CALL_ORIGINAL                                          \
  (NR_PHP_PROCESS_GLOBALS(orig_execute_internal)(execute_data, \
                                                 return_value_used TSRMLS_CC))

void nr_php_execute_internal(zend_execute_data* execute_data,
                             int return_value_used TSRMLS_DC)
#endif
{
  nrtime_t duration = 0;
  zend_function* func = NULL;
  nr_segment_t* segment;

  if (nrunlikely(!nr_php_recording(TSRMLS_C))) {
    CALL_ORIGINAL;
    return;
  }

  if (nrunlikely(NULL == execute_data)) {
    nrl_verbosedebug(NRL_AGENT, "%s: NULL execute_data", __func__);
    CALL_ORIGINAL;
    return;
  }

#ifdef PHP7
  func = execute_data->func;
#else
  func = execute_data->function_state.function;
#endif /* PHP7 */

  if (nrunlikely(NULL == func)) {
    nrl_verbosedebug(NRL_AGENT, "%s: NULL func", __func__);
    CALL_ORIGINAL;
    return;
  }

  /*
   * Handle the show_executes flags except for show_execute_returns. Getting
   * the return value reliably across versions is hard; given that the likely
   * number of times we'll want the intersection of internal function
   * instrumentation enabled, show_executes enabled, _and_
   * show_execute_returns enabled is zero, let's not spend the time
   * implementing it.
   */
  if (nrunlikely(NR_PHP_PROCESS_GLOBALS(special_flags).show_executes)) {
#if ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
    nr_php_show_exec_internal(NR_EXECUTE_ORIG_ARGS, func TSRMLS_CC);
#else
    /*
     * We're passing the same pointer twice. This is inefficient. However, no
     * user is ever likely to be affected, since this is a code path handling
     * a special flag, and it makes the nr_php_show_exec_internal() API cleaner
     * for modern versions of PHP without needing to have another function
     * conditionally compiled.
     */
    nr_php_show_exec_internal((zend_op_array*)func, func TSRMLS_CC);
#endif /* PHP >= 5.5 */
  }

  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  CALL_ORIGINAL;

  duration = nr_time_duration(segment->start_time, nr_txn_now_rel(NRPRG(txn)));
  nr_segment_set_timing(segment, segment->start_time, duration);

  if (duration >= NR_PHP_PROCESS_GLOBALS(expensive_min)) {
    nr_php_execute_metadata_t metadata;

    nr_php_execute_metadata_init(&metadata, (zend_op_array*)func);

    nr_php_execute_segment_add_metric(segment, &metadata, false);

    nr_php_execute_metadata_release(&metadata);
  }

  nr_segment_end(&segment);
}

void nr_php_user_instrumentation_from_opcache(TSRMLS_D) {
  zval* status = NULL;
  zval* scripts = NULL;
  zend_ulong key_num = 0;
  nr_php_string_hash_key_t* key_str = NULL;
  zval* val = NULL;
  const char* filename;

  status = nr_php_call(NULL, "opcache_get_status");

  if (NULL == status) {
    nrl_warning(NRL_INSTRUMENT,
                "User instrumentation from opcache: error obtaining opcache "
                "status, even though opcache.preload is set");
    return;
  }

  if (IS_ARRAY != Z_TYPE_P(status)) {
    nrl_warning(NRL_INSTRUMENT,
                "User instrumentation from opcache: opcache status "
                "information is not an array");
    goto end;
  }

  scripts = nr_php_zend_hash_find(Z_ARRVAL_P(status), "scripts");

  if (NULL == scripts) {
    nrl_warning(NRL_INSTRUMENT,
                "User instrumentation from opcache: missing 'scripts' key in "
                "status information");
    goto end;
  }

  if (IS_ARRAY != Z_TYPE_P(scripts)) {
    nrl_warning(NRL_INSTRUMENT,
                "User instrumentation from opcache: 'scripts' value in status "
                "information is not an array");
    goto end;
  }

  nrl_debug(NRL_INSTRUMENT, "User instrumentation from opcache: started");

  ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(scripts), key_num, key_str, val) {
    (void)key_num;
    (void)val;

    filename = ZEND_STRING_VALUE(key_str);

    nr_php_user_instrumentation_from_file(filename TSRMLS_CC);
  }
  ZEND_HASH_FOREACH_END();

  nrl_debug(NRL_INSTRUMENT, "User instrumentation from opcache: done");

end:
  nr_php_zval_free(&status);
}
