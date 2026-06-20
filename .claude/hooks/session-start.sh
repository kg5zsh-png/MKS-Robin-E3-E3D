#!/bin/bash
# SessionStart hook for MKS-Robin-E3-E3D (Marlin / PlatformIO firmware).
# Installs PlatformIO so any firmware variant can be built with `pio run`,
# and best-effort pre-warms the STM32 toolchain so the first build is fast.
# Web/remote sessions only; safe to run repeatedly (idempotent).
set -euo pipefail

# Only do heavy install work in the remote (Claude Code on the web) container.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

echo "[session-start] Setting up PlatformIO for Marlin firmware builds..."

# 1) PlatformIO core (idempotent: skip if already importable).
if ! python3 -c "import platformio" >/dev/null 2>&1; then
  echo "[session-start] Installing PlatformIO core..."
  python3 -m pip install --quiet --break-system-packages platformio \
    || python3 -m pip install --quiet platformio
else
  echo "[session-start] PlatformIO already installed."
fi

# Locate the pio entrypoint and make sure it's on PATH for the session.
PIO_BIN="$(command -v pio || command -v platformio || true)"
if [ -z "$PIO_BIN" ] && [ -x "$HOME/.local/bin/pio" ]; then
  PIO_BIN="$HOME/.local/bin/pio"
  echo 'export PATH="$HOME/.local/bin:$PATH"' >> "${CLAUDE_ENV_FILE:-/dev/null}"
fi
echo "[session-start] pio: ${PIO_BIN:-NOT FOUND}"
python3 -m platformio --version 2>/dev/null || true

# 2) Best-effort pre-warm the STM32 platform/toolchain used by the boards
#    (ststm32@~6.1.0). Non-fatal: sessions still work for non-build tasks.
echo "[session-start] Pre-warming ststm32 toolchain (best-effort)..."
python3 -m platformio platform install "ststm32@~6.1.0" >/dev/null 2>&1 \
  && echo "[session-start] ststm32 platform ready." \
  || echo "[session-start] ststm32 pre-warm skipped (will resolve on first build)."

echo "[session-start] Done. Build a variant with:"
echo "    pio run -d \"firmware/V1.1/Marlin-2.0.6.1_for_ender3_TMC2209\""
