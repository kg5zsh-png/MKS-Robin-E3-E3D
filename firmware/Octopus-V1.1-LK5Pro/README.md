# Marlin for BTT Octopus V1.1 driving the LK5 Pro DWIN panel

Marlin (bugfix-2.1.x) configured for a **BigTreeTech Octopus V1.1 (STM32F446ZE)**
driving the **stock Longer LK5 Pro DWIN T5UID1 touchscreen**, plus a new
**DGUS console** that mirrors debug output, printer state changes and executed
G-code onto the panel.

---

## Build status — read this first

**This firmware has not been compiled.** It was developed in an environment
where the PlatformIO package registry is blocked by network policy
(`api.registry.platformio.org` → HTTP 403 on CONNECT) and no ARM toolchain was
available, so `pio run -e STM32F446ZE_btt` could not resolve `ststm32` and never
reached the compiler.

What *has* been verified:

| Component | Status |
|---|---|
| `DGUSConsoleBuffer` scrollback ring | **Tested** — host-compiled, 8 cases pass under `-Werror` |
| `dgusConsoleIsNoisyGcode()` filter | **Tested** — host-compiled, 10 case groups pass under `-Werror` |
| `DGUSConsole.cpp` Marlin glue | **Not compiled** — APIs grep-verified against the tree only |
| ExtUI / `gcode.cpp` hooks | **Not compiled** |
| Board + DGUS configuration | **Not compiled** |
| Anything on real hardware | **Not run** |

### Getting a buildable tree

```bash
./apply.sh              # clones Marlin at the pinned commit, applies the patch, runs tests
```

The patch is generated against Marlin `bugfix-2.1.x` commit
`0ebac470a47d9e278096c955f36087b613001a65` (2026-08-17) and has been verified to
apply cleanly to it. `marlin-overlay/` holds the new files verbatim for review
without applying anything.

Run the host tests with:

```bash
buildroot/share/tests/dgus_console/run_tests.sh
```

Before flashing, build it yourself on a machine with working PlatformIO access:

```bash
pio run -e STM32F446ZE_btt
```

Expect to fix compile errors on the first pass. Treat this as a reviewed
starting point, not a validated binary.

---

## Wiring: Octopus V1.1 ↔ LK5 Pro panel

The LK5 Pro panel is a DWIN T5UID1 smart display. It is **not** a parallel or
SPI LCD — it speaks the DGUS protocol over a plain 3.3 V/5 V TTL UART at
115200 baud, 8N1. Only four conductors matter: `5V`, `GND`, `TX`, `RX`.

Connect to the Octopus **TFT header** (USART1). Marlin's own
`pins_BTT_OCTOPUS_V1_1.h` sets `BOARD_LCD_SERIAL_PORT 1`, which is why the
config below uses `LCD_SERIAL_PORT 1`.

| Octopus TFT header | LK5 Pro panel |
|---|---|
| `+5V` | `5V` |
| `GND` | `GND` |
| `TX1` | `RX`  ← **crossed** |
| `RX1` | `TX`  ← **crossed** |

TX and RX must cross. Getting those two backwards is harmless — the link just
stays silent — but **reversing `5V` and `GND` will destroy the panel.**

### Confirm the panel pinout before plugging anything in

The LK5 Pro's screen cable is a 6-pin JST-XH, but only four pins carry signal;
the remaining two are duplicated rails or not connected, and **their position
varies between LK5 Pro screen board revisions** (panels marked `TL5`, `DWJT` and
`DWJTB` are known to differ). No official pinout document was available when
this was written, so do not wire from a generic diagram:

1. Read the silkscreen on the **panel's own controller PCB** next to the
   connector — DWIN boards almost always print `5V / GND / TXD / RXD` at the pins.
2. If it is unlabeled, power the printer down and use a multimeter in continuity
   mode: the `GND` pin reads continuity to the board's ground plane and to
   chassis screws; the `5V` pin does not.
3. Only after `5V` and `GND` are positively identified should you worry about
   which of the remaining two is TX and which is RX.

---

## Configuration applied

In `Marlin/Configuration.h`:

```c
#define MOTHERBOARD BOARD_BTT_OCTOPUS_V1_1
#define DGUS_LCD_UI RELOADED
#define LCD_SERIAL_PORT 1        // Octopus V1.1 "TFT" header = USART1
```

`DGUS_LCD_UI RELOADED` selects Marlin's DGUS-Reloaded driver, which is the
protocol variant the LK4/LK5 Pro community screen firmware speaks. Marlin's
`SanityCheck.h` imposes hard prerequisites on that UI, so the following are also
enabled to satisfy them:

