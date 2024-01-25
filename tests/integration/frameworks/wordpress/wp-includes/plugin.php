<?php

// Mock WordPress hooks
$wp_filters = [];

function add_filter($hook_name, $callback) {
  global $wp_filters;
  if (!array_key_exists($hook_name, $wp_filters)) {
    $wp_filters[$hook_name] = [];
  }
  array_push($wp_filters[$hook_name], $callback);
}

function add_action($hook_name, $callback) {
  add_filter($hook_name, $callback);
}

function apply_filters($hook_name, $value, ...$args) {
  global $wp_filters;
  foreach ($wp_filters[$hook_name] as $filter) {
    $value = call_user_func_array($filter, array($value, $args));
  }
  return $value;
}

function do_action($hook_name, ...$args) {
  global $wp_filters;
  foreach ($wp_filters[$hook_name] as $filter) {
    call_user_func_array($filter, array($args));
  }
  return ;
}
