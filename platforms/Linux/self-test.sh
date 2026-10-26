#!/bin/sh
set -eu
demo_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$demo_dir/runtime.sh"
resolve_hiface_runtime
exec "$hiface_runtime/bin/hiface_demo" self-test --builtin
