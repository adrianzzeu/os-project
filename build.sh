#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

CC="${CC:-cc}"
CFLAGS="${CFLAGS:--Wall -Wextra -Wpedantic -std=c11 -D_POSIX_C_SOURCE=200809L -g}"
TARGET="${TARGET:-city_manager}"
SOURCES=(main.c report.c fs_utils.c)

echo "Building ${TARGET}..."
"${CC}" ${CFLAGS} -o "${TARGET}" "${SOURCES[@]}"
echo "Built ./${TARGET}"
