#!/bin/bash
make -j $(nproc) all
make -j run_tests

