#
# Copyright 2023 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

# Canonical list of supported PHP versions (ascending order).
# This is the single source of truth — update this when adding
# or removing PHP version support.
PHP_VERSIONS := 7.2 7.3 7.4 8.0 8.1 8.2 8.2-zts 8.3 8.3-zts 8.4 8.4-zts 8.5 8.5-zts

# PHP versions supported on arm64 (8.0+)
PHP_VERSIONS_ARM64 := $(filter 8.%, $(PHP_VERSIONS))

# Shell-friendly version list with PHPS env var override.
PHP_VERSION_LIST=$${PHPS:-$(PHP_VERSIONS)}

# ARCH can come from several sources:
#   - config.mk (top-level Makefile): x64, x86, arm64, aarch64
#   - GHA workflow input:             amd64, arm64
#   - uname -m fallback (standalone): x86_64, aarch64, arm64
# The php-versions-json recipe only checks for ARM variants (arm64, aarch64);
# any other value returns the full version list.
ARCH ?= $(shell uname -m)

# Output a JSON array of supported PHP versions for the given ARCH.
# Usage: make php-versions-json ARCH=arm64
.PHONY: php-versions-json
php-versions-json:
	@versions="$(if $(filter arm64 aarch64,$(ARCH)),$(PHP_VERSIONS_ARM64),$(PHP_VERSIONS))"; \
	json=""; sep=""; \
	for v in $$versions; do json="$${json}$${sep}\"$$v\""; sep=","; done; \
	echo "[$${json}]"
