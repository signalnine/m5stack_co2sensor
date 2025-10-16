# M5Stack CO2 Sensor

M5Stack Core2 based CO2 sensor using a SenseAir S8 with MQTT support.

## Overview

This project implements a CO2 monitoring system using an M5Stack Core2 and a SenseAir S8 CO2 sensor. The device continuously measures CO2 levels, displays them on the built-in screen with color-coded warnings, and publishes readings to an MQTT broker for remote monitoring.

## Hardware Requirements

- **M5Stack Core2**: ESP32-based device with 320x240 touchscreen display
- **SenseAir S8 CO2 Sensor**: NDIR CO2 sensor (0-10,000 ppm range)
- **Wiring**:
  - S8 RX → M5Stack GPIO 13
  - S8 TX → M5Stack GPIO 14
  - S8 VCC → 5V
  - S8 GND → GND

## Software Dependencies

Install these libraries via Arduino Library Manager:

1. **M5Core2** (by M5Stack)
2. **Adafruit MQTT Library** (by Adafruit)
3. **SoftwareSerial** (built-in to Arduino/ESP32)

## Configuration

Before uploading, modify these values in `s8_m5.ino`:

```cpp
const char* mqtt_server = "192.168.1.100";  // Your MQTT broker IP
const int mqtt_port = 1883;                  // Your MQTT broker port
const char* ssid = "YourWiFiName";           // Your WiFi SSID
const char* password = "YourWiFiPassword";   // Your WiFi password
```

Optional configuration:
```cpp
int valMultiplier = 1;  // Set to different value for alternate S8 models
#define FEED "esp32/co2"  // MQTT topic to publish to
```

## Installation

### Using Arduino IDE

1. Install Arduino IDE 2.0 or later
2. Add M5Stack board support:
   - Go to File → Preferences
   - Add to "Additional Board Manager URLs":
     `https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json`
3. Install board: Tools → Board Manager → Search "M5Stack" → Install
4. Install required libraries (see Software Dependencies)
5. Select board: Tools → Board → M5Stack Arduino → M5Stack-Core2
6. Select port: Tools → Port → (your device port)
7. Upload the sketch

### Using Arduino CLI

```bash
# Configure board support
arduino-cli config init
arduino-cli core update-index
arduino-cli core install m5stack:esp32

# Install libraries
arduino-cli lib install "M5Core2"
arduino-cli lib install "Adafruit MQTT Library"

# Compile
arduino-cli compile --fqbn m5stack:esp32:m5stack-core2 s8_m5.ino

# Upload (replace /dev/ttyUSB0 with your port)
arduino-cli upload -p /dev/ttyUSB0 --fqbn m5stack:esp32:m5stack-core2 s8_m5.ino

# Monitor serial output
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=9600
```

## Operation

### Startup Sequence

1. Device initializes M5Stack hardware and display
2. Connects to WiFi (displays connection progress on serial)
3. Begins main loop: reading CO2, updating display, publishing to MQTT

### Display

The CO2 reading is shown in large text with color coding:
- **Green** (< 500 ppm): Good air quality
- **Yellow** (500-700 ppm): Moderate air quality
- **Red** (≥ 700 ppm): Poor air quality, ventilation recommended

### MQTT Publishing

- **Topic**: `esp32/co2` (configurable)
- **Format**: Integer value (CO2 ppm)
- **Frequency**: Every 5 seconds
- **Connection**: Automatic reconnection with 3 retries

### Serial Output

Connect at 9600 baud to see:
- WiFi connection status and IP address
- CO2 readings every 5 seconds
- MQTT connection status and publish results

## SenseAir S8 Protocol

The S8 uses a proprietary serial protocol:

### Command Packet (Read CO2)
```
0xFE 0x44 0x00 0x08 0x02 0x9F 0x25
```

### Response Packet (7 bytes)
```
Byte 0: 0xFE (Header)
Byte 1: 0x44 (Command)
Byte 2: 0x00
Byte 3: CO2 High Byte
Byte 4: CO2 Low Byte
Byte 5: Checksum High
Byte 6: Checksum Low
```

CO2 value calculation: `(Byte3 × 256) + Byte4`

## Troubleshooting

### WiFi Connection Issues
- Verify SSID and password are correct
- Check WiFi signal strength
- Ensure WiFi is 2.4GHz (ESP32 doesn't support 5GHz)

### Sensor Not Reading
- Check wiring connections
- Verify S8 has power (LED should be on)
- Check GPIO pins 13 and 14 are not used by other hardware
- Allow 30 seconds for sensor warm-up

### MQTT Connection Failures
- Verify MQTT broker is running and accessible
- Check broker IP address and port
- Ensure no firewall blocking port 1883
- Check broker logs for connection attempts

### Display Issues
- If text size is wrong, check `setTextSize()` value
- Color thresholds can be adjusted in the display logic

## Maintenance

### Sensor Calibration

The S8 sensor has automatic baseline calibration (ABC) enabled by default. For accurate readings:
- Expose sensor to fresh outdoor air (≈400 ppm) regularly
- ABC assumes sensor sees 400 ppm at least once every 7 days
- For manual calibration, refer to S8 datasheet

### Performance

- **Update rate**: 5 seconds (adjustable via delay in loop)
- **S8 measurement time**: ~2 seconds per reading
- **WiFi reconnection**: Automatic if connection drops
- **MQTT reconnection**: Automatic with 3 retry attempts

## Advanced Modifications

### Changing Update Frequency
Modify the delay at the end of `loop()`:
```cpp
delay(5000);  // Change to desired milliseconds
```

### Adding Data Logging
The MQTT feed can be subscribed to by:
- Home Assistant
- Node-RED
- InfluxDB + Grafana
- Any MQTT-compatible system

### Multiple Sensors
To publish multiple sensors to MQTT, add additional feed definitions:
```cpp
Adafruit_MQTT_Publish temperature = Adafruit_MQTT_Publish(&mqtt, "esp32/temperature");
```

## References

- [SenseAir S8 Datasheet](https://senseair.com/products/size-counts/s8-lp/)
- [M5Stack Core2 Documentation](https://docs.m5stack.com/en/core/core2)
- [Adafruit MQTT Library](https://github.com/adafruit/Adafruit_MQTT_Library)

## License

See repository LICENSE file for details.
