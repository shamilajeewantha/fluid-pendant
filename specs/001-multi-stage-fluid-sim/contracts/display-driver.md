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
