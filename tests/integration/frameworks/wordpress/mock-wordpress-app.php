<?php

/* A simple mock of a WordPress app */

require_once __DIR__.'/wp-config.php';

// Register core action
add_action("wp_init", "wp_core_action_1");
add_action("wp_init", "wp_core_action_2");
add_action("wp_init", "wp_core_action_3");
add_action("wp_init", "wp_core_action_4");

/* Emulate loading custom plugin and theme */
require_once __DIR__.'/wp-content/plugins/mock-plugin1.php';
require_once __DIR__.'/wp-content/plugins/mock-plugin2.php';
require_once __DIR__.'/wp-content/themes/mock-theme1.php';
require_once __DIR__.'/wp-content/themes/mock-theme2.php';

// Register custom plugin action
add_action("wp_loaded", "plugin1_action_1");
add_action("wp_loaded", "plugin2_action_1");

// Emulate WordPress initialization:
do_action("wp_init");
do_action("wp_loaded");

// Register custom theme filter
add_filter("the_content", "wp_core_filter_1");

// Register custom theme filter
add_filter("template_include", "theme1_identity_filter");
add_filter("template_include", "theme2_identity_filter");

// Emulate WordPress loading a template to render a page:
$template = apply_filters("template_include", "./path/to/templates/page-template.php");

// Emulate WordPress rendering the content for the page:
$content = apply_filters("the_content", "Lore ipsum HTML");
