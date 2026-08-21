# JLS-1 firmware

ESP32-S3 firmware for the Lab Voice Satellite. Pairs with
[`../docs/lab_audio_satellite_design.md`](../docs/lab_audio_satellite_design.md) (architecture
and wire protocol), [`../docs/images/jls1_schematic.svg`](../docs/images/jls1_schematic.svg)
(wiring), and [`../docs/lab_audio_satellite_assembly.md`](../docs/lab_audio_satellite_assembly.md)
(build steps) — build the hardware first if you haven't.

## What this is (and isn't)

`src/main.cpp` is the push-to-talk state machine: capture mic audio while the button's held,
stream it to a LAB server over WebSocket, play back whatever comes back through the speaker.
It does **not** contain any speech-to-text, Claude API calls, or text-to-speech — that's the
LAB server's job, per the design doc's "LAB-server contract" section, and it isn't written
yet. `tools/echo_voice_server.py` (below) stands in for it so you can prove the hardware and
firmware work correctly before that server-side piece exists.

## Setup

1. Install [PlatformIO](https://platformio.org/) (already set up in this repo's dev container
   for the Marlin builds — `pio` should already be on your PATH there).
2. Copy the config template and fill in real values:
   ```sh
   cd jls1-firmware
   cp include/config.h.example include/config.h
   ```
   Edit `include/config.h`: your lab WiFi SSID/password, the LAB server's **local LAN**
   address (not its `100.82.240.6` tailnet address — see the design doc's Architecture
   section for why), and an auth token. `config.h` is gitignored — it's never committed.
3. Build and flash:
   ```sh
   pio run -t upload
   pio device monitor
   ```

## Bring-up: prove the hardware before the LAB server exists

This is the design doc's bring-up plan, item 6, made runnable. On any machine on the same lab
LAN as JLS-1 (doesn't need to be the real LAB server):

```sh
pip install websockets
python3 tools/echo_voice_server.py --port 5081 --token <same token as config.h>
```

Point `LAB_SERVER_HOST` in `config.h` at that machine's LAN IP, flash, then press and hold the
button, say something, let go. A beat later you should hear it played back through the
speaker — that proves mic capture, the WS link, the frame protocol, and speaker playback are
all working end to end. If that works, the only thing left before JLS-1 can actually talk to
Claude is the real STT → Claude API → TTS logic replacing this echo on the LAB server side.

`echo_voice_server.py` was smoke-tested against `websockets` 17.0.1 (its current asyncio
server API) with a script that mimics the firmware's exact frame protocol — byte-for-byte
round trip and auth rejection both verified. If your installed `websockets` version predates
v13's asyncio server (the old `websockets.legacy` implementation), see the note at the top of
that file for the one-line fix.

## Known rough edges (v1, honestly)

- **`MIC_SHIFT`** (in `main.cpp`) converts the INMP441's raw 32-bit I2S word to a 16-bit PCM
  sample. The right shift amount is gain/board-dependent — if audio comes through the echo
  test too quiet or clipped, adjust it on the bench.
- **No barge-in.** The button only starts a turn from `IDLE`; you can't interrupt Claude mid-
  reply. Listed as a future improvement in the design doc, not solved here.
- **Single-loop, no FreeRTOS tasks.** Simple to read and modify, but if this grows real-time
  requirements later (e.g. on-device wake word), it'll need to move to task-based concurrency.
- **`platform = espressif32@6.9.0`** is pinned in `platformio.ini` for a known-good match with
  the legacy `driver/i2s.h` API this firmware uses. Bump it once you've actually verified a
  newer platform version still builds and runs correctly on your bench — don't just delete the
  pin.
