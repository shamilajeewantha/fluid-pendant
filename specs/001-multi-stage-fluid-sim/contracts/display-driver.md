# Contract: Charlieplex Display Driver Module

**Source**: new `display/charlieplex_driver.c` in the Stage 3 firmware project, adapted from
`stm_projects/sample-charlieplexing`'s TIM+DMA pattern (research.md Decision 4).

## Contract

- Input: one `DisplayFrame` (16 × 15 binary cells, per data-model.md) per refresh cycle.
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
