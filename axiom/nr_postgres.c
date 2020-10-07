/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdlib.h>

#include "nr_postgres.h"
#include "nr_postgres_private.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

char* nr_postgres_default_host() {
  char* host = getenv("PGHOSTADDR");

  if (nr_strempty(host)) {
    host = getenv("PGHOST");
    if (nr_strempty(host)) {
      host = "localhost";
    }
  }

  return nr_strdup(host);
}

char* nr_postgres_default_port() {
  char* port_path_or_id = getenv("PGPORT");

  if (nr_strempty(port_path_or_id)) {
    /*
     * 5432 is the compiled-in default.
     * See:
     * https://github.com/postgres/postgres/blob/master/configure.in#L151-L164
     */
    port_path_or_id = "5432";
  }

  return nr_strdup(port_path_or_id);
}

char* nr_postgres_default_database_name() {
  char* database_name = getenv("PGDATABASE");

  if (nr_strempty(database_name)) {
    database_name = getenv("PGUSER");
  }

  return nr_strdup(database_name);
}

/*
 * The connection info parsing logic is directly from libpq's conninfo_parse():
 * https://github.com/postgres/postgres/blob/a0ae54df9b153256a9d0afe45732853cb5ccae09/src/interfaces/libpq/fe-connect.c#L4662-L4819
 *
 * Please see ../LICENSE.txt#L1-L5 for license information.
 */
void nr_postgres_parse_conn_info(const char* conn_info,
                                 char** host,
                                 char** port_path_or_id,
                                 char** database_name) {
  char* pname;
  char* pval;
  char* conn_info_copy = NULL;
  char* cp;
  char* cp2;

  if ((NULL != *host) || (NULL != *port_path_or_id)
      || (NULL != *database_name)) {
    return;
  }

  if (NULL == conn_info) {
    goto fill_defaults;
  }

  conn_info_copy = nr_strdup(conn_info);
  cp = conn_info_copy;

  while (*cp) {
    /* Skip blanks before the parameter name */
    if (nr_isspace((unsigned char)*cp)) {
      cp++;
      continue;
    }

    /* Get the parameter name */
    pname = cp;
    while (*cp) {
      if (*cp == '=') {
        break;
      }
      if (nr_isspace((unsigned char)*cp)) {
        *cp++ = '\0';
        while (*cp) {
          if (!nr_isspace((unsigned char)*cp)) {
            break;
          }
          cp++;
        }
        break;
      }
      cp++;
    }

    /* Check that there is a following '=' */
    if (*cp != '=') {
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "missing \"=\" after \"%s\" in connection info string\n",
                       pname);
      goto fill_defaults;
    }
    *cp++ = '\0';

    /* Skip blanks after the '=' */
    while (*cp) {
      if (!nr_isspace((unsigned char)*cp)) {
        break;
      }
      cp++;
    }

    /* Get the parameter value */
    pval = cp;

    if (*cp != '\'') {
      cp2 = pval;
      while (*cp) {
        if (nr_isspace((unsigned char)*cp)) {
          *cp++ = '\0';
          break;
        }
        if (*cp == '\\') {
          cp++;
          if (*cp != '\0') {
            *cp2++ = *cp++;
          }
        } else {
          *cp2++ = *cp++;
        }
      }
      *cp2 = '\0';
    } else {
      cp2 = pval;
      cp++;
      for (;;) {
        if (*cp == '\0') {
          nrl_verbosedebug(
              NRL_INSTRUMENT,
              "unterminated quoted string in connection info string\n");
          goto fill_defaults;
        }
        if (*cp == '\\') {
          cp++;
          if (*cp != '\0') {
            *cp2++ = *cp++;
          }
          continue;
        }
        if (*cp == '\'') {
          *cp2 = '\0';
          cp++;
          break;
        }
        *cp2++ = *cp++;
      }
    }

    if ((0 == nr_strcmp(pname, "host")) && (NULL == *host)) {
      /*
       * It's possible to set both the host and hostaddr, but the hostaddr is
       * more specific so we prefer it.
       * See:
       * https://www.postgresql.org/docs/9.3/static/libpq-connect.html#LIBPQ-CONNECT-HOSTADDR
       */
      *host = pval;
    } else if (0 == nr_strcmp(pname, "hostaddr")) {
      *host = pval;
    } else if (0 == nr_strcmp(pname, "port")) {
      *port_path_or_id = pval;
    } else if (0 == nr_strcmp(pname, "dbname")) {
      *database_name = pval;
    } else if ((0 == nr_strcmp(pname, "user")) && (NULL == *database_name)) {
      /*
       * If dbname isn't explicitly provided, the user value is used.
       * See:
       * https://github.com/postgres/postgres/blob/a0ae54df9b153256a9d0afe45732853cb5ccae09/src/interfaces/libpq/fe-connect.c#L943-L950
       */
      *database_name = pval;
    }
  }

fill_defaults:
  /*
   * Now that we've successfully parsed the info string, we'll check for
   * localhost, sockets-as-hosts, and use default values for any empty fields.
   */
  if (nr_strempty(*host)) {
    *host = nr_postgres_default_host();
    if ((0 == nr_stricmp(*host, "localhost")) && (NULL == *port_path_or_id)) {
      /*
       * Without either a host name or host address, libpq will connect using
       * a local Unix-domain socket. /tmp is the compiled-in default.
       * See:
       * https://github.com/postgres/postgres/blob/a0ae54df9b153256a9d0afe45732853cb5ccae09/src/interfaces/libpq/fe-connect.c#L943-L950
       * See:
       * https://github.com/postgres/postgres/blob/a0ae54df9b153256a9d0afe45732853cb5ccae09/src/include/pg_config_manual.h#L185
       */
      *port_path_or_id = "/tmp";
    }
  } else if (*host[0] == '/') {
    /*
     * We're going to ignore port if given a path for the host name.
     * See:
     * https://github.com/postgres/postgres/blob/a0ae54df9b153256a9d0afe45732853cb5ccae09/src/interfaces/libpq/fe-connect.c#L841-L842
     */
    *port_path_or_id = *host;
    *host = nr_strdup("localhost");
  } else {
    *host = nr_strdup(*host);
  }

  if (nr_strempty(*port_path_or_id)) {
    *port_path_or_id = nr_postgres_default_port();
  } else {
    *port_path_or_id = nr_strdup(*port_path_or_id);
  }

  if (nr_strempty(*database_name)) {
    *database_name = nr_postgres_default_database_name();
  } else {
    *database_name = nr_strdup(*database_name);
  }

  nr_free(conn_info_copy);
}
