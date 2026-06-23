# Contract: Charlieplex Display Driver Module

**Source**: new `display/charlieplex_driver.c` in the Stage 3 firmware project, adapted from
`stm_projects/sample-charlieplexing`'s TIM+DMA pattern (research.md Decision 4).

## Contract

- Input: one `DisplayFrame` (16 × 15 binary cells per data-model.md / research.md Decision 15 —
  reverted from a brief continuous-brightness detour, Decision 13, once it was confirmed the real
  charlieplex hardware has no per-LED PWM) per refresh cycle. Matches
  `sample-charlieplexing`'s existing `bsrr_buf[240]`/`moder_buf[240]` structure directly: one
  on/off pattern per LED slot, no bit-plane buffers needed.
- Owns the `LedMatrixAddressing` lookup table — translates each `DisplayFrame` cell into the
  (drive-high pin, drive-low pin, Hi-Z others) pattern for that scan step. This table MUST live
  entirely inside this module; no other module may reference physical pin numbers for the display.
- MUST drive the matrix via timer-triggered DMA writes to `GPIOx->BSRR` (pin level) and
  `GPIOx->MODER` (pin direction/Hi-Z), not CPU polling (constitution: no polling loops; DMA for
  display transfers).
- MUST sustain a scan rate that yields a perceived refresh ≥ 10 FPS for the full 16x15 frame
  (constitution Principle IV).
- MUST NOT call into the physics core or sensor driver, and MUST NOT contain any FLIP/PIC physics
  logic — it only renders whatever `DisplayFrame` it is given.

## Consumers

- Stage 3's `main.c` glue: builds a `DisplayFrame` from the physics core's output each tick and
  calls this module to (re)start/update the DMA-driven scan.

## Verification

- On-hardware check (per constitution Development Workflow): boot, observe that a known test
  pattern lights the correct 16x15 cells with no ghosting between adjacent charlieplex pairs,
  measure actual refresh rate.

## Round 8 status (2026-06-22, research.md Decision 17)

The DMA scan mechanism itself (this contract's core requirement) is implemented and confirmed
working on real hardware. What's still a placeholder is the *input* side: there is no real
`DisplayFrame` yet (the physics core isn't ported onto this board), so `axelor` now has a
firmware-local `AxelorFrameBuffer` (data-model.md) filled by a deterministic placeholder pattern
generator instead, refreshed every ~0.5s and translated into `moder_buf` only. The `LedMatrixAddressing`
mapping this contract requires (`DisplayFrame` index → physical pin pair) is also still open — the
placeholder pattern is deliberately defined in raw slot-index order rather than real (row, col)
positions, specifically because that mapping isn't known yet (see research.md Decision 17's
rationale). Swapping the pattern generator for a real `DisplayFrame` producer later should be a
drop-in replacement at the frame-buffer boundary; the `LedMatrixAddressing` mapping still needs to
be resolved before that swap can mean anything geometrically.

## Round 9 status (2026-06-23, research.md Decision 23)

The `LedMatrixAddressing` row/column mapping flagged as open above is now resolved at the row
level: the board's confirmed wiring (each PB pin takes a turn as one row's source pin, the other 15
as that row's sink pins) means `ChargePlex_BuildScanTable`'s existing `(src,dst)` enumeration order
already groups into exactly 16 contiguous 15-slot row blocks with no further lookup table needed —
`AxelorFrameBuffer.cpFrameBuf` was reshaped to `[16][15]` to make this explicit (data-model.md). The
placeholder pattern generator predicted in the prior status update has now actually been swapped for
a real `DisplayFrame` producer: `FlipFluid.cellColor`'s row-major output is copied directly into
`cpFrameBuf[row][col]`, confirmed index-for-index identical with no remapping required. The swap
was exactly the drop-in replacement at the frame-buffer boundary this contract anticipated —
`ChargePlex_ApplyFrameBuffer()`'s validate-or-reject gate and everything in this module below it are
unmodified. **Still open**: the *column*-level mapping (which of a row's 15 sink pins corresponds to
which real left-to-right column position) remains unconfirmed — irrelevant for Round 8/9's
whole-row or whole-cell-driven patterns, but would matter if a future round needs to verify
individual-column correctness against the physical board rather than just row groupings.

**Reconciled against as-built code (tasks.md T102)**: this status held exactly as planned, with no
drift — `main.c`'s `while(1)` body copies
`axelorScene.fluid->cellColor[row*GRID_X+col] > 0.5f` into `cpFrameBuf[row][col]` for every
`row`/`col`, then calls the unmodified `ChargePlex_ApplyFrameBuffer()`. Confirmed unchanged by
diff-reading: `ChargePlex_ApplyFrameBuffer()`, `ChargePlex_ValidateScanTable()`,
`ChargePlex_BuildScanTable()`, `ChargePlex_ForceSafeState()`, `ChargePlex_CheckAlive()`,
`bsrr_buf`, `moder_template`. Build-verified via headless STM32CubeIDE build: 0 errors, 0 warnings.

## Round 10 status (2026-06-23, research.md Round 10) — column mapping resolved; root cause was wiring, not software

The "Still open" column-level mapping noted above is now resolved — but not by a firmware change.
A dedicated raw row+column sweep was added to `axelor-test` (per explicit instruction to keep
`axelor` itself frozen until this was understood) and initially suggested both row and column
needed a firmware-side coordinate transform (a row offset+reversal and a column mirror, both
applied and build-verified in `axelor`). Before that compensation was validated further, the user
found and fixed a **physical wiring defect** on the board; re-running `axelor-test`'s sweep
afterward showed it now sweeps exactly as expected — rows top-to-bottom, columns left-to-right —
with the raw, untransformed `(src,dst)` enumeration from `ChargePlex_BuildScanTable` alone. The
firmware compensation was therefore retracted from `axelor`'s `main.c`: the `cellColor`→
`cpFrameBuf` copy step is back to the direct, untransformed mapping research.md Decision 23
originally specified, with no row/column transform of any kind. `bsrr_buf`, `moder_template`, and
every `ChargePlex_*` function were never touched by any of this — confirmed unmodified throughout
all three passes.
