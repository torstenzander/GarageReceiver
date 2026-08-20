# Garage Receiver

This project builds an ESP32-based receiver for a garage door status signal. It listens for LoRa messages from a garage transmitter, updates Apple HomeKit contact sensors, and raises a warning after the garage has been open for more than 10 minutes.

## What it does

- Receives LoRa packets on 868 MHz using an SX1262 module
- Detects garage state changes:
  - `GARAGE_OPEN`
  - `GARAGE_CLOSED`
- Exposes two HomeKit sensors via HomeSpan:
  - Garage door contact sensor
  - Open-warning contact sensor
- Starts a 10-minute warning when the garage stays open too long
- Publishes status updates over the serial console for debugging

## Hardware

This project is designed for the ThinkNode M2 board with an ESP32-S3 and an SX1262 LoRa radio module.

### LoRa pins used

- CS: GPIO 10
- DIO1: GPIO 3
- RST: GPIO 21
- BUSY: GPIO 14
- SCK: GPIO 12
- MOSI: GPIO 11
- MISO: GPIO 13
- RF power enable: GPIO 48

The radio is configured for:

- Frequency: 868.0 MHz
- Bandwidth: 125.0 kHz
- Spreading factor: 9
- Coding rate: 4/5
- Sync word: private
- TX power: 14 dBm
- Preamble length: 8

## Project structure

- `src/main.cpp` - main application logic
- `platformio.ini` - PlatformIO board and library configuration
- `README.md` - project documentation

## Required software

Install the following before building:

1. VS Code
2. PlatformIO extension for VS Code
3. Git (optional, for cloning)

## Setup

### 1. Open the project in VS Code

Open the project folder in VS Code and let PlatformIO detect the project automatically.

### 2. Install project dependencies

PlatformIO will install dependencies from `platformio.ini`, including:

- RadioLib
- HomeSpan

### 3. Check the serial port

The current configuration sets the board serial port to:

- `/dev/cu.wchusbserial110`

On macOS this is usually a USB serial adapter device. If your board uses a different port, update the following lines in `platformio.ini`:

```ini
upload_port = /dev/cu.wchusbserial110
monitor_port = /dev/cu.wchusbserial110
```

### 4. Build the firmware

From the PlatformIO terminal, run:

```bash
pio run -e thinknode-m2
```

### 5. Upload the firmware

```bash
pio run -e thinknode-m2 -t upload
```

### 6. Monitor the device

```bash
pio device monitor
```

or:

```bash
pio run -e thinknode-m2 -t monitor
```

## HomeKit setup

The project uses HomeSpan to expose the garage status to Apple Home.

The code comments note that Wi-Fi credentials and the HomeKit pairing code are stored in the ESP32 NVS memory. On first boot, the device will need to be configured for Wi-Fi and HomeKit pairing as part of the HomeSpan setup flow.

After booting the receiver:

1. Connect the ESP32 to your Wi-Fi network
2. Pair it with HomeKit
3. Add the accessory in the Home app
4. The garage contact sensor will appear as two devices:
   - Garagentor
   - Garagentor Warnung

## Behavior

When a valid LoRa message is received:

- `GARAGE_OPEN` sets the garage sensor to open and starts the 10-minute timer
- `GARAGE_CLOSED` sets the garage sensor to closed and clears the warning state
- If the door remains open for 10 minutes, the second HomeKit sensor is set to open to indicate a warning condition

## Troubleshooting

### Upload fails

- Confirm the correct USB serial port is selected
- Try reconnecting the device
- Ensure the board is in boot mode if required by your ESP32 variant

### No HomeKit devices appear

- Check that the ESP32 has Wi-Fi access
- Verify the HomeSpan setup is running correctly in the serial log
- Confirm that the device has completed the initial pairing flow

### LoRa messages are not received

- Check the LoRa wiring to the SX1262 module
- Verify the sender and receiver use the same frequency and radio settings
- Confirm the message strings match exactly:
  - `GARAGE_OPEN`
  - `GARAGE_CLOSED`

## Notes

This receiver is intended to work with a matching transmitter that sends the same message format and uses the same LoRa parameters. The radio configuration in the code is intentionally aligned with the known working transmitter setup.
