#!/bin/bash
set -euo pipefail
task_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
task_build="$task_root/simulator/build"

for task_tool in cmake ninja; do
    if ! command -v "$task_tool" >/dev/null 2>&1; then
        echo "Missing $task_tool. Install prerequisites: brew install cmake ninja sdl2" >&2
        exit 1
    fi
done
cmake -S "$task_root/simulator" -B "$task_build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBAMBU_SANITIZERS=OFF
cmake --build "$task_build" --parallel 8
if [[ "${1:-}" == "--build-only" ]]; then
    exit 0
fi
if [[ $# -gt 0 ]]; then
    exec "$task_build/bambu_studio.app/Contents/MacOS/bambu_studio" "$@"
fi
open -n "$task_build/bambu_studio.app"
