<?php

// Simple mock of wordpress's do_action, apply_filter, add_filter, add_action
// In a real Wordpress app, the $tag is not what is eventually
// called by the call_user_func_array. We do this for test simplicity
function do_action($tag, ...$args) {
    call_user_func_array($tag, $args);
}
function apply_filters($tag, ...$args) {
    call_user_func_array($tag, $args);
}

function add_filter($tag, $callback) {
    echo "add filter\n";
}

// WP's add_action wraps add_filter
function add_action($tag, $callback) {
    add_filter($tag, $callback);
}

//Simple mock of wordpress's get_theme_roots
function get_theme_roots() {
}

// Noop to prevent PHP 8.2 empty file optimization
sleep(0);
