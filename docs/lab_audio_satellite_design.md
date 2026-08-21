# JLS-1 — Lab Voice Satellite (audio module for Claude ⇄ Mike)

**Owner:** KG5ZSH mesh (CiC node) · **Status:** draft hardware design, no firmware yet · **Last updated:** 2026-08-04

> **Build-ready docs:** [`lab_audio_satellite_bom.md`](lab_audio_satellite_bom.md) (shopping
> list), [`images/jls1_schematic.svg`](images/jls1_schematic.svg) (schematic), and
> [`lab_audio_satellite_assembly.md`](lab_audio_satellite_assembly.md) (step-by-step build) turn
> this design into an actual bench build. This doc stays the source of truth for *why*.

> Committed here for the same reason as `JARVISBoot_cert_fix_runbook.md`: this branch is
> the working surface for the JARVIS project, not Robin E3 firmware. This doc specs a
> physical **audio satellite** — mic + speaker + button — that lets the Claude app hold a
> spoken, hands-free conversation with Mike while he's at the bench. It is a companion to
> the LAB server already referenced in the cert-fix runbook (`100.82.240.6`); it does not
> replace or modify that server.

## Goal

Mike's hands are usually full or dirty in the lab (soldering iron, filament, a screwdriver).
He needs to talk to Claude without touching a keyboard. This doc specs a small standalone
box — **JLS-1** — built from on-hand ESP32 voice-kit parts, that:

1. Captures Mike's speech on a push-to-talk button.
2. Streams it to the existing LAB server over the lab's local network.
3. Plays back Claude's spoken reply through an onboard speaker.
4. Shows listening/thinking/speaking state on an LED so Mike knows when it's safe to talk.

The heavy lifting (speech-to-text, the actual Claude conversation, text-to-speech) stays on
the LAB server, next to whatever JARVIS glue already exists there. JLS-1 is a "dumb" audio
front end — a satellite, not a second brain — which keeps the ESP32 firmware simple and
keeps Claude API keys off a device sitting on an open bench.

## Architecture

```
 ┌───────────────────────────┐                              ┌──────────────────────────────┐
 │  JLS-1 satellite           │   Wi-Fi, lab LAN             │  LAB server (100.82.240.6)   │
 │  ESP32-S3 devkit           │ ─────────────────────────▶   │                              │
 │  INMP441 mic  (I2S RX)     │  ws://<lab-lan-ip>:5081/voice │  STT → Claude API → TTS      │
 │  MAX98357A + speaker (TX)  │ ◀─────────────────────────   │  (existing JARVIS glue)      │
 │  push-to-talk + WS2812 LED │   binary PCM16 frames         └──────────────────────────────┘
 └───────────────────────────┘
        bench-mounted, next to Mike
```

JLS-1 joins the **lab's local Wi-Fi**, the same physical LAN the LAB server's NIC sits on —
it does **not** need to join the Tailscale mesh itself. It talks to the LAB server's local
LAN address, not its `100.82.240.6` tailnet address. That keeps an ESP32 (no realistic
Tailscale/WireGuard client) out of the mesh's trust boundary entirely, at the cost of only
working when JLS-1 and the LAB server share a physical network. If the LAB server ever
moves off the lab LAN, this needs a subnet router — that's future work, not this doc.

## Bill of materials (from on-hand ESP32 voice-kit parts)

| Qty | Part | Role | Notes |
|----:|------|------|-------|
| 1 | ESP32-S3 dev board (DevKitC-1 or similar) | MCU, Wi-Fi | Dual I2S peripherals — one for mic RX, one for amp TX, run concurrently. Plain ESP32 also works (see caveats below) but S3 leaves room for on-device wake word later. |
| 1 | INMP441 breakout | Digital I2S MEMS mic | Omnidirectional, 24-bit I2S out — no analog preamp/noise-floor fuss. |
| 1 | MAX98357A breakout | I2S mono class-D amp, 3.2 W into 4 Ω | Takes I2S directly, no I2C config needed. |
| 1 | Small speaker, 4–8 Ω, 3–5 W | Output transducer | Whatever 40–50 mm speaker is in the parts bin. |
| 1 | Momentary pushbutton | Push-to-talk | Lab is noisy (fans, steppers) — start with a button, not always-listening wake word. |
| 1 | WS2812 (single LED or small ring) | Status indicator | Same part family already used for the printer's case lighting per this repo's Neopixel-enabled firmware variants. |
| 1 | 5 V / 2 A USB or barrel supply | Power | See power notes — don't run the amp off the dev board's onboard USB-serial regulator alone. |
| — | Hookup wire, perfboard or protoboard, 100–220 µF cap | Assembly | Decoupling cap across the amp's power rail, close to the MAX98357A. |

## Wiring / pinout

GPIO numbers below assume an ESP32-S3-DevKitC-1 and avoid the USB pins (19/20) and
strapping pins (0/3/45/46). **Check your specific board's silkscreen before wiring** —
some S3 modules reserve GPIO26–37 for flash/PSRAM depending on N4/N8R2/N16R8 variant, and
that range shifts which "free" GPIOs are safe.

**Mic — INMP441 (I2S0, RX only)**

| INMP441 pin | ESP32-S3 pin | Note |
|---|---|---|
| VDD | 3V3 | |
| GND | GND | |
| L/R | GND | selects left channel → mono |
| WS | GPIO 16 | I2S0 word-select |
| SCK | GPIO 17 | I2S0 bit clock |
| SD | GPIO 15 | I2S0 data in |

