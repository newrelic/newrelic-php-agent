<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require __DIR__ . '/Illuminate/Foundation/Application.php';

class laravel_base_handler {
  function shouldReport($e) { return true; }
  function report($e) { return; }
  function render($request, $e) { return 'rendered'; }
}

class app_custom_handler extends laravel_base_handler {}

$app = new \Illuminate\Foundation\Application();

// Simulate DI
$exception_handler = new app_custom_handler();
$app->bind('Illuminate\Contracts\Debug\ExceptionHandler', $exception_handler);

// Install agent's instrumentation of exception handler
$app->boot();

// Simulate exception
$exception_handler->report(new \Exception('boom'));
echo "Exception reported\n";
