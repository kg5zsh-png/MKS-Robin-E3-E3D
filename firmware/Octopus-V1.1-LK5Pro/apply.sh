#!/usr/bin/env bash
#
# Build a Marlin tree for the BTT Octopus V1.1 + LK5 Pro DWIN panel.
#
# Clones Marlin at the exact upstream commit this port was developed against,
# applies lk5pro-octopus.patch, and runs the host-side tests.
#
# Usage: ./apply.sh [target-dir]     (default: ./Marlin-LK5Pro-Octopus)
#
set -euo pipefail

# bugfix-2.1.x as of 2026-08-17. The patch is generated against this tree; a
# different commit may need the hunks re-fitting.
UPSTREAM_URL="https://github.com/MarlinFirmware/Marlin"
UPSTREAM_COMMIT="0ebac470a47d9e278096c955f36087b613001a65"

here="$(cd "$(dirname "$0")" && pwd)"
target="${1:-$PWD/Marlin-LK5Pro-Octopus}"

if [[ -e "$target" ]]; then
  echo "error: $target already exists; remove it or pass another path" >&2
  exit 1
fi

echo "==> Cloning Marlin into $target"
git clone "$UPSTREAM_URL" "$target"

echo "==> Checking out $UPSTREAM_COMMIT"
git -C "$target" checkout --quiet "$UPSTREAM_COMMIT"

echo "==> Applying lk5pro-octopus.patch"
git -C "$target" apply --check "$here/lk5pro-octopus.patch"
git -C "$target" apply "$here/lk5pro-octopus.patch"

echo "==> Running host-side tests"
"$target/buildroot/share/tests/dgus_console/run_tests.sh"

cat <<EOF

Done. Tree ready at:
  $target

Next:
  cd "$target"
  pio run -e STM32F446ZE_btt

NOTE: this firmware has never been compiled -- the environment it was written in
could not reach the PlatformIO registry. Expect to fix errors on the first build.
Read README.md before wiring or flashing anything.
EOF
