#!/bin/sh
#
# LINUS SHELL SCRIPT
# Learn more: https://codecrafters.io/program-interface

set -e # Exit early if any commands fail

# Copied from .codecrafters/compile.sh
(
  cd "$(dirname "$0")" # Ensure compile steps are run within the repository directory
  cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
  cmake --build ./build
)

# Copied from .codecrafters/run.sh
exec $(dirname $0)/build/main "$@"
