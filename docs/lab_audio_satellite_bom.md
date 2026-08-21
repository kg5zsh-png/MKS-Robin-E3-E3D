# JLS-1 — Bill of Materials

**Owner:** KG5ZSH mesh (CiC node) · **Status:** build-ready BOM · **Last updated:** 2026-08-21

> Companion to [`lab_audio_satellite_design.md`](lab_audio_satellite_design.md) (architecture/why)
> and [`lab_audio_satellite_assembly.md`](lab_audio_satellite_assembly.md) (build steps). This is
> the shopping/pull list — everything here matches the "on-hand ESP32 voice-kit parts" assumption
> from the design doc. Swap in whatever you actually have on the shelf; the specific breakout
> board brand doesn't matter as long as the pinout matches what's below.

## Electronics

| Ref | Qty | Part | Typical spec | Notes |
|---|---:|---|---|---|
| U1 | 1 | ESP32-S3 dev board | DevKitC-1, N8R8 or similar, dual I2S | Plain ESP32 (non-S3) also works — see caveat in the design doc's wiring section. |
| U2 | 1 | INMP441 breakout | I2S MEMS mic, 24-bit, omnidirectional | Common "INMP441 I2S microphone module" breakout. |
| U3 | 1 | MAX98357A breakout | I2S mono class-D amp, 3.2 W @ 4Ω | Common "MAX98357A I2S 3W amp" breakout. |
| LS1 | 1 | Speaker | 4–8 Ω, 3–5 W, 40–50 mm | Whatever's in the parts bin; a sealed/enclosed speaker sounds better in a printed case than a bare driver. |
| SW1 | 1 | Momentary pushbutton | N.O., panel-mount preferred | Push-to-talk. Any 2-pin or 4-pin tactile/panel button works — wire only two legs. |
| D1 | 1 | WS2812 addressable LED | single LED or small ring | Status indicator. Same Neopixel family this repo's printer firmware already supports. |
| R1 | 1 | Resistor, 330 Ω | 1/4 W | Series resistor on the LED data line. |
| C1 | 1 | Electrolytic capacitor, 100–220 µF | ≥10 V | Decoupling cap across the amp's VIN/GND, mounted close to U3. |
| PSU | 1 | 5 V / 2 A power supply | USB-C or barrel jack | Dedicated supply — do not share the ESP32 dev board's own USB port for amp power. |

## Consumables / assembly materials

| Qty | Item | Notes |
|---|---|---|
| ~2 m | Hookup wire, 22–26 AWG, a few colors | Match wire color to the schematic's net colors if you have the spool colors for it (red/black/blue + a few signal colors) — makes later debugging much easier. |
| 1 | Small perfboard or protoboard | Big enough to anchor U1–U3, R1, C1, and the power rail junctions. |
| — | Solder, flux, heat-shrink or electrical tape | Standard bench consumables. |
| 4 | M2.5 or M3 standoffs + screws | Mounting U1 (and the enclosure lid) — size to whatever your dev board's mounting holes are. |
| — | PLA or PETG filament | For the printed enclosure — printed on the Robin E3/E3D-driven machine this repo is for. |

## Tools

- Soldering iron + solder
- Wire strippers/cutters
- Multimeter (continuity + voltage) — required for the pre-power-up checks in the assembly doc
- USB-C cable (data-capable, not charge-only) for flashing the ESP32-S3
- 3D printer + slicer, if printing the enclosure
- Small Phillips screwdriver

## Explicitly not included here

- **Firmware.** No ESP32 sketch exists yet — the assembly doc gets you to a wired, bench-tested
  board; flashing the push-to-talk/I2S/WebSocket firmware is separate follow-on work.
- **LAB-server `/voice` endpoint.** The STT → Claude → TTS pipeline the design doc specs as the
  "LAB-server contract" doesn't exist yet either. Both are required before JLS-1 can actually
  hold a conversation — see the design doc's *Bring-up / test plan* for how hardware bring-up is
  meant to happen independently of that (echo-endpoint test) so you're not blocked on it.
