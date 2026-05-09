#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

CC="${CC:-cc}"
CFLAGS="${CFLAGS:--Wall -Wextra -Wpedantic -std=c11 -D_POSIX_C_SOURCE=200809L -g}"
CITY_MANAGER_TARGET="${CITY_MANAGER_TARGET:-city_manager}"
MONITOR_TARGET="${MONITOR_TARGET:-monitor_reports}"
CITY_MANAGER_SOURCES=(main.c report.c fs_utils.c)
MONITOR_SOURCES=(monitor_reports.c fs_utils.c)

echo "Building ${CITY_MANAGER_TARGET}..."
"${CC}" ${CFLAGS} -o "${CITY_MANAGER_TARGET}" "${CITY_MANAGER_SOURCES[@]}"
echo "Built ./${CITY_MANAGER_TARGET}"

echo "Building ${MONITOR_TARGET}..."
"${CC}" ${CFLAGS} -o "${MONITOR_TARGET}" "${MONITOR_SOURCES[@]}"
echo "Built ./${MONITOR_TARGET}"
