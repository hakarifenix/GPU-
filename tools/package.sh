#!/usr/bin/env bash
# NOVA GPU - one command that produces a downloadable release archive.
#
#   tools/package.sh              # Release build -> tests -> archive
#   tools/package.sh --keep       # keep a previous build directory
#   tools/package.sh --no-tests   # skip tests (not recommended for a release)
#
# Output: build-release/nova-gpu-<version>-<os>-<arch>.tar.gz  (+ .sha256)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${NOVA_BUILD_DIR:-$ROOT/build-release}"
RUN_TESTS=1
KEEP=0
for arg in "$@"; do
    case "$arg" in
        --no-tests) RUN_TESTS=0 ;;
        --keep)     KEEP=1 ;;
        -h|--help)
            sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *)
            echo "unknown option: $arg (try --help)" >&2
            exit 2 ;;
    esac
done

# Prefer cmake from PATH; fall back to a workspace-local install if present.
if command -v cmake >/dev/null 2>&1; then
    CMAKE="$(command -v cmake)"
elif [ -x "$ROOT/tools/venv-cmake/bin/cmake" ]; then
    CMAKE="$ROOT/tools/venv-cmake/bin/cmake"
else
    echo "error: cmake not found. Install CMake 3.16+ first (see docs/build.md)." >&2
    exit 1
fi
echo "using: $($CMAKE --version | head -1)"

# ctest ships beside cmake; a PATH that has cmake but not ctest is common in
# hand-made toolchains, so resolve it from cmake's directory first.
CTEST="$(dirname "$CMAKE")/ctest"
if [ ! -x "$CTEST" ]; then
    CTEST="$(command -v ctest || true)"
fi

JOBS="$(command -v nproc >/dev/null 2>&1 && nproc || sysctl -n hw.ncpu 2>/dev/null || echo 2)"
# CMake/G++ get memory-hungry in parallel on small machines; clamp it.
[ "$JOBS" -gt 4 ] && JOBS=4

# A release archive should come from a clean build. Reuse the tree only when
# asked (--keep), and never delete anything that does not look like a CMake one.
if [ "$KEEP" = 0 ] && [ -d "$BUILD_DIR" ]; then
    if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
        echo "removing previous build directory: $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    else
        echo "error: $BUILD_DIR exists but is not a CMake build tree; refusing to delete it" >&2
        exit 1
    fi
fi

echo
echo "== configure (Release) =="
"$CMAKE" -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo
echo "== build (-j$JOBS) =="
"$CMAKE" --build "$BUILD_DIR" -j "$JOBS"

if [ "$RUN_TESTS" = 1 ]; then
    echo
    echo "== tests =="
    "$BUILD_DIR/bin/nova_tests"
    if [ -n "$CTEST" ] && [ -x "$CTEST" ]; then
        ( cd "$BUILD_DIR" && "$CTEST" --output-on-failure )
    else
        echo "warning: ctest not found next to cmake; nova_tests already ran above" >&2
    fi
fi

echo
echo "== package =="
"$CMAKE" --build "$BUILD_DIR" --target package

ARCHIVE="$(ls -t "$BUILD_DIR"/nova-gpu-*.tar.gz "$BUILD_DIR"/nova-gpu-*.zip 2>/dev/null | head -1 || true)"
if [ -z "$ARCHIVE" ]; then
    echo "error: no archive produced in $BUILD_DIR" >&2
    exit 1
fi

echo
echo "ready to upload:"
ls -la "$ARCHIVE"
[ -f "$ARCHIVE.sha256" ] && { echo; cat "$ARCHIVE.sha256"; }
