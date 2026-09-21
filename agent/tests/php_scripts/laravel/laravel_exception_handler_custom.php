<?php

/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require __DIR__ . '/Illuminate/Foundation/Application.php';

class laravel_base_handler
{
    public function shouldReport($e)
    {
        return true;
    }
    public function report($e)
    {
        return;
    }
    public function render($request, $e)
    {
        return 'rendered';
    }
}

class exception_custom_handler extends laravel_base_handler
{
}

$app = new \Illuminate\Foundation\Application();

$exception_handler = new exception_custom_handler();

$app->bind('Illuminate\Contracts\Debug\ExceptionHandler', $exception_handler);

$app->boot();

$exception_handler->report(new \Exception('boom'));

echo "exception reported\n";
