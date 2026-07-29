#!/usr/bin/env bash

# cmake -S . -B build
# cmake --build build
# ./build/server

set -euo pipefail
cd "$(dirname "$0")"

cmake -S . -B build
cmake --build build -j"$(nproc)"

ln -sf build/compile_commands.json compile_commands.json
exec ./build/server
