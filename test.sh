#!/bin/sh
set -e
cd "$(dirname "$0")"
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
