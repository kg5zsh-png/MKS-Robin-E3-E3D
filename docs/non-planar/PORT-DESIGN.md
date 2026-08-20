# Non-Planar: OrcaSlicer fork port design

Target: curved-layer ("slightly curved") slicing on a **3-axis** printer — the
modified LK5 Pro on a BTT Octopus V1.1 with dual-Z.

Status: **design only.** No fork code written yet. This document records what was
established by reading both codebases, so the implementation does not have to
re-derive it.

---

## Scope decision: 3-axis, not multi-axis

Dual-Z drives two leadscrews on **the same gantry**. It buys gantry squaring
(G34), not a degree of freedom — the machine remains 3-axis with a vertical
nozzle. That rules one family of prior art in and one out:

| Project | Fit | Why |
|---|---|---|
| **[CurviSlicer](https://github.com/mfx-inria/curvislicer)** (INRIA) | ✅ **Use this** | Explicitly "curved printing on standard, off-the-shelf, 3-axis FDM printers" |
| [S³-Slicer](https://github.com/zhangty019/S3_DeformFDM) | ❌ Reject | General *multi-axis* framework; emits toolpaths a 3-axis machine cannot execute |
| [Bricklayers](https://github.com/TengerTechnologies/Bricklayers) | ❌ Not a fork | Post-processing script, not curved layers |

Base slicer: **[OrcaSlicer](https://github.com/OrcaSlicer/OrcaSlicer)** — actively
maintained, PrusaSlicer lineage, native Klipper output (matters for the eventual
Klipper migration on this printer).

**Licensing is clean:** CurviSlicer is AGPL-3.0, OrcaSlicer is AGPL-3.0. Code can
be carried across with attribution intact.

---

## How CurviSlicer actually works

Traced from `curvislice.sh` and `src/`. It is **not** a slicer that emits curved
layers directly. It is a deform → slice-flat → un-deform pipeline:

```
model.stl
   │
   ├─(1)─► TetWild ──────────────► model.msh          tetrahedral mesh
   │
   ├─(2)─► curvislice_osqp ──────► displacements       per-vertex height field h
   │        (QP solver)            tetmats             per-tet 3×3 Jacobians
   │
   ├─(3)─► deform mesh by h
   │
   ├─(4)─► IceSL ────────────────► model.gcode         FLAT slicing of deformed mesh
   │
   └─(5)─► uncurve ──────────────► curved model.gcode  inverse map through tet mesh
```

The optimizer (step 2) solves for a scalar height displacement per tet vertex
that flattens the part's top surfaces, subject to deposition-slope and
layer-thickness constraints. Step 5 (`src/uncurve.cpp`, 809 lines) walks the flat
G-code, locates each point's containing tetrahedron (`find_containing_tet`),
barycentrically interpolates `h` to get the curved Z, and uses the per-tet
Jacobian to correct extrusion for the local volume change — then `reflow()`
recomputes E values and clamps feedrate between `min_speed_mm_sec` (10) and
`max_speed_mm_sec` (30).

### Source layout worth reusing

| File | Lines | Role |
|---|---|---|
| `src/main.cpp` | 1105 | QP setup and solve |
| `src/uncurve.cpp` | 809 | inverse deformation of toolpaths + extrusion compensation |
| `src/TetMesh.cpp` | 638 | tet mesh container, point location |
| `src/MeshFormat_msh.cpp` | 434 | `.msh` I/O (droppable once meshing is in-process) |
| `src/gcode.cpp` | 255 | G-code parsing (**droppable** — see below) |

---

## Integration into OrcaSlicer

The pipeline maps onto OrcaSlicer as a **pre-process** and a **post-process**
around an *unmodified* slicing core. This is the central design decision: it keeps
the fork shallow and rebasing on upstream feasible.

### Injection point A — deform before slicing

`src/libslic3r/PrintObject.cpp:836`

```cpp
TriangleMesh mesh = this->m_model_object->raw_mesh();
```

This is where the object's mesh enters the slicing pipeline. Steps 1–3 (tet-mesh,
solve, deform) run here, and the deformed mesh is handed to the existing slicer.
**Everything downstream — perimeters, infill, supports, arachne — is untouched.**
The tet mesh and `h` field are cached on the `PrintObject` for step 5.

### Injection point B — un-deform the toolpaths

`src/libslic3r/GCode.cpp:2428` (`GCode::do_export`) / `_do_export` at 2888.

**Improve on CurviSlicer here.** CurviSlicer parses G-code *text* it did not
generate, because IceSL is an external binary it does not control. Inside
OrcaSlicer we own the pipeline, so the inverse deformation should be applied to
the **in-memory `ExtrusionPath` / toolpath structures** before the G-code writer
emits them. That:

- avoids a parse → transform → re-emit round trip,
- preserves arc fitting, flavor handling, and all writer-side features,
- removes the need to port `src/gcode.cpp` at all,
- makes CurviSlicer's `layer_thickness` mismatch check (which currently
  `exit(-1)`s on mismatch) structurally impossible.

Extrusion compensation still uses the per-tet Jacobian, applied to each path's
`mm3_per_mm` rather than to emitted `E` values.

---

## Dependencies to resolve

| Dependency | Issue | Options |
|---|---|---|
| **TetWild** | Tetrahedral meshing; heavy, can be slow/fragile on non-watertight input | Vendor it; or evaluate fTetWild (faster); or CGAL. Needs a mesh-repair guard before it. |
| **OSQP** | QP solver, ~small, Apache-2.0 | Vendor. OrcaSlicer already builds many deps. |
| **Gurobi** | Paper results used it; **commercial licence** | Do **not** depend on it. Accept OSQP quality. |
| **LibSL** | Lefebvre's utility library used throughout CurviSlicer | Replace with OrcaSlicer's own `Eigen`/`TriangleMesh` types during the port rather than vendoring another framework. |

> The authors state plainly: *"Please don't expect high quality, production ready
> code, this is a research prototype."* And: the OSQP path "works great, [but]
> there are differences and limitations compared to the Gurobi version." Budget
> for numerical debugging, not just plumbing.

---

## Hardware constraint that bounds the whole feature

On a 3-axis machine the nozzle stays vertical, so the achievable curvature is
limited by **collision between the heatblock/fan shroud and the already-printed
curved surface** — not by the software. This sets the max deposition slope the
optimizer may use.

Practical consequences for this printer:

- Measure the real clearance cone of the installed hotend (nozzle tip to the
  widest point of the block/shroud) and derive the max slope angle from it. That
  number is a **hard input to the optimizer**, not a preference.
- A stock-profile hotend typically allows a shallow angle. A pointed/tapered
  nozzle and a trimmed shroud raise it materially.
- This is the single most likely reason first prints disappoint. Establish the
  clearance angle **before** tuning the solver.

---

## Suggested build order

1. **Harness first.** Fork OrcaSlicer, add a no-op deform/un-deform pass wired
   into both injection points, verify output is byte-identical to upstream.
   This proves the plumbing before any math lands.
2. **Tet mesh + point location.** Port `TetMesh` onto Orca's types; validate
   `find_containing_tet` against known points.
3. **Inverse deformation with a synthetic field.** Drive the un-deform stage with
   a hand-written analytic `h` (e.g. a gentle dome). Verifiable without the solver
   and prints something real on the LK5 Pro.
4. **QP solver.** Port `main.cpp` onto OSQP last — it is the piece most likely to
   need numerical iteration, and steps 1–3 make its output inspectable.
5. **Extrusion compensation and reflow.**

Step 3 is the first point at which the printer produces a curved-layer part, and
it is reachable without the solver. Prioritise getting there.

---

## G-code flavor: Marlin first — decided

Target **Marlin flavor first**, then Klipper. Rationale: the Octopus is running
Marlin now, so Marlin output is testable against real hardware immediately, and
the un-deform stage can be validated before the Klipper migration adds a second
unknown. Keep flavor-specific emission behind OrcaSlicer's existing
`GCodeFlavor` handling rather than hard-coding — the Klipper switch should then
be a configuration change, not a second port.

Note this only concerns *emission*. The deform / un-deform math is
flavor-independent, so nothing about the Klipper migration invalidates steps 1-5
of the build order.

## Open questions

- Measured hotend clearance angle on the modified LK5 Pro (blocks step 3 tuning).