**Amp — MAX98357A (I2S1, TX only)**

| MAX98357A pin | ESP32-S3 pin | Note |
|---|---|---|
| VIN | 5V (dedicated feed, see power notes) | |
| GND | GND (common with mic/MCU) | |
| LRC | GPIO 5 | I2S1 word-select |
| BCLK | GPIO 7 | I2S1 bit clock |
| DIN | GPIO 6 | I2S1 data out |
| SD | GPIO 4 | drive high to enable output, low to mute; float = enabled at 9 dB default gain |
| Speaker +/− | speaker terminals | |

**Push-to-talk button:** one leg to GPIO 18, other leg to GND, internal pull-up enabled in
firmware (no external resistor needed).

**Status LED (WS2812):** DIN to GPIO 8 through a ~330 Ω series resistor, VCC/GND to
3V3/GND. A single LED doesn't need its own bulk cap; a small ring does — add 100–1000 µF
across its power feed if you go that route.

## Power notes

- Feed the MAX98357A's `VIN` from the 5 V supply directly, not through the ESP32 dev
  board's onboard 3.3 V/5 V regulator chain — that regulator is sized for the MCU, not a
  3 W amp's current spikes, and undervoltage on speaker peaks is what causes random resets.
- Common-ground everything (mic, amp, MCU, button, LED) even though power feeds are split.
- Put the 100–220 µF decoupling cap physically close to the MAX98357A's power pins.
- Route the speaker leads and I2S wiring away from the printer's stepper/bed-heater
  wiring on the bench — those are strong EMI sources and will show up as audible noise on
  a Class-D amp's PWM output if the cable runs are long or parallel.

## Firmware design

State machine, driven by the button:

```
IDLE ──(button down)──▶ LISTENING ──(button up)──▶ THINKING ──(reply audio ready)──▶ SPEAKING ──▶ IDLE
  │ LED: dim/off          │ LED: blue, streaming        │ LED: amber          │ LED: green         │
```

- **Audio in:** capture 16 kHz mono 16-bit PCM from the I2S0 mic while the button is held;
  stream it as binary WebSocket frames to the LAB server as it's captured (don't buffer
  the whole utterance in RAM — the S3 has plenty, but there's no reason to add latency).
- **Audio out:** play 16-bit PCM frames from the LAB server back out through I2S1/MAX98357A
  as they arrive, so playback can start before the whole reply has been synthesized.
- **Transport:** `ws://<lab-lan-ip>:5081/voice` — plaintext WebSocket, consistent with the
  `LABH`/plaintext-on-trusted-network convention already established in
  `JARVISBoot_cert_fix_runbook.md`. Same caveat applies here: this only belongs on the
  trusted lab LAN, never exposed off-mesh (see MESH-013 in that runbook).
- Debounce the button in firmware (a simple 30–50 ms software debounce is enough for a
  mechanical pushbutton; no hardware RC needed).

## LAB-server contract (new work, not covered by this doc)

This doc only specs the physical satellite. The LAB server needs a small addition to
accept it — call it out explicitly so the two sides don't drift:

- New WS endpoint, e.g. `/voice`, on the existing `:5081` LABH listener.
- Accepts binary 16 kHz/16-bit/mono PCM frames while the client is in LISTENING.
- On end-of-utterance (button release, sent as a small control frame), runs STT → feeds
  the transcript plus running conversation context into the Claude API (reusing whatever
  glue already exists there) → TTS's the reply → streams 16-bit PCM back over the same
  connection.
- A minimal 1-byte frame-type tag (`0x01` = mic audio, `0x02` = TTS audio, `0x03` =
  end-of-utterance) is enough to keep both sides in sync without a heavier protocol.
- Auth: a shared pre-shared token in a query param or header is consistent with the
  plaintext-on-trusted-mesh model already accepted for LABH — don't design in anything
  stronger than what the rest of the LAB stack already uses, but also don't leave it
  wide open on the LAN.

## Enclosure

A small 3D-printed case is the obvious call here, given what this repo's board actually
drives — print it on the same machine JLS-1 is meant to sit next to. Keep it simple:
a mic port hole facing up/out (avoid printing directly over the INMP441's port), a speaker
grille, a top-mounted button, and a light pipe or diffused window for the WS2812.

## Bring-up / test plan

1. Power-on smoke test: MCU boots, LED lights, no brownouts under speaker load.
2. Mic-only test: log raw I2S samples over serial (or to an SD card) and confirm a clean
   waveform when tapping/speaking near the mic.
3. Amp-only test: play a local 440 Hz test tone from flash, confirm clean audio with no
   dropouts when the LED is also driven (checks for I2S/GPIO contention).
4. Button test: confirm clean, debounced LISTENING/THINKING/SPEAKING transitions.
5. Network test: join lab Wi-Fi, ping the LAB server's LAN address.
6. End-to-end test: stand up a trivial echo endpoint on `/voice` before the real
   STT→Claude→TTS pipeline exists, so the round trip (mic → network → speaker) can be
   validated independently of the LAB-server work above.

## Future improvements (not in scope for v1)

- On-device wake word (Espressif ESP-SR keyword spotting) to drop the physical button,
  once the always-on-mic-in-a-noisy-lab false-trigger rate is actually measured.
- Acoustic echo cancellation so Mike can barge in over Claude's reply instead of the mic
  picking up the speaker's own output.
- LiPo + charge circuit for a cordless puck instead of a tethered supply.
