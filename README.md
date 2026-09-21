# Automotive CAN Gateway & Telematics Cluster (Zephyr RTOS)

[![Zephyr Version](https://img.shields.io/badge/Zephyr%20RTOS-v3.7.0-blue.svg)](https://zephyrproject.org/)
[![Hardware](https://img.shields.io/badge/Hardware-STM32F746G--DISCO-red.svg)](https://www.st.com/en/evaluation-tools/32f746gdiscovery.html)
[![Standard](https://img.shields.io/badge/Standard-AUTOSAR%20E2E%20Profile%201-orange.svg)](#autosar-e2e-profile-1-validation)
[![Bus Standard](https://img.shields.io/badge/CAN%20Bus-ISO%2011898--1%20(500kbps)-green.svg)](#hardware-can-bit-timing--bus-off-recovery)
[![License](https://img.shields.io/badge/License-MIT-lightgrey.svg)](LICENSE)

An enterprise-grade **Automotive CAN Gateway & Telematics Cluster** built on **Zephyr RTOS** for the **STM32F746NG (ARM Cortex-M7 @ 216 MHz)**. The system implements hardware-filtered asynchronous CAN frame capture, a zero-floating-point **Vector DBC signal decoding engine**, 3-layer **AUTOSAR E2E Profile 1** safety validation, ISO 11898-1 Bus-Off state machine recovery, and a real-time interactive **Zephyr Shell CLI** for vehicle diagnostics.

---

## Table of Contents

- [System Architecture](#system-architecture)
- [Key Engineering Features](#key-engineering-features)
- [Hardware & Pin Configuration](#hardware--pin-configuration)
- [Vector DBC & Vehicle Signals](#vector-dbc--vehicle-signals)
- [AUTOSAR E2E Profile 1 Validation](#autosar-e2e-profile-1-validation)
- [Hardware CAN Bit Timing & Bus-Off Recovery](#hardware-can-bit-timing--bus-off-recovery)
- [Interactive Zephyr Diagnostic Shell](#interactive-zephyr-diagnostic-shell)
- [Project Directory Layout](#project-directory-layout)
- [Build, Flash & Verify with West](#build-flash--verify-with-west)
- [Author Information](#author-information)

---

## System Architecture

```text
                                CAN BUS TOPOLOGY (500 kbps)
                                              |
    +-----------------------------------------+-----------------------------------------+
    |                                         |                                         |
[Engine ECU]                             [Brake ECU]                           [Instrument Cluster]
    |                                         |                                         |
    +-----------------------------------------+-----------------------------------------+
                                              |
                                     CAN_H / CAN_L (120 Ohm)
                                              |
                                              v
                              +-------------------------------+
                              |    TJA1050 / SN65HVD230       | (Transceiver)
                              +---------------+---------------+
                                              |
                                      CAN_RX / CAN_TX (PB8 / PB9)
                                              |
                                              v
+-----------------------------------------------------------------------------------------------+
| STM32F746NG (ARM Cortex-M7 @ 216 MHz) - ZEPHYR RTOS ENVIRONMENT                              |
|                                                                                               |
|  +-----------------------------------------------------------------------------------------+  |
|  | Hardware CAN Controller (bxCAN1)                                                        |  |
|  |  - 14 Dedicated Hardware Filter Banks (ID & Mask Mode)                                  |  |
|  |  - Automatic Wakeup & Time-Triggered Communication Mode                                  |  |
|  +--------------------------------------------+--------------------------------------------+  |
|                                               |                                               |
|                               can_add_rx_filter_msgq() (Zero-Copy ISR Queue)                  |
|                                               |                                               |
|                                               v                                               |
|  +--------------------------------------------+--------------------------------------------+  |
|  | Thread 1: CAN Processing Worker Thread (Priority 2, Preemptive)                         |  |
|  |  - Pulls CAN frames from k_msgq asynchronously                                          |  |
|  |  - Dispatches to Vector DBC Fixed-Point Decoder                                         |  |
|  |  - Performs AUTOSAR E2E Profile 1 CRC-8 (SAE J1850 poly 0x2F) Verification             |  |
|  |  - Updates Global Vehicle Telemetry State protected by k_mutex                          |  |
|  +--------------------------------------------+--------------------------------------------+  |
|                                               |                                               |
|                      +------------------------+------------------------+                      |
|                      |                                                 |                      |
|                      v                                                 v                      |
|  +---------------------------------------+   +---------------------------------------------+  |
|  | Thread 2: Safety & Fault Supervisor   |   | Thread 3: Zephyr Interactive Shell (CLI)    |  |
|  | (Priority 5, Period: 100 ms)          |   | (Priority 10, ST-LINK VCP UART1 @ 115200)   |  |
|  |  - Frame Timeout Detection (> 500 ms) |   |  - vehicle status (Live telemetry table)    |  |
|  |  - DTC Storage (U0100, P0115, P0219)  |   |  - dtc read / dtc clear (Diagnostics)       |  |
|  |  - ISO 11898-1 Bus-Off Recovery FSM   |   |  - can sim <speed> (Hardware loopback test) |  |
|  +---------------------------------------+   +---------------------------------------------+  |
|                                                                                               |
|  +-----------------------------------------------------------------------------------------+  |
|  | Hardware Safety: ARM MPU Stack Guard (CONFIG_MPU_STACK_GUARD=y)                         |  |
|  +-----------------------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------------------+
```

---

## Key Engineering Features

* **Zero-Lock Asynchronous Ingestion:** Uses Zephyr's `can_add_rx_filter_msgq()` to connect hardware acceptance filter banks directly to kernel ring buffers (`k_msgq`), handling 1000+ frames/sec with zero CPU polling.
* **Vector DBC Fixed-Point Engine:** Signal extraction uses bit shifting and 32-bit fixed-point arithmetic instead of floating-point units (FPU), maintaining high throughput and execution determinism.
* **AUTOSAR E2E Profile 1:** Every safety-critical frame is validated against Data ID, a 4-bit monotonic sequence counter, and CRC-8 (polynomial `0x2F`), mitigating frame corruption and replay attacks.
* **ISO 11898-1 Bus-Off Recovery:** Registered `can_set_state_change_callback()` detects bus error transitions (`ERROR_ACTIVE` -> `ERROR_PASSIVE` -> `BUS_OFF`) and triggers automated recovery sequences.
* **Memory Protection Unit Guard:** Configured with `CONFIG_MPU_STACK_GUARD=y` to trap stack overflows at the exact instruction cycle of violation.
* **Priority Inversion Protection:** System-wide telemetry state is locked using `k_mutex` with built-in Priority Inheritance.

---

## Hardware & Pin Configuration

All peripheral assignments are bound at compile-time via Devicetree overlays (`app.overlay`).

| Subsystem | Signal Name | STM32F746 Pin | Alternate Function | Details |
| :--- | :--- | :--- | :--- | :--- |
| **bxCAN1** | CAN1_RX | **PB8** | AF9 (CAN1) | Connected to CAN Transceiver RXD |
| | CAN1_TX | **PB9** | AF9 (CAN1) | Connected to CAN Transceiver TXD |
| | STB (Standby) | **PI0** | GPIO Output | Transceiver Standby Control (Low = Active) |
| **ST-LINK VCP** | UART1_TX | **PA9** | AF7 (USART1) | Virtual COM Port Transmit (115200 bps) |
| | UART1_RX | **PB7** | AF7 (USART1) | Virtual COM Port Receive (115200 bps) |
| **User LED** | LED1 | **PI1** | GPIO Output | Heartbeat Indicator (1 Hz blink) |
| **User Button** | B1 | **PI11** | GPIO Input | Diagnostic Event Trigger |

---

## Vector DBC & Vehicle Signals

The decoding engine translates raw CAN payloads into physical values based on the vehicle communication matrix:

| Message Name | CAN ID | Cycle Time | Signal Name | Start Bit | Length | Scale | Offset | Physical Unit |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **ECM_Telemetry** | `0x100` | 20 ms | `VehicleSpeed` | 0 | 16 | 0.01 | 0.0 | km/h (0 - 250.00) |
| | | | `EngineRPM` | 16 | 16 | 0.25 | 0.0 | RPM (0 - 10000) |
| | | | `CoolantTemp` | 32 | 8 | 1.0 | -40.0 | deg C (-40 to 215) |
| | | | `ThrottlePos` | 40 | 8 | 0.5 | 0.0 | % (0 - 100) |
| | | | `RollingCounter` | 48 | 4 | 1.0 | 0.0 | Counter (0 - 15) |
| | | | `E2E_CRC8` | 56 | 8 | 1.0 | 0.0 | Checksum |
| **BCM_Status** | `0x200` | 100 ms | `FuelLevel` | 0 | 8 | 0.5 | 0.0 | % (0 - 100) |
| | | | `DoorAjar` | 8 | 4 | 1.0 | 0.0 | Bitmask |

---

## AUTOSAR E2E Profile 1 Validation

Safety-critical signals conform to AUTOSAR End-to-End (E2E) Communication Profile 1:

1. **Secret Data ID (16-bit):** `0x1A2B` injected into CRC computation to confirm message origin.
2. **Monotonic Sequence Counter:** Detects repeated, delayed, or lost frames.
3. **CRC-8 Algorithm:**
   - Polynomial: `0x2F` (SAE J1850: `x^8 + x^5 + x^3 + x^2 + x + 1`).
   - Initial Value: `0xFF`, Final XOR: `0xFF`.

```text
Raw CAN Payload: [ Data Bytes 0..6 | CRC Byte 7 ]
                     |
                     v
CRC8_Calculate(Data Bytes 0..6 + DataID_Low + DataID_High) == Payload[7]
```

---

## Hardware CAN Bit Timing & Bus-Off Recovery

Configured for 500 kbps high-speed CAN over APB1 peripheral clock (54 MHz):

```text
f_CAN = 54 MHz
Prescaler = 6 -> Time Quantum (tq) = 6 / 54 MHz = 111.11 ns
Bit Duration = 18 tq:
  - Synchronization Segment (Sync_Seg) = 1 tq
  - Time Segment 1 (Prop_Seg + Phase_Seg1) = 14 tq
  - Time Segment 2 (Phase_Seg2) = 3 tq
Sample Point = (1 + 14) / 18 = 83.3% (Complies with CiA 301 standard)
Bit Rate = 1 / (18 * 111.11 ns) = 500,000 bps (500 kbps)
```

---

## Interactive Zephyr Diagnostic Shell

Connect to the board via USB serial terminal (115200 8-N-1):

```bash
# Display live vehicle telemetry
uart:~$ vehicle status
+---------------------+-------------------+
| Parameter           | Current Value     |
+---------------------+-------------------+
| Vehicle Speed       | 64.50 km/h        |
| Engine Speed        | 2150 RPM          |
| Coolant Temperature | 88 deg C          |
| Throttle Position   | 24.5 %            |
| E2E Sequence Counter| 11                |
| E2E Validation      | PASS              |
| Bus Error Count     | TEC=0, REC=0      |
+---------------------+-------------------+

# Read Diagnostic Trouble Codes
uart:~$ dtc read
Active DTCs (1):
  - [DTC_U0100] Lost Communication With ECM/PCM Engine Control Module

# Clear Diagnostic Trouble Codes
uart:~$ dtc clear
All DTC records cleared. System restored to NORMAL.

# Trigger Hardware Simulation
uart:~$ can sim 80
Injected simulated CAN Frame (ID: 0x100, Speed: 80.00 km/h, CRC: Valid).
```

---

## Project Directory Layout

```text
stm32f7-zephyr-can-gateway/
├── CMakeLists.txt              # Standard Zephyr CMake build instructions
├── prj.conf                    # Static Kconfig configuration file
├── app.overlay                 # Devicetree hardware bindings for STM32F746G-DISCO
├── src/
│   ├── main.c                  # System startup, thread initializations
│   ├── can_gateway.h           # CAN subsystem and k_msgq interfaces
│   ├── can_gateway.c           # bxCAN initialization, filters, Bus-Off callbacks
│   ├── dbc_decoder.h           # Vehicle telemetry data models
│   ├── dbc_decoder.c           # Vector DBC fixed-point parsing and CRC-8 engine
│   ├── safety_monitor.h        # Timeout supervision and DTC manager
│   ├── safety_monitor.c        # Failure recovery logic and DTC storage
│   ├── diag_shell.c            # Zephyr Interactive Shell CLI commands
│   └── autosar_e2e.c           # AUTOSAR E2E Profile 1 CRC implementation
├── tests/                      # Automated unit tests (ztest)
│   └── test_e2e_crc.c
└── README.md
```

---

## Build, Flash & Verify with West

### Prerequisites
* Zephyr SDK v0.16.x or later installed.
* `west` meta-tool configured.

### Commands

```bash
# 1. Initialize and configure environment
cd stm32f7-zephyr-can-gateway

# 2. Build for STM32F746G-Discovery board
west build -b stm32f746g_disco

# 3. Flash to microcontroller via ST-LINK onboard debugger
west flash

# 4. Open Interactive Shell
west attach
```

---

## Author Information

* **Tran Huynh** - Embedded Systems & Firmware Engineer
* **Email:** huynhtran30112004@gmail.com
* **GitHub:** [HuynhTran112](https://github.com/HuynhTran112)
* **LinkedIn:** [Tran Huynh](https://linkedin.com)
