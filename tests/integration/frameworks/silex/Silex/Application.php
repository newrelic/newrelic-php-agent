<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* DESCRIPTION
This mocks enough of Silex for our instrumentation to fire. 
*/

namespace Symfony\Component\HttpFoundation {
  class ParameterBag {
    public $_route;

    public function get($name, $default = null, $deep = false) {
      return isset($this->_route) ? $this->_route : $default;
    }
  }

  class Request {
    public $attributes;
  }

  class Response {}
}

namespace Symfony\Component\HttpKernel {
  use Symfony\Component\HttpFoundation\Response;

  interface HttpKernelInterface {
    const MASTER_REQUEST = 1;
    const SUB_REQUEST = 2;

    public function handle($request, $type = self::MASTER_REQUEST, $catch = true);
  }

  class HttpKernel implements HttpKernelInterface {
    public function handle($request, $type = self::MASTER_REQUEST, $catch = true) {
      return $this->handleRaw($request, $type);
    }

    private function handleRaw($request, $type = self::MASTER_REQUEST) {
      return new Response;
    }
  }
}
