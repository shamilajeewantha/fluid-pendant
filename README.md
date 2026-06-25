# Fluid Simulator

A standalone device that simulates fluid motion in real time and displays it on a custom
LED matrix. A built-in motion sensor detects how the device is tilted or moved, and feeds
that directly into the simulation as gravity, so the displayed fluid responds accordingly.
Powered over USB-C with an internal rechargeable battery.

<p align="center">
  <img src="media/photos/pcb/simulation.jpg" width="400" alt="Fluid simulation running on the device" />
</p>

## Overview

This project went through four stages, in this order:

- **Simulation** - building and optimizing the fluid physics in software, first.
- **Prototyping** - proving the display and wiring techniques on off-the-shelf dev boards.
- **PCB** - designing the real, purpose-built circuit board.
- **STM32 Firmware** - running the simulation on that real board.

## Simulation

The fluid physics was written once and reused, unmodified in behavior, across two software
stages before it ever touched hardware. Each folder below has its own README with exact
build/run commands.

<p align="center">
  <img src="media/photos/simlators/web_simulator.png" width="400" height="300" style="object-fit:cover;" alt="Web simulator screenshot" />
</p>

- **[Web Simulator](simulators/web_simulator/)** - JavaScript browser prototype, based on
  [Matthias Müller's tenMinutePhysics FLIP tutorial](https://matthias-research.github.io/pages/tenMinutePhysics/18-flip.html).
  The main change from his original is rendering the simulation as a grid of blocks (matching
  the final LED matrix), instead of individual particles drawn on screen.

<p align="center">
  <img src="media/photos/simlators/windows_simulator.png" width="400" height="300" style="object-fit:cover;" alt="Windows simulator screenshot" />
</p>

- **[Windows Simulator](simulators/windows_desktop_simulator/)** is a C port of the same
  physics core, rendered at the final 16x15 grid resolution. It builds with MinGW, or with
  Docker if MinGW isn't installed. This C code was written to drop into the STM32 firmware
  unmodified, so the simulation logic never has to be debugged directly on the
  microcontroller.

## Prototyping

Before any custom PCB was designed, the simulation was proven to run on real hardware using
off-the-shelf dev boards.

<p align="center">
  <img src="media/photos/prototype/bluepill_simulation.jpg" width="400" height="300" style="object-fit:cover;" alt="Blue Pill driving an 8x8 LED matrix" />
</p>

- **STM32F103C8T6 ("Blue Pill")** driving an off-the-shelf 8x8 (64-LED) matrix module.
  Source: [`STM32F103C8T6_projects/stm-fluid-led`](https://github.com/shamilajeewantha/fluid-pendant/tree/dev-testing/STM32F103C8T6_projects/stm-fluid-led)
  on the `dev-testing` branch.

<p align="center">
  <img src="media/photos/prototype/nucleo_charlieplex.png" width="400" height="300" style="object-fit:cover;" alt="Nucleo board driving a charlieplexed LED test" />
</p>

- **NUCLEO-L432KC** used to prototype charlieplexing itself - notoriously fiddly wiring.
  A simple 3-pin, 6-LED charlieplex, scanned via DMA.

## Axelor

Axelor is the custom PCB this project runs on: an STM32L431CC microcontroller drives the
16x15 charlieplexed LED matrix, reads an onboard MPU-6500 accelerometer, and charges over
USB-C.

### Schematic & Components

<p align="center">
  <img src="media/photos/altium/main_board_sch.png" width="400" height="300" style="object-fit:cover;" alt="Main board schematic" /><br/>
  <img src="media/photos/altium/led_matrix_sch.png" width="400" height="300" style="object-fit:cover;" alt="LED matrix schematic" />
</p>

Key components (full BOM [here](pcb/jlc_order/axelor_pcb-BOM.csv)):

<div align="center">

| Ref | Part | Role |
|---|---|---|
| U3 | STM32L431CCT6 | MCU - Cortex-M4, 256KB flash |
| U4 | MPU-6500 | 3-axis accelerometer/gyro (I2C) |
| U2 | TPS7A0233PDBVR | 3.3V LDO regulator |
| U1 | MCP73832T-2ACI/OT | Li-ion/Li-poly charge controller |
| PWR_IN1 | USB Type-C receptacle | Power input |
| D1-D240 | 0402 blue LEDs | 16x15 charlieplexed display |

</div>

### PCB Design

It's a single-sided PCB, designed to be cut in half into two separable boards: the bottom
half holds power, the MCU, and the accelerometer (usable entirely on its own for other
projects), and the top half is just the LED matrix, which stacks on top of the bottom half.

<p align="center">
  <img src="media/photos/altium/pcb_2d.png" width="400" height="300" style="object-fit:cover;" alt="PCB 2D layout" />
  <img src="media/photos/altium/pcb_3d.png" width="400" height="300" style="object-fit:cover;" alt="PCB 3D render" />
</p>

- **[Altium Project](pcb/altium_project/)** - Altium Designer project (schematic + PCB
  layout), packaged with Altium's Project Packager.

### JLC Order

If you want to fabricate this yourself, the order files are here:

- **[JLC Order](pcb/jlc_order/)** - Gerbers, BOM, and CPL files as submitted to JLCPCB.

### Fabricated Board

<p align="center">
  <img src="media/photos/pcb/axelor1.jpg" width="400" height="300" style="object-fit:cover;" />
  <img src="media/photos/pcb/axelor2.jpg" width="400" height="300" style="object-fit:cover;" />
  <img src="media/photos/pcb/axelor3.jpg" width="400" height="300" style="object-fit:cover;" />
  <img src="media/photos/pcb/axelor4.jpg" width="400" height="300" style="object-fit:cover;" />
  <img src="media/photos/pcb/axelor7.jpg" width="400" height="300" style="object-fit:cover;" />
  <img src="media/photos/pcb/axelor9.jpg" width="400" height="300" style="object-fit:cover;" />
</p>

More build, bring-up, and debugging photos [here](media/photos/).

## STM32 Firmware

- **[STM32 Project](stm_project/axelor/)** - STM32CubeIDE project for the Axelor board
  (STM32L431CCTx). Reads the onboard accelerometer and drives the simulation's physics core
  - unmodified in behavior from the Windows simulator - onto the real charlieplexed LED
  matrix.

<p align="center">
  <img src="media/photos/pcb/flashing.jpg" width="400" height="300" style="object-fit:cover;" /><br/>
  <img src="media/gif/simulation.gif" width="400" height="300" style="object-fit:cover;" />
</p>

## Acknowledgments

- **[mitxela's "Fluid Simulation Pendant"](https://mitxela.com/projects/fluid-pendant)**
  ([Hackaday writeup](https://hackaday.com/2025/01/13/fluid-simulation-pendant-teaches-lessons-in-miniaturization/),
  [Hackaday.io](https://hackaday.io/project/205649-fluid-simulation-pendant)) - the direct
  inspiration for this project's overall architecture (FLIP simulation + charlieplexed
  matrix + accelerometer, on the same STM32L4 family). This board is **not** a clone of his -
  several deliberate departures from his exact build:
  - **Two rectangular boards instead of one round one.** A single round PCB at his size/layer
    count is expensive to fabricate; splitting into a rectangular main board + a separate LED
    matrix board, and dropping the round pendant shape, kept cost and complexity down.
  - **USB-C charging + a standard battery connector**, instead of coin-cell support.
  - **STM32L431CC instead of his STM32L432KC** - more GPIO means the whole LED matrix can be
    driven from a single GPIO port group (all of `PB`), and it's cheaper to assemble on JLCPCB.
  - **MPU-6500 instead of an ADXL362** - lower power, cheaper part, and fewer assembly
    requirements on JLCPCB, so assembly cost is lower too.
  - **Extra GPIO broken out on headers** that his design doesn't expose, so this board can be
    reused on other projects.
- **[Matthias Müller's tenMinutePhysics FLIP tutorial](https://matthias-research.github.io/pages/tenMinutePhysics/18-flip.html)**
  - the FLIP/PIC algorithm this project's `flip_fluid.c`/`flip.js` are ported from.
- Select component footprints/3D models sourced from **SnapEDA**.

## License

This project is licensed under the [MIT License](LICENSE) - free and open for anyone to
use, modify, and distribute. Bundled ST CMSIS/HAL driver files under
`stm_project/axelor/Drivers/` retain their own respective licenses (see the `LICENSE.txt`
files in those folders).
