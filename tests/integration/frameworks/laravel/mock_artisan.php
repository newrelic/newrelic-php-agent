<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This modifies interfaces from the Symfony project.
 *
 * (c) Fabien Potencier <fabien@symfony.com>
 * SPDX-License-Identifier: MIT
 */

/* Mock enough bits of the Artisan infrastructure for naming tests. */

namespace Symfony\Component\Console\Input {
  interface InputInterface {
    public function getFirstArgument();
  }

  class Input implements InputInterface {
    private mixed $argument;
    public function __construct($argument) {
      $this->argument = $argument;
    }

    public function getFirstArgument() {
      return $this->argument;
    }
  }
}

namespace Symfony\Component\Console\Output {
  interface OutputInterface {
  }

  class Output implements OutputInterface {
  }
}

namespace Symfony\Component\Console {
  use Symfony\Component\Console\Input\InputInterface;
  use Symfony\Component\Console\Output\OutputInterface;

  class Application {
    public function doRun(/*InputInterface*/ $input, /*OutputInterface*/ $output) {
    }
  }
}

namespace Illuminate\Console {
  use Symfony\Component\Console\Application as SymfonyApplication;

  class Application extends SymfonyApplication {
  }
}
