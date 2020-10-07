/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "util_hash.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_sql.h"
#include "util_sql_private.h"
#include "util_strings.h"

char* nr_sql_obfuscate(const char* raw) {
  char* obf;
  const char* p;
  char* q;
  int state = 0;

  if (nrunlikely(0 == raw)) {
    return 0;
  }

  obf = (char*)nr_malloc(nr_strlen(raw) + 1);
  p = raw;
  q = obf;

  while (*p) {
    switch (state) {
      case 0: /* normal */
        switch (*p) {
          case '"':
            p++;
            *q++ = '?';
            state = 1;
            break;

          case '\'':
            p++;
            *q++ = '?';
            state = 2;
            break;

          case '-': /* comment. */
            if ('-' == p[1]) {
              p = nr_strchr(p, '\n');
              if (NULL == p) {
                goto done;
              }
              p++;
            } else {
              *q++ = *p++;
            }
            break;

          case '/': /* checking for c-style comments */
            if ('*' == p[1]) {
              p = nr_strstr(p, "*/");
              if (NULL == p) {
                goto done;
              }
              p += 2;
            } else {
              *q++ = *p++;
            }
            break;

          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            *q++ = '?';
            p++;
            state = 3;
            break;

          default:
            *q++ = *p++;
            break;
        }
        break;

      case 1: /* inside "..." */
        switch (*p) {
          case '\\':
            p++;
            p++;
            break;

          case '"':
            if ('"' == p[1]) {
              p++;
              p++;
            } else {
              p++;
              state = 0;
            }
            break;

          default:
            p++;
            break;
        }
        break;

      case 2: /* inside '...' */
        switch (*p) {
          case '\\':
            p++;
            p++;
            break;

          case '\'':
            if ('\'' == p[1]) {
              p++;
              p++;
            } else {
              p++;
              state = 0;
            }
            break;

          default:
            p++;
            break;
        }
        break;

      case 3: /* inside \d+ */
        switch (*p) {
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            p++;
            break;

          default:
            state = 0;
            break;
        }
        break;
    }
  }

done:
  *q = 0;
  return obf;
}

char* nr_sql_normalize(const char* obfuscated_sql) {
  char* normalized;
  int state;
  const char* p;
  char* q;

  if (0 == obfuscated_sql) {
    return 0;
  }
  if (0 == obfuscated_sql[0]) {
    return 0;
  }

  normalized = (char*)nr_malloc(nr_strlen(obfuscated_sql) + 1);
  p = obfuscated_sql;
  q = normalized;
  state = 0;

  while (*p) {
    switch (state) {
      case 0: /* Initial / normal state */
        if (('i' == *p) || ('I' == *p)) {
          state = 1;
        }
        *q = *p;
        q++;
        p++;
        break;

      case 1: /* Seen 'I' or 'i' */
        if (('n' == *p) || ('N' == *p)) {
          state = 2;
        } else {
          state = 0;
        }
        *q = *p;
        q++;
        p++;
        break;

      case 2: /* Seen '[Ii][Nn]' */
        if ('(' == *p) {
          state = 3;
        } else if (nr_isspace(*p)) {
          /* EMPTY */;
        } else {
          state = 0;
        }
        *q = *p;
        q++;
        p++;
        break;

      case 3: /* Seen '[Ii][Nn][::iswhite:](' */
        if (('?' == *p) || (',' == *p) || nr_isspace(*p)) {
          /* EMPTY */;
        } else if (')' == *p) {
          *q = '?';
          q++;
          *q = ')';
          q++;
          state = 0;
        } else {
          state = 4; /* Covers case where there is something other than ?,?,? in
                        the IN clause */
          *q = *p;
          q++;
        }
        p++;
        break;

      case 4: /* Seen non ?, in IN clause */
        if (')' == *p) {
          state = 0;
        }
        *q = *p;
        q++;
        p++;
        break;
    }
  }

  *q = 0;

  return normalized;
}

uint32_t nr_sql_normalized_id(const char* obfuscated_sql) {
  uint32_t ret;
  char* normalized;

  normalized = nr_sql_normalize(obfuscated_sql);
  if (0 == normalized) {
    return 0;
  }
  ret = nr_mkhash(normalized, 0);
  nr_free(normalized);

  return ret;
}

/*
 * This enumeration tells the SQL scanner/feature extractor
 * what the general structure of the SQL statement is after the SQL operator
 * keyword.
 */
