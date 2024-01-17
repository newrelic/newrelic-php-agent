<?php

// Custom theme filter hook callback
function theme2_identity_filter($value, ...$args) {
    echo "executing theme2_identity_filter on value=[$value]\n";
    return $value;
}
