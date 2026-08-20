#!/usr/bin/env bash
#
# Host-side tests for the DGUS console logic.
#
# These cover the parts that do not depend on Marlin headers: the scrollback
# ring and the G-code noise filter. The Marlin glue (DGUSConsole.cpp and the
# ExtUI/gcode hooks) is only exercised by a real firmware build.
#
set -euo pipefail

cd "$(dirname "$0")"
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

status=0
for t in test_console_buffer test_console_filter; do
  echo "=== $t ==="
  g++ -std=c++11 -Wall -Wextra -Werror -o "$out/$t" "$t.cpp"
  "$out/$t" || status=1
  echo
done

exit $status
