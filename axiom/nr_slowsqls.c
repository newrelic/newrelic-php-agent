/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_slowsqls.h"
#include "util_memory.h"
#include "util_logging.h"
#include "util_object.h"
#include "util_sql.h"
#include "util_strings.h"
#include "util_system.h"
#include "util_time.h"

struct _nr_slowsql_t {
  char* metric_name; /* Metric name of call. eg "Database/my_table/insert" */
  uint32_t
      sql_id; /* Hash of obfuscated and normalized sql used for aggregation */
  int count;  /* Number of times this slow sql has occurred */
  nrtime_t total;    /* Total amount of time within these calls */
  nrtime_t min_time; /* The duration of the fastest instance of this SQL call */
  nrtime_t max_time; /* The duration of the slowest instance of this SQL call */
  char* params_json; /* A JSON hash containing a backtrace and possibly an
                        explain plan */
  char* sql; /* The SQL raw or obfuscated as provided by nr_slowsqls_add*/
};

struct _nr_slowsqls_t {
  int slowsqls_used;       /* Number of slow SQLs within this structure */
  int max_slowsqls;        /* Maximum number of slow SQLs that can be stored */
  nr_slowsql_t** slowsqls; /* Array of size max_slowsqls */
};

static uint32_t nr_sql_id(const char* sql) {
  uint32_t sql_id;
  char* obfuscated;

  obfuscated = nr_sql_obfuscate(sql);
  if (0 == obfuscated) {
    return 0;
  }
  sql_id = nr_sql_normalized_id(obfuscated);
  nr_free(obfuscated);

  return sql_id;
}

uint32_t nr_slowsql_id(const nr_slowsql_t* slow) {
  if (slow) {
    return slow->sql_id;
  }
  return 0;
}

int nr_slowsql_count(const nr_slowsql_t* slow) {
  if (slow) {
    return slow->count;
  }
  return 0;
}

nrtime_t nr_slowsql_min(const nr_slowsql_t* slow) {
  if (slow) {
    return slow->min_time;
  }
  return 0;
}

nrtime_t nr_slowsql_max(const nr_slowsql_t* slow) {
  if (slow) {
    return slow->max_time;
  }
  return 0;
}

nrtime_t nr_slowsql_total(const nr_slowsql_t* slow) {
  if (slow) {
    return slow->total;
  }
  return 0;
}

const char* nr_slowsql_metric(const nr_slowsql_t* slow) {
  if (slow) {
    return slow->metric_name;
  }
  return NULL;
}

const char* nr_slowsql_query(const nr_slowsql_t* slow) {
  if (slow) {
    return slow->sql;
  }
  return NULL;
}

const char* nr_slowsql_params(const nr_slowsql_t* slow) {
  if (slow) {
    return slow->params_json;
  }
  return NULL;
}

static nr_slowsql_t* nr_slowsql_dup(const nr_slowsql_t* orig) {
  nr_slowsql_t* duplicate;

  if (0 == orig) {
    return 0;
  }

  duplicate = (nr_slowsql_t*)nr_zalloc(sizeof(*duplicate));

  duplicate->metric_name = nr_strdup(orig->metric_name);
  duplicate->sql_id = orig->sql_id;
  duplicate->count = orig->count;
  duplicate->total = orig->total;
  duplicate->min_time = orig->min_time;
  duplicate->max_time = orig->max_time;
  duplicate->params_json = nr_strdup(orig->params_json);
  duplicate->sql = nr_strdup(orig->sql);

  return duplicate;
}

static void nr_slowsql_merge(nr_slowsql_t* dest, const nr_slowsql_t* src) {
  dest->count += src->count;
  dest->total += src->total;

  if (src->min_time < dest->min_time) {
    dest->min_time = src->min_time;
  }
  if (src->max_time > dest->max_time) {
    dest->max_time = src->max_time;

    /*
     * Take the sql, explain plan, params_json, and metric name from the
     * slowest instance.
     */
    nr_free(dest->metric_name);
    nr_free(dest->params_json);
    nr_free(dest->sql);
    dest->metric_name = nr_strdup(src->metric_name);
    dest->sql = nr_strdup(src->sql);
    dest->params_json = nr_strdup(src->params_json);
  }
}

static void nr_slowsql_destroy(nr_slowsql_t** slowsql_ptr) {
  nr_slowsql_t* slowsql;

  if (0 == slowsql_ptr) {
    return;
  }
  slowsql = *slowsql_ptr;
  if (0 == slowsql) {
    return;
  }

  nr_free(slowsql->metric_name);
  nr_free(slowsql->params_json);
  nr_free(slowsql->sql);
  nr_realfree((void**)slowsql_ptr);
}

void nr_slowsqls_destroy(nr_slowsqls_t** slowsqls_ptr) {
  int i;
  nr_slowsqls_t* slowsqls;

  if (0 == slowsqls_ptr) {
    return;
  }
  slowsqls = *slowsqls_ptr;
  if (0 == slowsqls) {
    return;
  }
  for (i = 0; i < slowsqls->slowsqls_used; i++) {
    nr_slowsql_destroy(&slowsqls->slowsqls[i]);
  }
  nr_free(slowsqls->slowsqls);
  nr_realfree((void**)slowsqls_ptr);
}

