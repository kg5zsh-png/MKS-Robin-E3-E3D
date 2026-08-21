# JLS-1 — Assembly Instructions

**Owner:** KG5ZSH mesh (CiC node) · **Status:** hardware build guide · **Last updated:** 2026-08-21

> Companion to [`lab_audio_satellite_design.md`](lab_audio_satellite_design.md) (architecture) and
> [`lab_audio_satellite_bom.md`](lab_audio_satellite_bom.md) (parts list). Schematic:
> [`images/jls1_schematic.svg`](images/jls1_schematic.svg).

> **Scope check before you start soldering:** these steps get you a fully wired, bench-tested
> JLS-1 board that boots, records, and plays audio locally. They do **not** by themselves let you
> talk to Claude yet — the firmware (in [`../jls1-firmware/`](../jls1-firmware/)) is written and
> flashable, but the LAB server's `/voice` endpoint (STT → Claude → TTS) described in the design
> doc's *LAB-server contract* section is still not built. Step 9 below can use either a local
> test tone or `jls1-firmware/tools/echo_voice_server.py` (which exercises the real firmware +
> network path, not just the speaker) to validate hardware independently of that remaining
> software work.

## 1. Gather parts and verify pinout

Pull everything from [`lab_audio_satellite_bom.md`](lab_audio_satellite_bom.md). Before wiring
anything, find your ESP32-S3 board's actual pinout diagram (varies by module: N4/N8R2/N16R8, etc.)
and confirm GPIO 4–8, 15–18 are free general-purpose pins on your specific board — not reserved
for flash/PSRAM. Substitute pins 1:1 if any of these are unavailable on your board; just keep the
mic on one I2S bus and the amp on the other.

## 2. Print the enclosure (start this first — it runs unattended)

If you're printing a case, start the print now so it's finishing while you wire the electronics.
Keep the mic port clear of infill/supports directly over the INMP441's hole, leave a speaker
grille, a cutout for the button, and a light pipe or diffused window for the LED. Any Marlin
build in this repo's `firmware/` tree runs it — no special settings needed for a simple case.

## 3. Dry-fit before soldering

Lay U1 (ESP32-S3), U2 (INMP441), U3 (MAX98357A), the speaker, button, and LED on the perfboard (or
just on the bench) in roughly their final positions. Confirm wire lengths will actually reach
before you cut and strip anything — it's much easier to fix now than after solder joints are made.

## 4. Wire the microphone (U2 → U1)

Reference: schematic left side. All connections are point-to-point, no passives needed.

1. U2 `VDD` → U1 `3V3`
2. U2 `GND` → U1 `GND`
3. U2 `L/R` → U2 `GND` (a short local jumper on the mic breakout itself — selects the left/mono
   channel, doesn't route back to the MCU)
4. U2 `WS` → U1 `GPIO16`
5. U2 `SCK` → U1 `GPIO17`
6. U2 `SD` → U1 `GPIO15`

## 5. Wire the amplifier and speaker (U3 → U1, U3 → LS1)

Reference: schematic right side.

1. U3 `LRC` → U1 `GPIO5`
2. U3 `BCLK` → U1 `GPIO7`
3. U3 `DIN` → U1 `GPIO6`
4. U3 `SD` → U1 `GPIO4` (leave unconnected/floating instead if you'd rather it default to
   always-enabled — GPIO4 lets firmware mute it, which is worth having)
5. U3 `Speaker+` / `Speaker−` → LS1's two terminals (polarity doesn't matter for a single driver)
6. **Do not** wire U3 `VIN`/`GND` to U1 yet — those come from the power rails in step 7, not from
   the dev board. Wiring the amp's power from the ESP32 board's own regulator is the single most
   common mistake here and it will brown out under speaker load.

## 6. Wire the button and LED

1. SW1: one leg → U1 `GPIO18`, other leg → GND rail (step 7). No external pull-up resistor —
   firmware enables the ESP32's internal one.
2. R1 (330 Ω): in series between U1 `GPIO8` and D1's `DIN` pin.
3. D1 `VCC` → 3V3 rail, D1 `GND` → GND rail.

## 7. Build the power rails, then connect the power-hungry parts

1. Run three short bus wires (or three rows of a perfboard) for **+5V**, **3V3**, and **GND** —
   don't skip this even for a small board; it's what keeps the amp's power isolated from the
   MCU's onboard regulator per step 5.
2. PSU `+` → +5V rail. PSU `GND` → GND rail.
3. U1 `5V`/`VIN` pin → +5V rail (powers the board; its onboard regulator makes the 3.3 V the mic
   and LED use).
4. U1 `3V3` pin → 3V3 rail (this is the *source* for that rail, not a separate feed).
5. U1 `GND` → GND rail.
6. U3 `VIN` → +5V rail directly, with **C1 (100–220 µF)** across `VIN`/`GND` mounted physically
   close to U3 — this is the decoupling cap that stops speaker peaks from browning out the amp.
7. U3 `GND` → GND rail.
8. SW1 and D1's GND legs (from step 6) → GND rail. D1's VCC leg → 3V3 rail.

## 8. Pre-power-up checks (do this before plugging anything in)

With a multimeter in continuity mode, power still disconnected:

1. Confirm there is **no continuity** between the +5V rail and the GND rail, and none between +5V
   and 3V3 — a short here is the thing you want to catch before, not after, power-up.
2. Confirm continuity from U1's GPIO pins listed above out to their matching U2/U3/SW1/D1 pins —
   catches a cold joint or a wire on the wrong pin.
3. Double check L/R really lands on U2's own GND pin, not the rail (step 4.3) — wrong here just
   silently picks the other channel, not a hard fault, but worth confirming while you're at it.

## 9. First power-up smoke test

1. Plug in the 5 V supply. Confirm U1 boots (its own power LED, if it has one) and nothing gets
   hot to the touch in the first 10–15 seconds.
2. Measure the 3V3 and +5V rails with the multimeter — both should read within ~5% of nominal.
3. Flash the real firmware from [`../jls1-firmware/`](../jls1-firmware/) (see its README) and
   run `jls1-firmware/tools/echo_voice_server.py` on any machine on the lab LAN. Press and hold
   the button, say something, let go — you should hear it echoed back through LS1 a beat later.
   That single test proves the mic, I2S wiring, WS2812 state colors, amp, and speaker all
   together, using the real firmware and network path rather than a synthetic tone.

## 10. Close it up

Once bring-up checks pass, seat the board in the enclosure, route the mic so its port lines up
with the case's mic hole, mount the speaker against its grille, and secure the lid. Leave the USB-C
port on U1 reachable — you'll still want it for firmware updates.

## What's next

Hardware and firmware are both built and bench-verified (step 9's echo test proves the whole
mic → network → speaker path). The one remaining piece to actually talk to Claude is the LAB
server's `/voice` endpoint — see `lab_audio_satellite_design.md`'s *LAB-server contract* section
for the frame protocol it needs to speak (the same one `jls1-firmware` and the echo server
already use).
