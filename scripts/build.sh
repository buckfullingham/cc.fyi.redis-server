#!/usr/bin/env bash

BUILD_TYPE=${BUILD_TYPE:-Release}
ROOT_DIR="${ROOT_DIR:-"$(readlink -f "$(dirname "$0")"/..)"}"
BUILD_DIR="${BUILD_DIR:-cmake-build-"$(echo "${BUILD_TYPE}" | tr 'A-Z' 'a-z')"}"

cd "$ROOT_DIR" || exit 1

mkdir -p "$BUILD_DIR" || exit 1
cd "$BUILD_DIR" || exit 1

source /opt/rh/gcc-toolset-11/enable || exit 1

export CONAN_USER_HOME="${CONAN_USER_HOME:-$(pwd)}"

conan profile update settings.compiler.libcxx=libstdc++11 default

conan install -sbuild_type="${BUILD_TYPE}" -scompiler.libcxx=libstdc++11 --build missing "$ROOT_DIR" || exit 1

cmake "${ROOT_DIR}" || exit 1

cmake --build . --parallel || exit 1

ctest || exit 1