nr_slowsqls_t* nr_slowsqls_create(int max_slowsqls) {
  nr_slowsqls_t* slowsqls;

  if (max_slowsqls <= 0) {
    return 0;
  }

  slowsqls = (nr_slowsqls_t*)nr_zalloc(sizeof(nr_slowsqls_t));
  slowsqls->slowsqls_used = 0;
  slowsqls->max_slowsqls = max_slowsqls;
  slowsqls->slowsqls
      = (nr_slowsql_t**)nr_zalloc(max_slowsqls * sizeof(nr_slowsql_t*));

  return slowsqls;
}

int nr_slowsqls_saved(const nr_slowsqls_t* slowsqls) {
  if (slowsqls) {
    return slowsqls->slowsqls_used;
  }
  return 0;
}

const nr_slowsql_t* nr_slowsqls_at(const nr_slowsqls_t* slowsqls, int i) {
  if (NULL == slowsqls) {
    return NULL;
  }
  if (i < 0) {
    return NULL;
  }
  if (i >= slowsqls->slowsqls_used) {
    return NULL;
  }
  return slowsqls->slowsqls[i];
}

static void nr_slowsqls_add_internal(nr_slowsqls_t* slowsqls,
                                     const nr_slowsql_t* slow) {
  int i;
  nrtime_t smallest_max_time;
  int smallest_max_idx;

  if (0 == slowsqls) {
    return;
  }
  if (0 == slow) {
    return;
  }

  /* Check if this is a duplicate */
  for (i = 0; i < slowsqls->slowsqls_used; i++) {
    if (slow->sql_id == slowsqls->slowsqls[i]->sql_id) {
      nr_slowsql_merge(slowsqls->slowsqls[i], slow);
      return;
    }
  }

  /* Insert the slowsql directly if there is room */
  if (slowsqls->slowsqls_used < slowsqls->max_slowsqls) {
    slowsqls->slowsqls[slowsqls->slowsqls_used] = nr_slowsql_dup(slow);
    slowsqls->slowsqls_used++;
    return;
  }

  /* Find the slowsql with the smallest max time */
  smallest_max_idx = 0;
  smallest_max_time = slowsqls->slowsqls[0]->max_time;
  for (i = 1; i < slowsqls->slowsqls_used; i++) {
    if (slowsqls->slowsqls[i]->max_time < smallest_max_time) {
      smallest_max_time = slowsqls->slowsqls[i]->max_time;
      smallest_max_idx = i;
    }
  }

  /* If this new slowsql is slower, replace the fastest */
  if (slow->max_time < smallest_max_time) {
    return;
  }
  nr_slowsql_destroy(&slowsqls->slowsqls[smallest_max_idx]);
  slowsqls->slowsqls[smallest_max_idx] = nr_slowsql_dup(slow);
}

static char* nr_slowsqls_create_params_json(
    const nr_slowsqls_params_t* params) {
  char* json;
  nrobj_t* obj = nro_new_hash();

  if (params->plan_json && ('\0' != *params->plan_json)) {
    nro_set_hash_jstring(obj, "explain_plan", params->plan_json);
  }

  if (params->stacktrace_json) {
    nro_set_hash_jstring(obj, "backtrace", params->stacktrace_json);
  }

  if (params->input_query_json) {
    nro_set_hash_jstring(obj, "input_query", params->input_query_json);
  }

  if (params->instance) {
    if (params->instance_reporting_enabled) {
      nro_set_hash_string(obj, "host", params->instance->host);
      nro_set_hash_string(obj, "port_path_or_id",
                          params->instance->port_path_or_id);
    }
    if (params->database_name_reporting_enabled) {
      nro_set_hash_string(obj, "database_name",
                          params->instance->database_name);
    }
  }

  json = nro_to_json(obj);

  nro_delete(obj);

  return json;
}

void nr_slowsqls_add(nr_slowsqls_t* slowsqls,
                     const nr_slowsqls_params_t* params) {
  nr_slowsql_t slow;

  if (0 == slowsqls) {
    return;
  }
  if (0 == params) {
    return;
  }
  if (0 == params->sql) {
    return;
  }
  if (0 == params->stacktrace_json) {
    return;
  }
  if (0 == params->metric_name) {
    return;
  }
  if (0 == params->duration) {
    return;
  }

  slow.sql_id = nr_sql_id(params->sql);
  if (0 == slow.sql_id) {
    return;
  }

  /*
   * String duplications are used to avoid const correctness warnings.
   * Alternative stategies include adding parameters to nr_slowsqls_add_internal
   * for each the the nr_slowsql_t fields or having a nr_slowsql_const_t
   * structure which is the same as nr_slowsql_t with the strings being const.
   * This strategy is chosen for simplicity.  This is not a hot code path:
   * It will only be called when a slow sql occurs, and by default the
   * slow sql threshold is half a second.
   */
  slow.metric_name = nr_strdup(params->metric_name);
  slow.count = 1;
  slow.total = params->duration;
  slow.min_time = params->duration;
  slow.max_time = params->duration;
  slow.params_json = nr_slowsqls_create_params_json(params);
  slow.sql = nr_strdup(params->sql);

  nr_slowsqls_add_internal(slowsqls, &slow);

  nr_free(slow.params_json);
  nr_free(slow.metric_name);
  nr_free(slow.sql);
}
