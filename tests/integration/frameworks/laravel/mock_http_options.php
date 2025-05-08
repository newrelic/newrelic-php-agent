<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Mock out enough of laravel so we can ensure the agent
 * will automatically name the route.
 */
namespace Illuminate\Http {
  class Request {
      private $method;
      public function method() {
          return $this->method;
      }

      public function setMethod($method) {
          $this->method = $method;
      }
  }
}

namespace Illuminate\Routing {
  class Route {
      private $action = array();
      public function getName()
      {
          return isset($this->action['as']) ? $this->action['as'] : null;
      }

      public function name($name)
      {
          $this->action['as'] = isset($this->action['as']) ? $this->action['as'].$name : $name;
          return $this;
      }
  }
}

namespace Illuminate\Routing {
  class RouteCollection {
      private $mockedRoute;
      public function getRouteForMethods($request, array $methods) {
          return $this->mockedRoute;
      }

      private function routeFactory() {
          $route = new \Illuminate\Routing\Route;
          if($this->routeNameForFactory) {
              $route->name($this->routeNameForFactory);
          }
          return $route;
      }

      public function setMockedRoute($route) {
          $this->mockedRoute = $route;
      }
  }
}

