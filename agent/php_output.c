/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_output.h"

/*
 * A few general notes on PHP's output system (specifically around how it
 * signals its current state) follow.
 *
 * The mode (or flags, depending on the PHP version) argument is a bitfield,
 * the exact meaning of which varies by PHP version. In PHP 5.3 and earlier, it
 * will be some combination of:
 *
 * PHP_OUTPUT_HANDLER_START: denotes the first chunk in an output buffer
 * PHP_OUTPUT_HANDLER_CONT:  denotes a continued chunk in an output buffer
 * PHP_OUTPUT_HANDLER_END:   denotes the final chunk in an output buffer
 *
 * Note that PHP_OUTPUT_HANDLER_START is 0.
 *
 * PHP's output system was completely rewritten in PHP 5.4 and
 * now provides a bitfield made up of these fields:
 *
 * PHP_OUTPUT_HANDLER_WRITE: denotes that there is data to be written
 * PHP_OUTPUT_HANDLER_START: as above, denotes the first chunk
 * PHP_OUTPUT_HANDLER_CLEAN: denotes that the buffer is being cleaned; any data
 *                           provided should be thrown away
 * PHP_OUTPUT_HANDLER_FLUSH: denotes that the buffer is being flushed
 * PHP_OUTPUT_HANDLER_FINAL: denotes the final chunk in an output buffer
 *
 * PHP_OUTPUT_HANDLER_CONT still exists as an alias for
 * PHP_OUTPUT_HANDLER_WRITE, and PHP_OUTPUT_HANDLER_END is an alias for
 * PHP_OUTPUT_HANDLER_FINAL.
 *
 * Note that PHP_OUTPUT_HANDLER_WRITE is now 0.
 */

int nr_php_output_has_content(int flags) {
  return !(flags & PHP_OUTPUT_HANDLER_CLEAN);
}

void nr_php_output_install_handler(const char* name,
                                   php_output_handler_func_t handler
                                       TSRMLS_DC) {
  if (NULL == name) {
    return;
  }
  if (0 == handler) {
    return;
  }

  /*
   * On PHP 5.4+, php_output_start_internal checks for duplicate handler and
   * doesn't install the handler if a handler with the same name already exists.
   *
   * On PHP 5.3, php_ob_set_internal_handler doesn't check for duplicate
   * handlers, so we check with php_ob_handler_used.
   */
  int flags = PHP_OUTPUT_HANDLER_STDFLAGS;
  size_t chunk_size = 40960;
  int name_len = nr_strlen(name);

  php_output_start_internal(name, name_len, handler, chunk_size,
                            flags TSRMLS_CC);
}

int nr_php_output_is_end(int flags) {
  return flags & PHP_OUTPUT_HANDLER_END;
}

int nr_php_output_is_start(int flags) {
  return flags & PHP_OUTPUT_HANDLER_START;
}
