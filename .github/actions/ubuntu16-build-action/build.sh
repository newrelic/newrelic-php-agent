#!/bin/bash
make -j $(nproc) all
make run_tests