typedef enum _nr_sql_parse_type_t {
  NR_SQL_PARSE_UNKNOWN = 0,
  NR_SQL_PARSE_UPDATE = 1, /* update statement */
  NR_SQL_PARSE_FROM = 2,   /* select and delete statements */
  NR_SQL_PARSE_INTO = 3    /* insert and replace statements */
} nr_sql_parse_type_t;

static const char* nr_sql_parse_type_string(
    nr_sql_parse_type_t from_into_none) {
  switch (from_into_none) {
    case NR_SQL_PARSE_UPDATE:
      return "update";
    case NR_SQL_PARSE_FROM:
      return "from";
    case NR_SQL_PARSE_INTO:
      return "into";
    case NR_SQL_PARSE_UNKNOWN:
      return "unknown";
    default:
      return "unknown";
  }
}

#define NR_SQL_WHITESPACE_CHARS " \r\n\t\v\f"
#define NR_SQL_DELIMITER_CHARS NR_SQL_WHITESPACE_CHARS "'\"`([@{"

/*
 * Purpose : Find the end of whitespace and comments prefixing an SQL string.
 *
 * Params  : 1. A NUL-terminated string which is a full SQL statement or
 *              an SQL fragment.
 *
 * Returns : Location at which comments and whitespace ends.  Returns NULL
 *           if a comment is not terminated.
 */
const char* nr_sql_whitespace_comment_prefix(const char* sql,
                                             int show_sql_parsing) {
  int x;
  const char* s = sql;

  if (0 == s) {
    return 0;
  }

  if (0 == s[0]) {
    return s;
  }

  x = nr_strspn(s, NR_SQL_WHITESPACE_CHARS);
  s += x;

  while (('/' == s[0]) && ('*' == s[1])) {
    s += 2;

    while (1) {
      if (0 == s[0]) {
        if (show_sql_parsing) {
          nrl_verbosedebug(NRL_SQL, "SQL parser: unterminated comment");
        }
        return NULL;
      }
      if (('*' == s[0]) && ('/' == s[1])) {
        s += 2;
        break;
      }
      s++;
    }

    x = nr_strspn(s, NR_SQL_WHITESPACE_CHARS);
    s += x;
  }

  return s;
}

static char* nr_sql_parse_over(const char* str,
                               int character,
                               int show_sql_parsing) {
  char* cend = nr_strchr(str, character);

  if (0 == cend) {
    if (show_sql_parsing) {
      nrl_verbosedebug(NRL_SQL, "SQL parser: unterminated %c", character);
    }
    return 0;
  }

  return cend + 1; /* Character is consumed */
}

