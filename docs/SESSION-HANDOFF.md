# Session handoff — 2026-08-20

State at the end of the session that built the Octopus/LK5 Pro firmware port and
designed the non-planar slicer fork. Read `CLAUDE.md` first for durable facts.

Branch: `claude/voice-input-mic-sldfb1` (name is vestigial — it predates this
work). PR: [#3](https://github.com/kg5zsh-png/MKS-Robin-E3-E3D/pull/3), **draft, open**.

---

## 1. Marlin port — code complete, unverified

`firmware/Octopus-V1.1-LK5Pro/`

Marlin `bugfix-2.1.x` @ `0ebac470a47d9e278096c955f36087b613001a65`, configured for
BOARD_BTT_OCTOPUS_V1_1 + `DGUS_LCD_UI RELOADED` + `LCD_SERIAL_PORT 1`, plus a new
**DGUS console** that mirrors debug output, printer state and executed G-code to
the panel's four message lines.

```
firmware/Octopus-V1.1-LK5Pro/
├── README.md               wiring, config, design notes, caveats
├── apply.sh                clone Marlin @ pinned commit, apply patch, run tests
├── lk5pro-octopus.patch    full changeset (13 files) — verified to apply cleanly
└── marlin-overlay/         new files verbatim, for review without applying
```

### Verification status — be precise about this

| Component | Status |
|---|---|
| `DGUSConsoleBuffer` scrollback ring | ✅ **Tested** — host-compiled, 8 cases, `-Werror` |
| `dgusConsoleIsNoisyGcode()` filter | ✅ **Tested** — host-compiled, 10 case groups, `-Werror` |
| Patch applies to pinned upstream | ✅ **Verified** |
| `DGUSConsole.cpp` Marlin glue | ❌ Not compiled — APIs grep-verified only |
| ExtUI / `gcode.cpp` hooks | ❌ Not compiled |
| Board + DGUS + dual-Z config | ❌ Not compiled |
| Real hardware | ❌ Never run |

**The firmware has never been built.** The PlatformIO registry is blocked in this
environment. Do not describe it as working.

### Design points worth not re-deriving

- `DGUSDisplay::writeString()` writes **synchronously** to `LCD_SERIAL`. A 4-row
  repaint is ~152 bytes against a 48-byte TX buffer ≈ **13 ms of blocking
  writes**. Repainting per G-code command would saturate the UART and stall
  command processing — hence the ring buffer + `DGUS_CONSOLE_FLUSH_MS` (120 ms)
  coalescing, and the default filtering of G0–G3 / M105 / M114 / M155.
- The kill path calls `console.flush(true)` to bypass the rate limiter, since no
  further idle cycle runs after `kill()`.
- `DGUS_LCD_UI RELOADED` has hard prerequisites in `SanityCheck.h` (probe, mesh,
  `LCD_BED_TRAMMING`, babystepping, `BUFSIZE>=4`, `HOME_AFTER_DEACTIVATE` off).
  BLTouch + bilinear leveling were enabled **to satisfy those**, not because the
  hardware was confirmed. Revisit against the real probe.

---

## 2. Non-planar slicer fork — designed, not started

`docs/non-planar/PORT-DESIGN.md` — read it before writing any fork code.

Summary: CurviSlicer is **not** a curved-layer slicer. It tet-meshes, solves a QP
for a per-vertex height field, deforms the mesh, slices it **flat** with IceSL,
then inverse-maps the G-code back. That maps onto OrcaSlicer as a pre-process at
`PrintObject.cpp:836` and a post-process around `GCode::do_export` — the slicing
core stays untouched.

Planned deviation from the original: apply the inverse deformation to
**in-memory toolpaths** rather than parsing emitted G-code text, since we own the
pipeline. Drops their `src/gcode.cpp` entirely.

**No fork repo exists yet** — repo creation is 403 in this environment.

---

## 3. Next actions

**Blocked on the user:**

1. **Create an empty repo** — suggested `kg5zsh-png/orca-nonplanar`, private, no
   README/gitignore. Then attach it and start the fork harness.
2. **Widen the environment network policy** so PlatformIO resolves. Until then no
   firmware can be compiled, and OrcaSlicer's dependency build will hit the same
   wall.
3. **Measure the hotend clearance angle** (nozzle tip → widest point of
   block/shroud). This is a hard input to the curved-layer optimizer and bounds
   the achievable curvature on a 3-axis machine.
4. **Verify the Z leadscrew lead.** Config leaves Z at 400 steps/mm, correct for
   a 4-start T8 at 2 mm pitch (8 mm lead) / 16 microsteps. "4-flight" fixes starts
   but not pitch. A wrong lead scales layer height by a constant factor.

**Ready to do once unblocked:**

5. Fork OrcaSlicer, build the no-op deform/un-deform harness, verify byte-identical
   output against upstream. Proves plumbing before math.
6. Port `TetMesh` + `find_containing_tet` onto Orca types.
7. Drive the un-deform stage with a hand-written analytic height field — **first
   point a real curved-layer part comes off the printer, reachable without the
   QP solver.**
8. Port the OSQP optimizer last.

---

## 4. Things that were checked and found wanting

Recorded so they are not retried:

- **No official LK5 Pro panel pinout exists** in any findable source — Longer
  docs, DWIN datasheets, community repos. Meter-verification is the only route.
- **`teamgloomy.github.io` is blocked** by the egress proxy.
- **The community LK4/LK5 Marlin forks** (Guizz27, mrv96, Ajtak, LONGER3D) ship
  no Octopus board target — they build for Longer's own mainboard. They are
  useful for screen firmware, not for board config.
- **`Marlin/src/lcd/extui/lib/dgus/`** is the *old* 2.0.x layout still present in
  this repo's vendor directories. Mainline has moved to
  `Marlin/src/lcd/extui/dgus/` and `dgus_reloaded/`. Do not port against the
  vendor copies.
- **Gurobi** gives CurviSlicer's paper-quality results but is commercial. Use
  OSQP and accept the documented quality difference.

## 5. PR watch

A self check-in is armed for PR #3 (state, CI, review comments). The repo has **no
CI configured** — 0 check runs — so nothing will ever go green there; do not wait
on it.
