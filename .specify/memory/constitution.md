<!--
SYNC_IMPACT_REPORT
Version change: 2.0.0 → 2.1.0
Modified sections:
  - Hardware Constraints — Sensor corrected from ADXL362 to MPU-6500 (SPI, accelerometer axes
    only; gyroscope disabled to save power)
Rationale for MINOR bump: the user corrected the sensor part after this constitution was already
amended once in this session for the MCU. This is a Hardware Constraint correction (different
part number, different register map/power-mode table) but does not redefine or remove a
principle, so it is MINOR rather than MAJOR.
Added sections: None
Removed sections: None
Templates requiring updates:
  ✅ .specify/templates/plan-template.md — Constitution Check gate reviewed; aligned
  ✅ .specify/templates/spec-template.md — Performance Goals / Constraints fields aligned
  ✅ .specify/templates/tasks-template.md — Task phase structure reviewed; no structural changes needed
  ⚠ .specify/extensions/agent-context/ — Run /speckit-agent-context-update to refresh agent context
Deferred items: None

Prior entry (preserved for history):
Version change: 1.0.0 → 2.0.0
Modified principles:
  - IV. Performance Requirements — fixed-point mandate replaced with single-precision-float
    guidance; frame-budget reference chip corrected
Modified sections:
  - Hardware Constraints — Target MCU corrected from STM32F103C8T6 to STM32L431CC
