# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview
HID Forwarder is a remote control system for USB input devices. It allows inputs generated on a PC to be sent to target systems (like game consoles) via a Raspberry Pi Pico receiver that emulates USB devices.

## Architecture

### System Components
1. **Transmitter** (PC side): Python scripts or web app that capture/generate inputs
2. **Receiver** (Pico/Pico W): Firmware that receives inputs and emulates USB HID devices
3. **Communication**: Three protocols - wired (serial UART), WiFi (UDP), and Bluetooth (RFCOMM)

### Key Files
- `receiver-pico/src/receiver.c`: Main receiver logic, handles networking, Bluetooth, and USB HID emulation
- `receiver-pico/src/bt.c`: Bluetooth RFCOMM implementation
- `receiver-pico/src/descriptors.c`: USB HID descriptors for different device types
- `transmitter-python/*.py`: Python transmitter implementations

## Build Commands

### Receiver Firmware (Pico/Pico W)
```bash
cd receiver-pico
mkdir build
cd build

# For standard Pico (serial only):
cmake ..

# For Pico W (WiFi + Bluetooth + serial):
cmake -DPICO_BOARD=pico_w ..

# For Pico 2:
cmake -DPICO_BOARD=pico2 ..

# For Pico 2 W:
cmake -DPICO_BOARD=pico2_w ..

make
```

The build will generate `receiver.uf2` in the build directory.

## Configuration System

The receiver stores configuration in flash memory at offset `CONFIG_OFFSET_IN_FLASH` (last 16KB of flash). Configuration includes:
- Device type to emulate (`our_descriptor_number`)
- WiFi credentials (`wifi_ssid`, `wifi_password`)
- Bluetooth enable flag (`flags & BLUETOOTH_ENABLED_FLAG_MASK`)

Configuration is managed via USB HID reports through the web configuration tool.

## Network/Bluetooth Initialization

### Current Implementation
- WiFi is always initialized if `NETWORK_ENABLED` is defined (Pico W builds)
- WiFi connects automatically if SSID is configured
- Bluetooth initialization depends on the `BLUETOOTH_ENABLED_FLAG_MASK` flag in config

### Key Functions
- `cyw43_arch_init()`: Initializes the CYW43 chip (WiFi/Bluetooth hardware)
- `cyw43_arch_enable_sta_mode()`: Enables WiFi station mode
- `bt_init()`: Initializes Bluetooth stack and RFCOMM service

## Protocol Details

### Packet Structure
All protocols use the same packet format:
- Protocol version (1 byte)
- Descriptor number (1 byte)
- Data length (1 byte)
- Report ID (1 byte)
- Data (variable length, max 64 bytes)

### Communication Modes
- **Serial**: UART1 at 921600 baud, pins GPIO4 (TX) and GPIO5 (RX)
- **WiFi**: UDP port 42734, no acknowledgment/retransmission
- **Bluetooth**: RFCOMM channel 1, SPP service

## USB Device Types
The receiver can emulate different HID device types selected by `our_descriptor_number`:
0. Mouse
1. Keyboard
2. Switch gamepad
3. PS4 arcade stick
4. XAC/Flex compatible controller

## Testing
No automated tests are provided. Testing is done manually with the transmitter scripts and target devices.