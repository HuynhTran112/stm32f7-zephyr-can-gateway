# Automotive CAN Gateway & Telematics Cluster (Zephyr RTOS)

[![Zephyr Version](https://img.shields.io/badge/Zephyr--RTOS-v3.7.0-blue.svg)](https://zephyrproject.org/)
[![Hardware](https://img.shields.io/badge/Hardware-STM32F746G--DISCO-red.svg)](https://www.st.com/en/evaluation-tools/32f746gdiscovery.html)
[![Bus Protocol](https://img.shields.io/badge/Bus-CAN%202.0B%20(500kbps)-green.svg)](#-requirements)
[![Safety Standard](https://img.shields.io/badge/Standard-AUTOSAR%20E2E%20Profile%201-orange.svg)](#-system-protection)
[![License](https://img.shields.io/badge/License-MIT-lightgrey.svg)](LICENSE)

An automotive telematics gateway and diagnostic cluster built on **Zephyr RTOS** for the **STM32F746G-Discovery** board (ARM Cortex-M7 @ 216 MHz). The system features hardware-filtered asynchronous CAN ingestion, a fixed-point **Vector DBC signal decoding engine**, **AUTOSAR E2E Profile 1** data integrity checking, automated ISO 11898-1 Bus-Off recovery, and a real-time **Zephyr Shell CLI** for interactive telemetry and diagnostics.

---

## 📑 Table of Contents

- [Demo](#-demo)
- [Key Features](#-key-features)
- [Requirements](#️-requirements)
- [Hardware Connections](#-hardware-connections)
- [Getting Started](#-getting-started)
- [System Behavior & Workflow](#-system-behavior--workflow)
- [Project Structure](#️-project-structure)
- [System Protection](#️-system-protection)
- [Author Information](#-author-information)

---

## 📷 Demo

<p align="center">
  <img src="docs/images/can_gateway_hero.png" alt="CAN Gateway Demo" width="650">
</p>

```text
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
```

---

## 📌 Key Features

* **Asynchronous Zero-Copy Ingestion:** Uses Zephyr's `can_add_rx_filter_msgq()` to route hardware-filtered CAN frames directly into kernel message queues (`k_msgq`), handling 1000+ frames/sec without CPU polling.
* **Vector DBC Signal Extraction:** Decodes raw CAN payloads into engineering values (Vehicle Speed, RPM, Coolant Temp, Throttle Position, Fuel Level) using fixed-point integer arithmetic.
* **AUTOSAR E2E Profile 1 Validation:** Protects safety-critical messages with a 3-layer check: 16-bit secret Data ID, 4-bit monotonic rolling counter, and CRC-8 (SAE J1850 polynomial 0x2F).
* **Interactive Diagnostic Shell (CLI):** Full Zephyr Shell accessible over USB ST-LINK VCP UART with commands to view live telemetry (`vehicle status`), read and clear DTCs (`dtc read`, `dtc clear`), and inject simulated traffic (`can sim`).
* **Automated Bus-Off Recovery:** Implements ISO 11898-1 state monitoring via `can_set_state_change_callback()` to automatically restart communication after severe bus disturbances.
* **Hardware MPU Guard:** Uses `CONFIG_MPU_STACK_GUARD=y` to immediately catch stack overflow at the hardware boundary.

---

## ⚙️ Requirements

* **Toolchain & SDK:** Zephyr SDK (v0.16.x or newer), West CLI, CMake, Ninja
* **Hardware Components:**
  * **STM32F746G-Discovery Board:** ARM Cortex-M7 @ 216 MHz, ST-LINK V2-1 on-board.
  * **CAN Transceiver Module:** TJA1050, SN65HVD230, or VP230 (3.3V / 5V).
  * **USB Cables:** Mini-USB for ST-LINK programming/shell, Micro-USB for power/secondary interface.
  * **120-Ohm Termination Resistors:** Standard automotive bus termination on CAN_H / CAN_L.

---

## 🔌 Hardware Connections

### Pinout Table

| Peripheral | Pin Name | STM32F746 Pin | Connection Type & Notes |
| :--- | :--- | :--- | :--- |
| **bxCAN1** | CAN_RX | **PB8** | Alternate Function 9 (Connect to Transceiver RXD) |
| | CAN_TX | **PB9** | Alternate Function 9 (Connect to Transceiver TXD) |
| | STB | **PI0** | GPIO Output (Transceiver Standby control, LOW = Active) |
| **ST-LINK VCP** | UART_TX | **PA9** | USART1 TX (USB Virtual COM Port @ 115200 baud) |
| | UART_RX | **PB7** | USART1 RX (USB Virtual COM Port @ 115200 baud) |
| **Status LED** | LED1 | **PI1** | GPIO Output (Heartbeat indicator, toggles at 1 Hz) |
| **User Button**| B1 | **PI11** | GPIO Input (Triggers diagnostic test event) |

---

## 🚀 Getting Started

### 1. Set up Zephyr SDK & West

Follow the official [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) to install `west` and the toolchain.

### 2. Clone this repository

```bash
git clone https://github.com/HuynhTran112/stm32f7-zephyr-can-gateway.git
cd stm32f7-zephyr-can-gateway
```

### 3. Build firmware

```bash
west build -b stm32f746g_disco
```

### 4. Flash to board

```bash
west flash
```

### 5. Open Diagnostic Shell

Connect to the ST-LINK Virtual COM port at **115200 8-N-1** using your favorite terminal (PuTTY, Tera Term, Minicom) or via west:

```bash
west attach
```

Once connected, press Enter to get the `uart:~$` prompt and run:
```bash
vehicle status     # View live vehicle dashboard
dtc read           # Read active trouble codes
can sim 80         # Simulate vehicle running at 80 km/h
```

---

## 🔄 System Behavior & Workflow

The firmware coordinates three cooperating threads under Zephyr's preemptive scheduler:

```mermaid
flowchart TD
    CANBus[CAN Bus Traffic 500kbps] -->|Hardware Filter| CANISR[bxCAN1 Hardware RX]
    CANISR -->|can_add_rx_filter_msgq| Queue[(k_msgq Ring Buffer)]

    subgraph T1["Thread 1: CAN Worker (Priority 2)"]
        Queue --> Pull[Lấy CAN Frame từ Queue]
        Pull --> DBC[Vector DBC Decoder]
        DBC --> E2E{Kiểm tra AUTOSAR E2E CRC8?}
        E2E -- Hợp lệ --> UpdateState[Cập nhật Vehicle State - k_mutex]
        E2E -- Lỗi --> FlagError[Ghi nhận lỗi E2E Counter / CRC]
    end

    subgraph T2["Thread 2: Safety Supervisor (Period 100ms)"]
        UpdateState --> CheckTimeout{Có mất tín hiệu > 500ms?}
        CheckTimeout -- Có --> SetDTC[Lưu mã lỗi DTC_U0100]
        CheckTimeout -- Không --> Heartbeat[Blink LED1 Heartbeat]
    end

    subgraph T3["Thread 3: Zephyr Shell CLI (Priority 10)"]
        UserCmd[Lệnh từ người dùng: vehicle status / dtc] --> ReadState[Đọc Vehicle State qua k_mutex]
        ReadState --> PrintTable[In bảng thông số thời gian thực]
    end
```

---

## 🗂️ Project Structure

```text
stm32f7-zephyr-can-gateway/
├── CMakeLists.txt              # Top-level Zephyr CMake build configuration
├── prj.conf                    # Static Kconfig settings (CAN, Shell, MPU, Mutex)
├── app.overlay                 # Devicetree hardware pinmux bindings for STM32F746G-DISCO
├── src/
│   ├── main.c                  # System entry point, thread setup & heartbeat
│   ├── can_gateway.h           # CAN subsystem & k_msgq interfaces
│   ├── can_gateway.c           # CAN controller init, filters & Bus-Off callback
│   ├── dbc_decoder.h           # Vehicle telemetry data structure definitions
│   ├── dbc_decoder.c           # Fixed-point DBC signal parsing & E2E algorithms
│   ├── safety_monitor.h        # Safety supervisor & DTC definitions
│   ├── safety_monitor.c        # Frame timeout detection & fault logger
│   └── diag_shell.c            # Zephyr Interactive Shell command handlers
└── README.md
```

---

## 🛡️ System Protection

* **AUTOSAR E2E Profile 1:** Validates data authenticity on every critical message using a 16-bit secret Data ID, 4-bit sequence counter, and CRC-8.
* **ISO 11898-1 Bus-Off State Machine:** Automatically detects bus-off conditions and initiates bus recovery without requiring a system reset.
* **Hardware MPU Stack Guard:** ARM Cortex-M7 Memory Protection Unit traps stack overflows at the exact instruction of violation (`CONFIG_MPU_STACK_GUARD=y`).
* **Priority Inheritance Mutexes:** Shared telemetry data is guarded by `k_mutex` to prevent priority inversion between the high-priority CAN worker and the low-priority Shell thread.

---

## 👥 Author Information

* **Author:** Trần Huỳnh
* **Major:** Computer Engineering Technology
* **Faculty:** Faculty of Electrical and Electronics Engineering (FEEE)
* **Institution:** Ho Chi Minh City University of Technology and Education (HCMUTE)
* **Email:** [huynhtran30112004@gmail.com](mailto:huynhtran30112004@gmail.com)
* **GitHub:** [HuynhTran112](https://github.com/HuynhTran112)
