<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should handle the </body> element not being in the first or last
output chunk.
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

function new_chunk() {
  echo str_repeat(str_repeat(' ', 1023)."\n", 40);
}

?>
<!DOCTYPE HTML>
<html>
  <?php new_chunk() ?>
  <head>
    <meta charset="UTF-8">
  </head>
  <body>
  </body>
  <?php new_chunk() ?>
</html>
