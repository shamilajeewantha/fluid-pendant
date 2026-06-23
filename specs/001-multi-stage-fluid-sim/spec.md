# Feature Specification: Multi-Stage Fluid Simulation Pendant

**Feature Branch**: `001-multi-stage-fluid-sim`

**Created**: 2026-06-21

**Status**: Draft

**Input**: User description: "Build a multi-stage fluid simulation project. Stage 1: Create a very simple JavaScript fluid simulation prototype running on a web page for quick visual testing. Stage 2: Create a matching C simulator as a Windows application, reusing the existing, already-functional C simulation source found in the local 'final projects' directory rather than rewriting the math. Stage 3: Build an STM32CubeIDE-compatible bare-metal project utilizing the identical ported C simulation logic. The firmware must read data from an accelerometer and map those inputs directly to the simulation, then output the physics states onto a custom charlieplexed LED matrix configured with 16 rows and 15 columns. The core physics calculation must remain fully modular and separate from the visual display and hardware driver layers."

## Clarifications

### Session 2026-06-23

- Q: The Edge Cases section requires the display to "remain visually stable" under a "sudden acceleration spike" / being "shaken hard," but doesn't quantify that threshold, and `research.md` Decision 22 names a magnitude-clamp constant without a value. What should the accelerometer-derived gravity magnitude be clamped at before it reaches the simulation? → A: Clamp at `19.62 m/s²` (the MPU-6500's own ±2g full-scale range) — rely on the sensor's own physical ceiling rather than further restricting it to the simulation's normal 1g resting magnitude.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Quick Visual Prototype in the Browser (Priority: P1)

A developer wants to see and tweak fluid-like motion (gravity direction, flow behavior) in a normal web browser, with no build tools, hardware, or compilation step, so they can validate "does this look and feel right" before committing logic to the embedded target.

**Why this priority**: This is the fastest feedback loop in the whole project. Every later stage (Windows simulator, firmware) is judged against "does it still look like this." Without it, the team has no quick way to validate physics tuning decisions.

**Independent Test**: Open the web page in a browser with no other part of the project built. Interact with the on-screen controls (e.g., gravity direction) and confirm the fluid visibly responds within the same session — delivers value entirely on its own as a design/tuning tool.

**Acceptance Scenarios**:

1. **Given** the web page is open in a browser, **When** the user changes the simulated gravity direction, **Then** the on-screen fluid visibly redirects its flow within a few animation frames.
2. **Given** the simulation is running, **When** the user leaves it idle, **Then** the fluid continues to animate smoothly without freezing, crashing, or visibly breaking down (e.g., particles escaping the visible area permanently).

---

### User Story 2 - Desktop Simulator for Pre-Hardware Validation (Priority: P2)

A developer wants to run the exact same physics logic that will eventually run on the embedded device, on a Windows PC, displayed at the same grid resolution as the final LED matrix, so they can validate simulation behavior, performance, and visual output before ever touching real hardware.

**Why this priority**: Hardware bring-up (flashing firmware, wiring sensors, debugging LEDs) is slow and hard to observe. Validating the ported physics on a PC first removes one major variable from hardware debugging and protects the investment already made in the working C simulation source.

**Independent Test**: Build and run the Windows application by itself. Confirm it reproduces fluid motion using the reused simulation source (not reimplemented math), displayed on a grid matching the final display's row/column count — delivers value as a standalone validation tool even before any embedded work starts.

**Acceptance Scenarios**:

1. **Given** the existing working C fluid simulation source has been located and brought into this project, **When** the Windows simulator is built, **Then** it runs using that same simulation source without its core math having been rewritten.
2. **Given** the Windows simulator is running, **When** simulated input (e.g., a control representing tilt) changes, **Then** the displayed fluid state updates consistently with the physics model, at the same grid dimensions (16 rows by 15 columns) intended for the final device.

---

### User Story 3 - Motion-Reactive Pendant (Priority: P3)