Rationale for MAJOR bump: the original constitution recorded the wrong physical target
(STM32F103C8T6, no FPU) for this project. The actual hardware is the existing `axelor`
board (STM32L431CC, Cortex-M4F, hardware FPU, confirmed via stm_projects/axelor/axelor.ioc
and https://www.st.com/en/microcontrollers-microprocessors/stm32l431cc.html). This is a
correction of a previously-wrong constraint, not a deliberate loosening, but it reverses a
load-bearing engineering rule (fixed-point arithmetic was mandatory; it is now unnecessary)
so it is treated as backward-incompatible per the MAJOR rule.
-->

# Fluid Pendant Constitution

## Core Principles

### I. Clean Code Quality

All C source code MUST be readable, self-documenting, and reviewable without verbal explanation.

- Identifiers MUST be descriptive; abbreviations are prohibited except for established domain terms
  (e.g., `vel`, `pos`, `sim`).
- Magic numbers MUST be replaced with named constants (`#define` or `const`).
- Functions MUST have a single, clear responsibility; any function exceeding 40 lines MUST be
  refactored unless the exception is justified with an inline comment explaining why.
- Global mutable state MUST be minimized; any shared state MUST be documented with ownership noted
  in the owning module's header.
- All public API headers MUST include a one-line description of purpose, parameters, and return value.

**Rationale**: Embedded C has a historically poor readability reputation. Enforcing these rules keeps
the codebase maintainable across long gaps between contributions and across multiple contributors.

### II. Low-Power C Development for STM32

The firmware MUST minimize power consumption at all times; active computation is a last resort.

- The MCU MUST enter the lowest viable sleep mode (`STOP` or `STANDBY`) whenever the simulation is
  idle or awaiting the next frame tick.
- Peripherals (SPI, ADC, DMA channels) MUST be disabled when not actively in use.
- The system clock MUST run at the lowest frequency that still meets the simulation frame-rate target;
  frequency scaling is REQUIRED when transitioning between high- and low-activity states.
- Polling loops are PROHIBITED; all waiting MUST use interrupt-driven or DMA-driven patterns.
- Power consumption MUST be validated on real hardware; estimates from datasheets alone are
  insufficient for a merge approval.

**Rationale**: As a battery-operated wearable pendant, runtime is a primary user-visible quality
attribute. Violating this principle directly shortens usable device lifetime.

### III. Modular Architecture

The codebase MUST be decomposed into independently testable and replaceable modules.

- Hardware-specific code (GPIO, SPI, timers, power modes) MUST live in a HAL (Hardware Abstraction
  Layer) module; simulation logic MUST NOT reference register addresses or STM32 HAL functions
  directly.
- The fluid simulation core (`flip_fluid`, `flip_utils`, `scene`) MUST compile and run on a host PC
  without any STM32 HAL dependency, enabling off-device testing and iteration.
- Each module MUST expose its public interface exclusively through its header file; all internal
  helpers MUST be declared `static`.
- Circular dependencies between modules are PROHIBITED.
- New drivers or peripheral support MUST be added as new modules, not by extending existing ones.

**Rationale**: Modularity allows the simulation logic to be developed and debugged on a PC and then
deployed to hardware, dramatically shortening the iteration loop and reducing on-device debugging.

### IV. Performance Requirements

The fluid simulation MUST meet real-time frame-rate targets within the STM32's resource constraints.

- The simulation MUST sustain a minimum of 10 FPS on the LED display at the target grid resolution.
- Each simulation tick MUST complete within 50 ms on an STM32L431CC at its configured operating
  clock (up to 80 MHz).
- Flash usage MUST NOT exceed 80% of available flash; RAM usage MUST NOT exceed 80% of available
  SRAM; these limits MUST be verified in the linker map before any hardware PR is merged.
- Single-precision `float` arithmetic MUST be used throughout the simulation core (the target MCU
  has a hardware FPU); `double` MUST be avoided everywhere, since it is software-emulated and slow.
  All math library calls MUST use the `f`-suffixed single-precision form (`sqrtf`, `floorf`, etc.).
  Fixed-point arithmetic is NOT required and SHOULD NOT be introduced — it would add complexity
  with no performance benefit on this hardware.
- DMA MUST be used for all LED panel transfers (and SPI sensor transfers) to free the CPU for
  simulation computation during each frame.

**Rationale**: The STM32L431CC has 64 KB SRAM, 256 KB flash, and a hardware single-precision FPU
(Cortex-M4F). Exceeding resource budgets or missing frame-rate targets produces a directly
observable degraded experience; using `double` or unnecessary fixed-point math wastes the
headroom this FPU provides.

## Hardware Constraints

These are non-negotiable physical boundaries that all feature work MUST respect.

- **Target MCU**: STM32L431CC (Cortex-M4F, up to 80 MHz, 64 KB SRAM, 256 KB flash, hardware
  single-precision FPU), as used on this project's `axelor` board.
- **Display**: Charlieplexed LED matrix, addressed via 16 GPIO row signals and 15 GPIO column
  signals (16-pin charlieplexing, up to 16x15 = 240 addressable LEDs); exact pin assignments are
  defined per project variant and MUST be documented in `plan.md` for each feature.
- **Sensor**: MPU-6500 6-axis IMU via SPI, accelerometer axes only (X/Y for tilt; gyroscope axes
  disabled via `PWR_MGMT_2` `DIS_XG/YG/ZG` to save power — this project has no use for rotation
  rate). MUST run in a duty-cycled Low-Power Accelerometer Mode (not full 6-axis/Low-Noise mode)
  with the Data Ready interrupt driving each read; gravity input MUST be read via interrupt, not
  polling.
- **Power**: Battery-operated; supply voltage and peak/idle current draw MUST be documented per
  variant in the feature spec.
- All timing assumptions (sensor ODR, display refresh rate) MUST be derived from datasheets and
  verified experimentally, not assumed.

## Development Workflow

- All firmware changes MUST compile without warnings under `-Wall -Wextra`; warnings are treated as
  errors in CI.
- New modules MUST include a host-side test harness or simulation stub before the PR is merged.
- On-device validation (boot, run, frame-rate measurement) is REQUIRED before any PR targeting
  the STM32 sub-project is closed.
- Constitution Check violations (e.g., function length, polling loops) MUST be logged in `plan.md`'s
  Complexity Tracking table with a written justification before merge.

## Governance

This constitution supersedes all other documented practices for the Fluid Pendant project.

Amendments MUST:

1. Increment the version per semantic versioning:
   - **MAJOR**: backward-incompatible principle removal or redefinition.
   - **MINOR**: new principle or section added, or materially expanded guidance.
   - **PATCH**: clarifications, wording fixes, non-semantic refinements.
2. Update `LAST_AMENDED_DATE` to the date of the amendment.
3. Propagate changes to all `.specify/templates/` files listed in the Sync Impact Report.
4. Any amendment loosening a Hardware Constraint MUST include experimental validation evidence.

All implementation plans (`plan.md`) MUST include a Constitution Check gate that verifies compliance
before Phase 0 research begins and again after Phase 1 design.

**Version**: 2.1.0 | **Ratified**: 2026-06-21 | **Last Amended**: 2026-06-21
