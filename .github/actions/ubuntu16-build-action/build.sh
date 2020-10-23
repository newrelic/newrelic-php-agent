#!/bin/bash
make -j $(nproc) all
make -j $(nproc) run_tests