A person wearing or holding the finished pendant wants the LED matrix to display fluid motion that visibly reacts to how they tilt or move the device, so the object feels alive and physically responsive rather than just playing a fixed animation.

**Why this priority**: This is the final product experience, but it depends entirely on Stages 1 and 2 having already produced and validated correct simulation behavior — it is the last and most expensive stage to test (requires real hardware), so earlier stages exist specifically to de-risk it.

**Independent Test**: With assembled hardware (accelerometer, charlieplexed LED matrix, microcontroller) and no host PC connection, tilt or move the device and observe that the LED matrix pattern changes in a way that visibly corresponds to the motion — delivers the complete end-user experience on its own.

**Acceptance Scenarios**:

1. **Given** the firmware is running on the device, **When** the device is tilted in a given direction, **Then** the displayed fluid pattern on the LED matrix visibly shifts toward that direction within a short, perceptible delay.
2. **Given** the firmware is running, **When** the device is held still, **Then** the fluid settles into a stable resting pattern rather than oscillating indefinitely or displaying visual artifacts.
3. **Given** the same simulation logic validated in Stage 2, **When** it runs on the embedded target, **Then** the physics calculation code itself is the same ported logic, not a separate reimplementation, with no embedded-only behavioral divergence beyond what hardware constraints require.

---

### Edge Cases

