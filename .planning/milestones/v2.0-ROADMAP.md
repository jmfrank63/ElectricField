# Roadmap — Milestone v2.0: Charge Editor

**Created:** 2026-10-11
**Status:** Ready for planning
**Milestone goal:** As a user, I want to create, place and edit my own charge configurations — stored in a config file, moved by mouse, cursor keys and numeric fields, redrawn in realtime — and recompute them explicitly with a Calculate button and a per-calculation method choice.

## Phases

### Phase 1: User Configuration Files

**Goal:** As a user, I want to save my charge arrangements by name and reopen them later, so that my own configurations are first-class citizens next to the 9 built-in presets.
**Mode:** mvp
**Depends on**: —
**Requirements**: CFG-01, CFG-02, CFG-03
**Success Criteria** (what must be TRUE):

1. A human-readable `configurations.ini` in the platform user config directory stores named charge sets (value, x, y per charge); missing or invalid entries fall back safely and the 9 built-in presets are unaffected
2. The preset chooser lists user configurations below the built-ins; opening one creates a tab from its charges; File > New > "Empty Configuration" opens an editable empty tab
3. Save As (Ctrl+S / File > Save As) writes the current tab's charges under a chosen name into `configurations.ini`, overwriting an existing name only after confirmation; presets cannot be overwritten

**Plans**: TBD

### Phase 2: Charge Editor & Explicit Calculation

**Goal:** As a user, I want to place and fine-tune charges in the canvas — by mouse, cursor keys and numeric fields with up/down buttons — see the charges move in realtime, and recompute explicitly with a Calculate button and a per-calculation method.
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: EDIT-01, EDIT-02, EDIT-03, EDIT-04, EDIT-05, CALC-01, CALC-02
**Success Criteria** (what must be TRUE):

1. Clicking on or near a charge selects it; dragging moves it; the charge glyphs follow the mouse in realtime; arrow keys nudge the selected charge (Shift = larger step); double-clicking empty canvas adds a charge; Delete removes the selected one (at least one charge must remain)
2. The editor panel shows the selected charge's value, x and y as input fields with up/down buttons; every change moves the charge on the canvas immediately — without starting any computation
3. A Calculate button (F5) recomputes the tab from the current charges; after any edit the tab is marked stale (the drawn lines belong to the pre-edit charges) until Calculate is pressed or the tab is saved/reverted
4. The editor panel has a method selector defaulting to the app option; it applies at the next Calculate, never retroactively
5. CI stays green on all three OSes plus TSan; the smoke exercises add/select/move/calculate headlessly; the README documents the editor

**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. User Configuration Files | 2/2 | Complete | 2026-10-11 |
| 2. Charge Editor & Explicit Calculation | 2/2 | Complete | 2026-10-11 |

## Out of scope (v2)

- Continuous/line charge distributions, 3D, dielectrics
- Progressive drawing (lines appearing while computing) — still a wish, separate milestone
- Copy-to-clipboard / PDF export
- Undo/redo of charge edits (v2 keeps: edits are visually immediate and cheap to redo by hand; revert-to-computed exists via stale marker + Calculate)

## Long-term direction (v3+, recorded 2026-10-11)

The owner's long-term goals are **3D charges, magnetic fields and quantum behaviour**. Deliberately NOT
stubbed in v2 (no dead z coordinate, no 0-% fields in the UI or the config file): these are structural
migrations of the math core, not parameter additions —

- **3D:** Vec2 → Vec3 across field/potential formulas, seeds, tracer and scene; the renderer needs a
  projection decision. The editor's 2D placement UX remains the basis, extended with a z input.

- **Magnetic fields:** the traced ODE gains the Lorentz term (v x B); integrators and the de-dup/finalizer
  follow. B parameters belong to the physics config of that milestone, not to v2 files.

- **Quantum behaviour:** a different computation engine (probability densities) alongside the
  deterministic lines; own requirements milestone.

Compatibility kept cheap on purpose: `configurations.ini` is parsed in one place and `ef::PresetCharge`
is the single charge interchange type, so the later format extension (optional z token + format marker)
is a small, local change.
