<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
If Laravel generates a dynamic route for a CORS HTTP OPTIONS requests the agent should
automatically name the route in order to avoid MGIs.  This test uses mocked laravel
objects to ensure this happens correctly.
*/

/*INI
newrelic.framework = laravel
*/

/*EXPECT
ok - Route Name Set Correctly?
ok - Non-OPTIONS request ignored?
ok - Does not clobber existing route name?
*/
require_once __DIR__.'/mock_http_options.php';
require_once(dirname(__FILE__).'/../../../include/tap.php');

/*
 * Test that an OPTIONS http request handled by getRouteForMethods
 * is renamed by the agent.
 */
$collection = new \Illuminate\Routing\RouteCollection;
$route = new \Illuminate\Routing\Route;
$collection->setMockedRoute($route);
$request    = new \Illuminate\Http\Request;
$request->setMethod('OPTIONS');
$route = $collection->getRouteForMethods($request, array());
tap_equal('_CORS_OPTIONS', $route->getName(), 'Route Name Set Correctly?');

/* 
 * Test that a non-OPTIONS http request handled by getRouteForMethods
 * is NOT renamed by the agent. 
 */
$collection = new \Illuminate\Routing\RouteCollection;
$route = new \Illuminate\Routing\Route;
$collection->setMockedRoute($route);
$request    = new \Illuminate\Http\Request;
$request->setMethod('POST');
$route = $collection->getRouteForMethods($request, array());
tap_equal(NULL, $route->getName(), 'Non-OPTIONS request ignored?');

/*
 * Test that an OPTIONS http request handled by getRouteForMethods preserves
 *  a route name IF Laravel sets one (i.e. have we ensured our
 * _CORS_OPTIONS name doesn't stomp on an actual route name if a
 * future version of Laravel decides to start naming these routes).
 */
$routeName  = 'a-better-name-for-this-route';
$collection = new \Illuminate\Routing\RouteCollection;
$route = new \Illuminate\Routing\Route;
$route->name($routeName);
$collection->setMockedRoute($route);
$request    = new \Illuminate\Http\Request;
$request->setMethod('OPTIONS');
$route = $collection->getRouteForMethods($request, array());
tap_equal($routeName, $route->getName(), 'Does not clobber existing route name?');

/* 
 * Test graceful handling if something that's not a request/has a method named ->method
 * ends up passed to `getRouteForMethods`.
 * No tap assertions, this code just ensures nothing bad happens.
 */
$collection = new \Illuminate\Routing\RouteCollection;
$route = new \Illuminate\Routing\Route;
$collection->setMockedRoute($route);
$request    = new \stdClass;
$route = $collection->getRouteForMethods($request, array());

/*
 * Test graceful handling if  a route that somehow doesn't have ->getName and ->name methods.
 * No tap assertions, this code just ensures nothing bad happens when the agent.
 */
$collection = new \Illuminate\Routing\RouteCollection;
$route = new \stdClass;
$collection->setMockedRoute($route);
$request    = new \Illuminate\Http\Request;
$request->setMethod('OPTIONS');
$route = $collection->getRouteForMethods($request, array());