- What happens when the device is shaken hard or experiences a sudden acceleration spike? The displayed fluid MUST remain visually stable (no frozen frames, no runaway/exploding particle behavior) even under input far outside normal handling motion — the accelerometer-derived gravity magnitude fed into the simulation MUST be clamped at 19.62 m/s² (the MPU-6500's own ±2g full-scale range) before being applied (Clarifications, Session 2026-06-23).
- What happens if the accelerometer briefly fails to provide a reading (e.g., a single missed sample)? The simulation MUST continue running using the most recent valid input rather than stopping or displaying a corrupted frame.
- How does the system handle the mismatch between a 16-row and 15-column display (not square)? The mapping from simulation state to LED matrix MUST account for the non-square shape so the displayed pattern is not stretched, cropped, or misaligned.
- What happens during the brief period after power-on before the first accelerometer reading arrives? The display MUST show a defined neutral/resting state rather than undefined or garbage output.
- What happens if the Windows simulator and the browser prototype show visibly different fluid behavior for the same input? This is acceptable for Stage 1 (a quick approximate prototype) but NOT acceptable between Stage 2 and Stage 3, which must share identical ported physics logic.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST provide a browser-based fluid simulation prototype that runs by opening a web page, with no installation, build step, or hardware required.
- **FR-002**: The browser prototype MUST let a user change at least one simulation input (e.g., simulated gravity direction) and visibly observe the resulting change in fluid behavior in real time.
- **FR-003**: The project MUST provide a Windows desktop application that runs the fluid simulation using the existing, already-functional simulation source code located in the project's prior work, rather than a newly rewritten physics implementation.
- **FR-004**: The Windows desktop simulator MUST display the simulation state at the same grid resolution (16 rows by 15 columns) as the final embedded display target, so its visual output is directly representative of the eventual hardware behavior.
- **FR-005**: The project MUST provide a bare-metal embedded firmware project, buildable in STM32CubeIDE, that runs the identical ported physics logic validated in the Windows simulator (same source logic, adapted only as required for the embedded environment).
- **FR-006**: The firmware MUST read live motion data from an onboard accelerometer and use that data as the driving input to the fluid simulation (e.g., as the simulated gravity/force vector).
- **FR-007**: The firmware MUST convert the current simulation state into a displayable pattern and drive that pattern onto a charlieplexed LED matrix wired as 16 active row signals by 15 column signals.
- **FR-008**: The physics calculation logic MUST be implemented as a self-contained module with no direct dependency on the display rendering or hardware driver code, such that the physics module can be exercised independently of the LED matrix and accelerometer driver.
- **FR-009**: The hardware driver logic (accelerometer reading, LED matrix multiplexing/drive) MUST be implemented separately from the physics module, such that drivers can be modified without changing physics behavior and vice versa.
- **FR-010**: The system MUST keep producing a valid, stable displayed pattern continuously during normal operation, without requiring a host computer connection once firmware is running on the device.
- **FR-011**: The system MUST tolerate momentary invalid or missing sensor input without crashing, freezing, or displaying a corrupted/undefined pattern.

### Key Entities

- **Fluid Simulation State**: The current physics representation of the simulated fluid (e.g., particle or cell positions, velocities, density/occupancy) at a point in time; the thing each stage visualizes differently but must compute consistently between Stage 2 and Stage 3.
- **Motion Input**: A directional/force value derived from either a user control (Stages 1–2) or a live accelerometer reading (Stage 3), fed into the simulation as the driving force (e.g., simulated gravity).
- **Display Frame**: A derived 16-row by 15-column on/off (or intensity) grid produced from the current Fluid Simulation State, representing what the LED matrix should currently show.
- **LED Matrix**: The physical charlieplexed output device, addressed via 16 row signals and 15 column signals, that renders each Display Frame.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can open the browser prototype and observe a visible, correct fluid response to a control change within 1 second of interacting with it, with no setup beyond opening the page.
- **SC-002**: The Windows simulator reproduces fluid behavior using 100% reused simulation source logic — zero physics formulas are rewritten from scratch for Stage 2.
- **SC-003**: The same physics module compiles and runs unmodified in logic (only environment-specific adaptation, no behavioral rewrite) across both the Windows simulator and the embedded firmware.
- **SC-004**: On real hardware, a person tilting the device observes the LED matrix pattern visibly responding to that tilt within a perceptible, near-immediate delay (under half a second).
- **SC-005**: The assembled device can run continuously for at least 10 minutes of varied handling (tilting, shaking, resting) without the display freezing, corrupting, or requiring a manual reset.
- **SC-006**: A reviewer can identify the physics calculation code and confirm it contains no direct references to accelerometer registers, GPIO pins, or LED-drive timing — full separation from hardware and display code.

## Assumptions

- The "already-functional C simulation source" referenced by the user is the existing fluid simulation code found in this project's `final_project` directory; it is treated as the canonical physics reference for Stages 2 and 3 and is reused rather than rewritten.
- The browser prototype (Stage 1) is a fast, approximate visual reference for tuning and demos; it is not required to share literal source code with the C-based Stages 2–3, only to demonstrate comparable fluid-like behavior for quick visual testing.
- The Windows simulator (Stage 2) renders the fluid state at the same 16x15 resolution as the final hardware display so it can serve as an accurate pre-flash preview, rather than at an arbitrary/different demo grid size.
- **(Re-revised 2026-06-22, research.md Decision 15, reverting the 2026-06-21 Decision 13 revision)** Each LED cell in the Display Frame is treated as on/off (binary occupancy) again, matching the early prototype's original convention — the real charlieplexed hardware was confirmed not to support per-LED PWM/brightness after all (this project's own `sample-charlieplexing` reference driver holds exactly one on/off pattern per scan, not multiple brightness bit-planes), so the brief continuous-brightness detour was reverted to keep the simulators an accurate preview of real hardware capability. The binary decision is still made from a hysteresis threshold on local particle density (not a raw step-function flag), so it does not reintroduce the surface flicker that motivated the original brightness experiment.
- The accelerometer and charlieplexed LED matrix are existing, already-defined hardware (consistent with this project's established hardware constraints); this feature covers firmware/software behavior, not new hardware or schematic design.
- Motion input mapping uses the accelerometer's tilt/orientation reading as a 2D directional force (e.g., simulated gravity angle), matching the existing prototype's gravity-direction control concept.
- "Identical ported C simulation logic" means the same algorithm and source-level logic is shared between the Windows simulator and the firmware; minor, clearly-isolated adaptations required strictly for running without an operating system (e.g., memory allocation strategy) are permitted but must not change simulation behavior.
