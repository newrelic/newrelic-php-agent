<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Minimal Laravel mocks reproducing a long-running worker (e.g. Octane on
 * FrankenPHP): the application is constructed and booted once, then the HTTP
 * kernel handles requests. Tests decide whether the boot happens inside or
 * outside a transaction.
 */

namespace Illuminate\Http {
    class Request {
    }
}

namespace Illuminate\Routing {
    class Route {
        private $name;

        public function __construct($name) {
            $this->name = $name;
        }

        public function getName() {
            return $this->name;
        }
    }

    class Router {
        private $current;

        public function __construct($routeName) {
            $this->current = new Route($routeName);
        }

        public function current() {
            return $this->current;
        }

        public function prepareResponse($request, $response) {
            return $response;
        }
    }
}

namespace Illuminate\Foundation {
    class Application implements \ArrayAccess {
        const VERSION = '11.0.0';

        private $bindings = array();
        private $booted = false;
        public $resolvedBeforeBoot = array();

        public function __construct() {
        }

        public function boot() {
            $this->booted = true;
        }

        public function isBooted() {
            return $this->booted;
        }

        public function bind($key, $value) {
            $this->bindings[$key] = $value;
        }

        public function offsetExists($key): bool {
            return isset($this->bindings[$key]);
        }

        public function offsetGet($key): mixed {
            if (!$this->booted) {
                $this->resolvedBeforeBoot[] = $key;
            }
            return $this->bindings[$key];
        }

        public function offsetSet($key, $value): void {
            $this->bindings[$key] = $value;
        }

        public function offsetUnset($key): void {
            unset($this->bindings[$key]);
        }
    }
}

namespace Illuminate\Foundation\Http {
    class Kernel {
        protected $app;
        protected $router;
        protected $middleware = array('App\Http\Middleware\MockMiddleware');

        public function __construct($app, $router) {
            $this->app = $app;
            $this->router = $router;
        }

        public function handle($request, $action) {
            /* A regular request boots the application from within handle(). */
            if (!$this->app->isBooted()) {
                $this->app->boot();
            }

            try {
                $middleware = new \App\Http\Middleware\MockMiddleware;
                $response = $middleware->handle($request, $action);
                return $this->router->prepareResponse($request, $response);
            } catch (\Throwable $e) {
                $handler = $this->app['Illuminate\Contracts\Debug\ExceptionHandler'];
                $handler->report($e);
                return $handler->render($request, $e);
            }
        }
    }
}

namespace App\Http\Middleware {
    class MockMiddleware {
        public function handle($request, $next) {
            return $next($request);
        }
    }
}

namespace App\Exceptions {
    class Handler {
        public function shouldReport($e) {
            return true;
        }

        public function report($e) {
        }

        public function render($request, $e) {
            return 'rendered';
        }
    }
}

namespace {
    /*
     * Build the application and its HTTP kernel. A worker also boots the
     * application once, at worker boot; a regular request leaves booting to
     * Kernel::handle().
     */
    function mock_kernel($routeName, $boot) {
        $app = new Illuminate\Foundation\Application;
        $router = new Illuminate\Routing\Router($routeName);
        $kernel = new Illuminate\Foundation\Http\Kernel($app, $router);
        $app->bind('Illuminate\Contracts\Http\Kernel', $kernel);
        $app->bind('Illuminate\Contracts\Debug\ExceptionHandler', new App\Exceptions\Handler);
        if ($boot) {
            $app->boot();
        }

        return array($app, $kernel);
    }

    function mock_worker_boot($routeName) {
        list(, $kernel) = mock_kernel($routeName, true);

        return $kernel;
    }
}
