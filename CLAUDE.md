# Project memory — KG5ZSH printer project

Read this first. It carries the durable facts so a new session does not have to
re-derive them. Session-specific state lives in `docs/SESSION-HANDOFF.md`.

> This repo started as the vendor's MKS Robin E3/E3D drop (`firmware/V1.0`,
> `firmware/V1.1`, `hardware/`) and is **now also serving as the working repo for
> a different machine** — a modified Longer LK5 Pro on a BTT Octopus V1.1. New
> work lives in `firmware/Octopus-V1.1-LK5Pro/` and `docs/non-planar/`. Do not
> assume vendor directories describe the machine being built.

## The machine being built

| Item | Value |
|---|---|
| Printer | Longer LK5 Pro, 300 × 300 × 400 mm, being modified |
| Controller | **BTT Octopus V1.1 (STM32F446ZE)** — replaces the stock Longer board |
| Display | **Stock LK5 Pro DWIN T5UID1 panel** — DGUS over UART, 115200 8N1 |
| Z | **Dual-Z**: one motor per 4-start leadscrew, Z2 on **MOTOR 3** |
| Drivers | **TMC2209, UART** — every Octopus V1.1 slot has its own UART TX line |
| Firmware now | Marlin |
| Firmware later | **Klipper** (planned migration) |
| SBCs on hand | Raspberry Pi 3B v1.2, BTT Pi V1.2.1 |

### Hardware facts that keep mattering

- **Dual-Z is not a fourth axis.** Both motors drive the same gantry. The machine
  is **3-axis with a vertical nozzle**. This decision gates the entire non-planar
  effort — see `docs/non-planar/PORT-DESIGN.md`.
- The LK5 Pro panel is **not** a parallel/SPI LCD. It is a DWIN smart display
  running its own OS, speaking DGUS over a plain UART.
- Panel connector is a **6-pin JST-XH carrying only 4 signals**; the spare two
  vary between screen board revisions (`TL5` / `DWJT` / `DWJTB` differ). No
  official pinout was findable. **Meter-verify 5V/GND before plugging in** —
  swapping TX/RX is harmless, reversing power destroys the panel.
- `EPC_STM_104` steppers are **6-wire unipolar**, 16 mm short stack, ~13–17 Ncm.
  Wire them **bipolar** (ignore centre taps). Adequate for Z; marginal for X/Y;
  too weak for a direct extruder.
- Inventory lives on Google Drive as `MikesInventory.json`
  (`17No0PzP0fOA0SadTu5bOpm6JlYvJ_hl7`) with a summary in `inventory_context.md`
  (`1dDWpBFHEQGtBP_f1OAOjFeNX46aCzYMl`). **It goes stale** — it still lists
  A4988/DRV8825 and no Octopus. Confirm against the user, not the file.

## Environment constraints (verified, not guessed)

These are properties of the cloud session, and they shape what can be delivered:

| Capability | Status |
|---|---|
| Google Drive | ✅ Works — search/read/write via the Drive connector |
| GitHub push to attached repos | ✅ Works |
| **Compiling firmware** | ❌ **PlatformIO registry blocked** (`api.registry.platformio.org` → 403 on CONNECT), no ARM toolchain |
| **Creating repos / forks** | ❌ `403 Resource not accessible by integration` — the user must create empty repos |
| miniMac / `~/` / local filesystem | ❌ No route from the cloud container. Needs a locally-run Claude Code session. |

**Never claim firmware builds or was tested on hardware.** Nothing in
`firmware/Octopus-V1.1-LK5Pro/` has been compiled. Say so plainly in any summary.

## Working conventions

- **Verify against the tree, don't recall.** Marlin option names, VP addresses and
  pin macros were all confirmed by grep before use.
- **Never invent DGUS VP addresses.** They must match the screen firmware's own
  project file or the panel silently ignores writes. Use the map in
  `Marlin/src/lcd/extui/dgus_reloaded/config/DGUS_Addr.h`.
- **Keep logic that can be host-tested free of Marlin headers.** That is why
  `DGUSConsoleBuffer.h` and `DGUSConsoleFilter.h` have no Marlin includes — they
  compile and run under `g++` in `buildroot/share/tests/dgus_console/`. Continue
  this pattern; it is the only real verification available here.
- New firmware work is delivered as a **patch pinned to an upstream commit** plus
  an overlay of new files, not a vendored fork. See
  `firmware/Octopus-V1.1-LK5Pro/apply.sh`.
- Commits: no model identifiers in messages or code.

## Where things are

| Path | What |
|---|---|
| `firmware/Octopus-V1.1-LK5Pro/` | Marlin port: DGUS console, dual-Z, wiring docs |
| `docs/non-planar/PORT-DESIGN.md` | CurviSlicer → OrcaSlicer fork design |
| `docs/SESSION-HANDOFF.md` | Current state and next actions |
| `firmware/V1.0`, `firmware/V1.1`, `hardware/` | Original vendor MKS Robin E3/E3D content — untouched |

## Decisions already made (do not relitigate)

1. **3-axis, not multi-axis.** CurviSlicer, not S³-Slicer.
2. **Fork OrcaSlicer**, not PrusaSlicer or Cura. AGPL-3.0 both sides — compatible.
3. **Marlin G-code flavor first**, Klipper second.
4. **`DGUS_LCD_UI RELOADED`** — the user will reflash the panel to a
   DGUS-Reloaded screen project to match. A stock Longer screen image will not work.
5. **`Z_STEPPER_AUTO_ALIGN` (G34)** for gantry squaring, not dual endstops.
   `Z_MULTI_ENDSTOPS` is documented in place if an endstop is added per screw.
6. **TMC2209 UART** confirmed on hand by the user, overriding the stale inventory.
