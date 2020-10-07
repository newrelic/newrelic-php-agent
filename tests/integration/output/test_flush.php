<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should handle the output buffer being flushed a lot.
*/

/*
 * We need output_buffering to be a non-zero value, but the exact value doesn't
 * matter, as nr_php_install_output_buffer_handler() hard codes 40960 as the
 * internal buffer size. 4096 has been chosen simply because it matches most
 * default distro configurations.
 */

/*INI
output_buffering = 4096
*/

/*ENVIRONMENT
REQUEST_METHOD=GET
*/

/*
 * In English: look for the first window.NREUM (the start of the RUM header),
 * then look for the trailing </script>, then look for another window.NREUM
 * (the start of the RUM footer).
 *
 * The ((?s).*?) construction matches all characters, including newlines (hence
 * the (?s) flag), in ungreedy mode.
 */

/*EXPECT_REGEX
window\.NREUM((?s).*?)</script>((?s).*?)window\.NREUM
*/

?>
<!DOCTYPE HTML>
<?php ob_flush() ?>
<html>
  <?php ob_flush() ?>
  <head>
    <?php ob_flush() ?>
    <meta charset="UTF-8">
    <?php ob_flush() ?>
  </head>
  <?php ob_flush() ?>
  <body>
    <?php ob_flush() ?>
  </body>
  <?php ob_flush() ?>
</html>