| Setting | Why |
|---|---|
| `BLTOUCH` | RELOADED requires `HAS_BED_PROBE` |
| `AUTO_BED_LEVELING_BILINEAR` | RELOADED requires `HAS_MESH` |
| `LCD_BED_TRAMMING` | required by RELOADED |
| `BABYSTEPPING` + `BABYSTEP_ALWAYS_AVAILABLE` + `BABYSTEP_ZPROBE_OFFSET` | required by RELOADED |

Bed geometry is set to the LK5 Pro's 300 × 300 × 400 mm.

> If you are not running a BLTouch/CR-Touch, you must still provide *some* probe
> for the RELOADED UI to compile. Adjust to match your actual hardware.

### Screen firmware must match

The firmware expects the panel to be running a **DGUS-Reloaded** screen project,
whose VP map is defined in
`Marlin/src/lcd/extui/dgus_reloaded/config/DGUS_Addr.h`. A stock Longer screen
image will *not* respond correctly — the variable-pointer addresses differ. Flash
the panel with a DGUS-Reloaded-compatible LK5 Pro screen project first
(SD card ≤ 32 GB, FAT32, 4096-byte allocation units).

---

## The DGUS console

New code in `Marlin/src/lcd/extui/dgus_reloaded/console/`:

| File | Role |
|---|---|
| `DGUSConsoleBuffer.h` | Scrollback ring. No Marlin dependencies, so it is host-testable. |
| `DGUSConsoleFilter.h` | G-code noise filter. No Marlin dependencies, so it is host-testable. |
| `DGUSConsole.h` / `.cpp` | Marlin glue: formatting, rate-limited repaint, panel writes. |

It writes to the panel's four message lines — `MESSAGE_Line1`..`MESSAGE_Line4`
(`0x1100`–`0x117F`, 32 characters each) — and mirrors state changes onto
`MESSAGE_Status` (`0x3000`).

### What gets shown

- **Printer state** — homing start/done, print done, print timer transitions,
  and anything routed through `onStatusChanged()` (which is where Marlin's
  `M117` and internal status text ends up).
- **Debug / fatal output** — `kill()` reasons are pushed and painted immediately,
  bypassing the rate limiter, since no further idle cycle will run.
- **Executed G-code** — hooked in `GcodeSuite::process_next_command()`, so it
  reflects what is actually dispatched, not what was merely queued.

### Why it is rate-limited

`DGUSDisplay::writeString()` writes **synchronously** to `LCD_SERIAL`. A full
4-row repaint is 4 × (6-byte header + 32 bytes) = 152 bytes; against
`DGUS_TX_BUFFER_SIZE 48` at 115200 baud that is roughly 13 ms of blocking
writes. A print dispatches hundreds of commands per second, so repainting per
command would saturate the UART and stall command processing.

The console therefore never writes from the event path. Events land in the ring;
`loop()` repaints at most once per `DGUS_CONSOLE_FLUSH_MS` (default 120 ms) and
sends only rows whose text changed. Note that a scroll changes every row, so a
scrolling flush is the full ~13 ms — about 11% duty at the default interval.
Raise `DGUS_CONSOLE_FLUSH_MS` if you see print stutter.

By default `G0`/`G1`/`G2`/`G3`, `M105`, `M114` and `M155` are filtered out —
during a print they would otherwise refill the 4-line window several times per
second and hide everything worth reading. Set `DGUS_CONSOLE_GCODE_ALL` for a
full trace (useful for debugging, not for printing).

### Options

In `Marlin/Configuration_adv.h`, under `HAS_DGUS_LCD`:

```c
#if DGUS_UI_IS(RELOADED)
  #define DGUS_CONSOLE
  #if ENABLED(DGUS_CONSOLE)
    #define DGUS_CONSOLE_ROWS         4   // Message rows mirrored (max 4)
    #define DGUS_CONSOLE_FLUSH_MS   120   // (ms) Minimum interval between repaints
    //#define DGUS_CONSOLE_GCODE_ALL      // Echo every command
  #endif
#endif
```

`DGUS_CONSOLE` is guarded in `SanityCheck.h`: it requires `DGUS_LCD_UI RELOADED`,
`DGUS_CONSOLE_ROWS` in 1–4, and `DGUS_CONSOLE_FLUSH_MS` ≥ 20.

### Files touched outside the new module

- `Marlin/src/lcd/extui/dgus_reloaded/dgus_reloaded_extui.cpp` — init, idle,
  status, homing, print-done and kill hooks
- `Marlin/src/gcode/gcode.cpp` — one guarded call in `process_next_command()`
- `Marlin/src/inc/SanityCheck.h` — option guards
- `Marlin/Configuration.h`, `Marlin/Configuration_adv.h`

All additions are wrapped in `ENABLED(DGUS_CONSOLE)` and compile out cleanly when
the option is off.

---

## License

Marlin is GPLv3. The added files carry the same header and are GPLv3.