void nr_sql_get_operation_and_table(const char* sql,
                                    const char** operation_ptr,
                                    char** table_ptr,
                                    int show_sql_parsing) {
  int i;
  const char* start = 0;
  const char* end = 0;
  const char* x;
  int sl;
  typedef struct _sql_parse_expectation {
    const char* opname;
    int oplength;
    nr_sql_parse_type_t opflag;
  } sql_parse_expectation_t;
  static sql_parse_expectation_t operations[] = {
      /*
       * In the future, we could match additional sql statements, e.g., create,
       * alter, drop, etc. We could also expand on the show to parse the table
       * name from "show columns in", etc.
       */
      {"select", sizeof("select") - 1, NR_SQL_PARSE_FROM},
      {"update", sizeof("update") - 1, NR_SQL_PARSE_UPDATE},
      {"insert", sizeof("insert") - 1, NR_SQL_PARSE_INTO},
      {"replace", sizeof("replace") - 1, NR_SQL_PARSE_INTO},
      {"delete", sizeof("delete") - 1, NR_SQL_PARSE_FROM},
      {NULL, 0, NR_SQL_PARSE_UNKNOWN}};

  if (table_ptr) {
    *table_ptr = 0;
  }
  if (operation_ptr) {
    *operation_ptr = 0;
  }
  if (0 == table_ptr) {
    return;
  }
  if (0 == operation_ptr) {
    return;
  }

  sql = nr_sql_whitespace_comment_prefix(sql, show_sql_parsing);
  if (NULL == sql) {
    return;
  }

  for (i = 0; operations[i].opname; i++) {
    if (0 == nr_strnicmp(operations[i].opname, sql, operations[i].oplength)) {
      break;
    }
  }

  *operation_ptr = operations[i].opname;
  if (0 == operations[i].opname) {
    return;
  }

  if (show_sql_parsing) {
    nrl_verbosedebug(NRL_SQL, "SQL parser: mode='%.32s' sql='%.1024s'",
                     nr_sql_parse_type_string(operations[i].opflag), sql);
  }

  x = sql;

  if (NR_SQL_PARSE_UPDATE == operations[i].opflag) {
    /*
     * If this is an UPDATE statement then the table name should follow
     * directly after the 'UPDATE' (ignoring whitespace and comments).
     * Since 'UPDATE' is the first word, we merely advance to the second
     * word.
     */
    sl = nr_strcspn(x, NR_SQL_WHITESPACE_CHARS);
    x += sl;
  } else {
    while (*x) {
      x = nr_sql_whitespace_comment_prefix(x, show_sql_parsing);
      if (NULL == x) {
        return;
      }

      if ('\'' == *x) {
        x = nr_sql_parse_over(x + 1, '\'', show_sql_parsing);
        if (NULL == x) {
          return;
        }
        continue;
      }

      if ('"' == *x) {
        x = nr_sql_parse_over(x + 1, '"', show_sql_parsing);
        if (NULL == x) {
          return;
        }
        continue;
      }

      if ((NR_SQL_PARSE_FROM == operations[i].opflag)
          && ('f' == nr_tolower(x[0])) && ('r' == nr_tolower(x[1]))
          && ('o' == nr_tolower(x[2])) && ('m' == nr_tolower(x[3]))
          && (NULL != nr_strchr(NR_SQL_DELIMITER_CHARS, x[4]))) {
        x += 4;
        break;
      }

      if ((NR_SQL_PARSE_INTO == operations[i].opflag)
          && ('i' == nr_tolower(x[0])) && ('n' == nr_tolower(x[1]))
          && ('t' == nr_tolower(x[2])) && ('o' == nr_tolower(x[3]))
          && (NULL != nr_strchr(NR_SQL_DELIMITER_CHARS, x[4]))) {
        x += 4;
        break;
      }

      sl = nr_strcspn(x, NR_SQL_WHITESPACE_CHARS "'\"");
      x += sl;
    }
  }

  /*
   * Skip comments and whitespace before table name.
   */
  x = nr_sql_whitespace_comment_prefix(x, show_sql_parsing);
  if (NULL == x) {
    return;
  }

  if ('(' == *x) {
    /*
     * There are two reasons there could be an open paren here: one is that
     * we have a subquery like SELECT * FROM (SELECT x FROM ...) and the
     * other is the way the Facebook API surrounds table names
     * like SELECT * FROM (`fb_users`)
     */
    x++;
    if (('`' == *x) || ('\'' == *x) || ('"' == *x)) {
      /* this is a table name surrounded by backquotes, continue below */
      /* EMPTY */
    } else {
      sl = nr_strcspn(x, NR_SQL_WHITESPACE_CHARS ",`)'\";");
      if ((x[sl] == ')') || (x[sl] == ',')) {
        /* this is a table name surrounded by parens, continue below */
        /* EMPTY */
      } else {
        const char* subquery = "(subquery)";
        /* this is a subquery */
        if (show_sql_parsing) {
          nrl_verbosedebug(NRL_SQL, "SQL parser: returning success: " NRP_FMT,
                           NRP_SQL(subquery));
        }
        *table_ptr = nr_strdup(subquery);
        return;
      }
    }
  }

  while (1) {
    if (('`' == *x) || ('\'' == *x) || ('"' == *x) || ('{' == *x)) {
      x++;
    }
    start = x;
    sl = nr_strcspn(x, NR_SQL_DELIMITER_CHARS "]});,*./");
    end = (x + sl);

    x += sl;
    sl = nr_strspn(x, NR_SQL_DELIMITER_CHARS "]});,*/");
    if ('.' == x[sl]) {
      /*
       * We've found the SQL `database`.`table` syntax, and all we have is the
       * database name so far, so go back up and get the table name.
       */
      x += (sl + 1);
      continue;
    }
    break;
  }

  if (start >= end) {
    if (show_sql_parsing) {
      nrl_verbosedebug(NRL_SQL, "SQL parser: returning failure: start >= end");
    }
    return;
  }

  *table_ptr = nr_strndup(start, (int)(end - start));

  if (show_sql_parsing) {
    nrl_verbosedebug(NRL_SQL, "SQL parser: returning success: " NRP_FMT, 100,
                     *table_ptr);
  }
}
