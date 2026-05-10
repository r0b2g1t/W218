# PH & ORP Project: W218 Monitoring with WB3S

This repository contains the complete implementation for local integration of the Tuya W218 water-quality monitor (8-in-1) using the ESPHome ecosystem.

## Architecture and Hardware

The W218 monitor uses an internal microcontroller (MCU) to read the sensors' analog values and communicates with the network module via the Tuya V3 serial protocol.


## The Path to a Perfect Handshake (Reverse Engineering)

The biggest challenge in this project was overcoming the W218 communication "deadlock". Below is what was learned during the trial-and-error process:

### What Failed (Frustrating Attempts)

*   **WiFi Status 0x03**: Initially the network module reported status `0x03` (Connected to WiFi), which is common for many Tuya modules. However, the W218 MCU ignored that message and remained waiting, never sending its data packets (Data Points).
*   **Version 0x03 in Header**: Although the MCU identifies itself as version `0x03`, attempting to reply using that same version byte in the header caused the MCU to discard packets.
*   **Passive Synchronization**: Waiting for the MCU to request data or start the handshake proved ineffective, resulting in complete UART silence after a few seconds of boot.

### The Solution (What Worked)

To "unlock" the MCU and make it stream the 8 sensors continuously, the following approaches were implemented:

1.  **The Necessary "Lie" (Status 0x04)**: The secret to unlocking the data flow is reporting status `0x04` (Connected to Cloud). The W218 MCU only releases data if it believes the network module has an established connection to Tuya servers.
2.  **Fallback Header (0x00)**: We found that, regardless of the version the MCU reports, it always accepts responses using version byte `0x00`. This is the most stable communication mode for this specific hardware.
3.  **Proactive Heartbeat**: The WB3S module sends a Heartbeat command (`0x00`) every 10 seconds. This prevents the MCU from entering low-power modes or restarting the network discovery process.
4.  **Time Synchronization (Time Sync)**: The device requires time synchronization (`CMD 0x24`). Responding with the correct time obtained via SNTP stabilizes the transmission of the pH and ORP DPs, which are the most sensitive.

## Firmware and Stability Optimizations

1.  **Socket Management**: Increased the lwIP socket table to 16 (`CONFIG_LWIP_MAX_SOCKETS: 16`). This fixes the critical "Socket Exhaustion" (ENFILE/errno 23) error that previously crashed the device.
2.  **Load Reduction**: The `web_server` component was disabled to prioritize RAM and the stability of the native ESPHome API.
3.  **Latency**: Setting `power_save_mode: none` ensures cryptographic handshakes are processed without delays.

## Data Points Mapping (DP IDs)

| Sensor | ID | Multiplier | Unit |
| :--- | :--- | :--- | :--- |
| pH | 106 | 0.01 | pH |
| ORP | 131 | 1.0 | mV |
| Temperature | 8 / 108 | 0.1 | °C |
| TDS | 126 | 1.0 | ppm |
| EC | 116 | 0.01 | mS/cm |
| Salinity | 121 | 1.0 | ppm |
| CF Factor | 136 | 0.1 | CF |

## License

This project is distributed under the MIT license. See the `LICENSE` file for details.
