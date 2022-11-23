/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#if LOOKUP_METHOD == LOOKUP_USE_LINKED_LIST

/*
 * Purpose : Store zend_function's metadata (filename, lineno) in wraprec
 *           for use in looking up the instrumentation.
 * 
 * Params  : 1. The wraprec to update with metadata
 *           2. The zend function with meta data
 *
 * Returns : void
 */
static void wraprec_metadata_set(nruserfn_t* wraprec, zend_function* func) {
    const char* filename = nr_php_function_filename(func);
  /*
   * Before setting a filename, ensure it is not NULL and it doesn't equal the
   * "special" designation given to funcs without filenames. If the function is
   * an evaluated expression or called directly from the CLI there is no
   * filename, but the function says the filename is "-". Avoid setting in this
   * case; otherwise, all the evaluated/cli calls would match.
   */
  if ((NULL == wraprec->filename) && (NULL != filename)
      && (0 != nr_strcmp("-", filename))) {
    wraprec->filename = nr_strdup(filename);
  }

  wraprec->lineno = nr_php_zend_function_lineno(func);

  if (chk_reported_class(func, wraprec)) {
    wraprec->reportedclass = nr_strdup(ZSTR_VAL(func->common.scope->name));
  }
}

/*
 * Purpose : Determine if a func matches a wraprec.
 *
 * Params  : 1. The wraprec to match to a zend function
 *           2. The zend function to match to a wraprec
 *
 * Returns : True if the class/function of a wraprec match the class function
 *           of a zend function.
 */
static inline bool nr_php_wraprec_matches(nruserfn_t* p, zend_function* func) {
  char* klass = NULL;
  const char* filename = NULL;

  /*
   * We are able to match either by lineno/filename pair or funcname/classname
   * pair.
   */

  /*
   * Optimize out string manipulations; don't do them if you don't have to.
   * For instance, if funcname doesn't match, no use comparing the classname.
   */

  if (NULL == p) {
    return false;
  }
  if ((NULL == func) || (ZEND_USER_FUNCTION != func->type)) {
    return false;
  }

  if (0 != p->lineno) {
    /*
     * Lineno is set in the wraprec.  If lineno doesn't match, we can exit without
     * going on to the funcname/classname pair comparison.
     * If lineno matches, but the wraprec filename is NULL, it is inconclusive and we
     * we must do the funcname/classname compare.
     * If lineno matches, wraprec filename is not NULL, and it matches/doesn't match,
     * we can exit without doing the funcname/classname compare.
     */
    if (p->lineno != nr_php_zend_function_lineno(func)) {
      return false;
    } 
    /*
     * lineno matched, let's check the filename
     */
    filename = nr_php_function_filename(func);

    /*
     * If p->filename isn't NULL, we know the comparison is accurate;
     * otherwise, it's inconclusive even if we have a lineno because it
     * could be a cli call or evaluated expression that has no filename.
     */
    if (NULL != p->filename) {
      if (0 == nr_strcmp(p->filename, filename)) {
        return true;
      }
      return false;
    }
  }

  if (NULL == func->common.function_name) {
    return false;
  }

  if (0 != nr_stricmp(p->funcnameLC, ZSTR_VAL(func->common.function_name))) {
    return false;
  }
  if (NULL != func->common.scope && NULL != func->common.scope->name) {
    klass = ZSTR_VAL(func->common.scope->name);
  }

  if ((0 == nr_strcmp(p->reportedclass, klass))
      || (0 == nr_stricmp(p->classname, klass))) {
    /*
     * If we get here it means lineno/filename weren't initially set.
     * Set it now so we can do the optimized compare next time.
     * lineno/filename is usually not set if the func wasn't loaded when we
     * created the initial wraprec and we had to use the more difficult way to
     * set, update it with lineno/filename now.
     */
    if (NULL == p->filename) {
      filename = nr_php_function_filename(func);
      if ((NULL != filename) && (0 != nr_strcmp("-", filename))) {
        p->filename = nr_strdup(filename);
      }
    }
    if (0 == p->lineno) {
      p->lineno = nr_php_zend_function_lineno(func);
    }
    return true;
  }
  return false;
}

/*
 * Purpose : Get the wraprec stored in nr_wrapped_user_functions and associated
 *           with a zend_function.
 *
 * Params  : 1. The zend function to find in a wraprec
 *
 * Returns : The function wrapper that matches the zend_function.
 *           This will first try to match the lineno/filename.  If we don't have
 *           that for any reason (maybe the func didn't exist in the function
 *           table when we first added), it will match by function name/class.
 *           NULL if no function wrapper matches the zend_function.
 */
static inline nruserfn_t* nr_php_get_wraprec_by_func(unsigned *n, zend_function* func) {
    nruserfn_t* p = nr_wrapped_user_functions;
    while (NULL != p) {
      (*n)++;
      if (nr_php_wraprec_matches(p, func)) {
        break;
      }
      p = p->next;
    }
    return p;
}
#endif