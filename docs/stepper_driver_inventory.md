# Stepper / Motor Driver Inventory — KG5ZSH Mesh

**Logged:** 2026-06-20 · **Source:** CiC bench disambiguation (physical inspection)
**Cross-ref:** MikesInventory · checkpoint item "Driver haul disambiguation"

> Identifications below are from physical markings/layout. StepStick modules
> (#1–#3) fit the MKS Robin **E3D** pluggable sockets; #4–#5 are standalone
> external drivers and do **not** plug into the E3/E3D.

## Catalog

| # | ID | Form factor | Identifying features (as inspected) | Confidence |
|---|----|-------------|-------------------------------------|------------|
| 1 | **A4988** | StepStick | Red PCB; small heatsink on chip; pot horizontally centered below the sink; `A4988` silk between chip and pot | High (silk) |
| 2 | **DRV8825** | StepStick | Purple PCB; small aluminum sink on top; `DRV8825` chip top-side; **pot in the EN corner** | High |
| 3 | **TMC2209** | StepStick | Purple PCB; chip marked/exposed on the **underside** (QFN pad); large **blue** heatsink on top-side copper pad (BTT/FYSETC style) | High |
| 4 | **L298N** | Module | Dual full H-bridge; unmistakable | High |
| 5 | **TB6600** | Standalone box | Black; **3.5 A**, **9–42 VDC**; DIP-switch microstep/current tables printed on the side; heatsink base | Med-High |

## Key specs & usage notes

### 1. A4988 (StepStick)
- Up to **1/16** microstep; ~1 A continuous (≈2 A peak w/ cooling); Vmot 8–35 V.
- **VREF:** `Imax = VREF ÷ (8 × Rsense)` → for 0.1 Ω boards, **VREF ≈ Imax × 0.8**.
- Loudest of the three; no UART.

### 2. DRV8825 (StepStick)
- Up to **1/32** microstep; ~1.5 A continuous (≈2.2 A peak); Vmot 8.2–45 V.
- **VREF:** `Imax = VREF × 2` → **VREF ≈ Imax ÷ 2** (⚠️ *different from A4988 — do not reuse the same pot setting when swapping*).
- StepStick footprint, Marlin-supported, but **not on MKS's E3D validated list** (A4988/2208/2209).

### 3. TMC2209 (StepStick)
- UART or standalone; ~1.4 A RMS / ~2 A peak; keep **Vmot ≤ ~28 V**.
- Quiet (StealthChop) + **sensorless homing (StallGuard)** — headline feature.
- Current set via UART (preferred) or VREF in standalone mode.

### 4. L298N (dual H-bridge module)
- Full/half step only — **no microstepping, no current chopping**, ~2–3 V drop.
- Use for DC motors or a coarse axis; poor choice for a printer axis.

### 5. TB6600 (standalone)
- 9–42 VDC, up to ~4 A (3.5 A rated); Step/Dir/Enable opto inputs; microstep + current via DIP table on the case.
- High-current / NEMA23 option; external to any StepStick board.

## Compatibility summary

- **Robin E3D pluggable sockets:** #1 A4988, #2 DRV8825 (fits, not MKS-validated), #3 TMC2209.
- **Robin E3 (non-D):** TMC2209 is **soldered on-board** — loose StepSticks are for the E3D / other builds.
- **Standalone (no socket):** #4 L298N, #5 TB6600.
