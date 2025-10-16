# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an Arduino project for an M5Stack Core2-based CO2 sensor using a SenseAir S8 sensor with MQTT support. The device reads CO2 levels, displays them on the M5Stack screen with color-coded warnings, and publishes readings to an MQTT broker.

## Architecture

**Single-File Arduino Application**: The entire application is contained in `s8_m5.ino`.

**Hardware Configuration**:
- M5Stack Core2 (ESP32-based device with built-in display)
- SenseAir S8 CO2 sensor connected via software serial (GPIO 13: RX, GPIO 14: TX)
- WiFi connectivity for MQTT publishing

**Key Components**:
- **Sensor Communication**: Uses SoftwareSerial to communicate with S8 sensor via proprietary protocol (7-byte command/response packets)
- **Display Logic**: M5Stack LCD shows CO2 readings with color coding:
  - Green: < 500 ppm
  - Yellow: 500-700 ppm
  - Red: >= 700 ppm
- **MQTT Publishing**: Uses Adafruit MQTT library to publish readings to `esp32/co2` topic every 5 seconds

**Configuration Required** (lines 14-17 in s8_m5.ino):
- `mqtt_server`: IP address of MQTT broker
- `mqtt_port`: Port of MQTT broker
- `ssid`: WiFi network name
- `password`: WiFi password

## Development Commands

**Compilation**: This Arduino sketch must be compiled and uploaded using the Arduino IDE or Arduino CLI with the following libraries installed:
- M5Core2 library
- Adafruit MQTT Library
- SoftwareSerial (built-in)

**Arduino CLI commands** (if using CLI):
```bash
# Compile
arduino-cli compile --fqbn m5stack:esp32:m5stack-core2 s8_m5.ino

# Upload
arduino-cli upload -p /dev/ttyUSB0 --fqbn m5stack:esp32:m5stack-core2 s8_m5.ino
```

**Serial Monitoring**:
```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=9600
```

## Code Structure Notes

**S8 Sensor Protocol**:
- Command packet (line 10): `{0xFE, 0X44, 0X00, 0X08, 0X02, 0X9F, 0X25}` requests CO2 reading
- Response is 7 bytes, with CO2 value encoded in bytes 3-4 (high/low bytes)
- `valMultiplier` (line 12) may need adjustment for different S8 sensor models

**MQTT Connection Handling**:
- Automatic reconnection with 3 retries (lines 100-124)
- System hangs after failed retries to trigger watchdog timer reset

**Main Loop Flow** (lines 39-62):
1. Request CO2 reading from S8 sensor
2. Parse response to get CO2 value
3. Update display with color-coded reading
4. Ensure MQTT connection
5. Publish reading to MQTT broker
6. Wait 5 seconds before next reading
