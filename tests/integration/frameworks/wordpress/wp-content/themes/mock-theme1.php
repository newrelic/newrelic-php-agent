<?php

// Custom theme filter hook callback
function theme1_identity_filter($value, ...$args) {
    echo "executing theme1_identity_filter on value=[$value]\n";
    return $value;
}
