#include "SoftwareSerial.h"
#include <M5Core2.h>
#include <WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"


SoftwareSerial s8_Serial(13,14); //connect rx & tx from s8 to GPIO pins 13 & 14

byte readCO2[] = {0xFE, 0X44, 0X00, 0X08, 0X02, 0X9F, 0X25};  //Command packet to read Co2
byte response[] = {0,0,0,0,0,0,0};
int valMultiplier = 1; //different models of sensor need other values here

const char* mqtt_server = "IP_OF_MQTT_BROKER";
const int mqtt_port = 1883;
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, mqtt_server, mqtt_port);
#define FEED "esp32/co2"
Adafruit_MQTT_Publish co2 = Adafruit_MQTT_Publish(&mqtt, FEED);

void setup() 
{
  M5.begin();
  Serial.begin(9600);
  s8_Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  int wifi_timeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_timeout < 30) {
    Serial.print('.');
    delay(1000);
    wifi_timeout++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("WiFi connection failed! Continuing anyway...");
  }
  
}

void loop()
{
  int xpos = 105;
  int ypos = 90;
  bool ok = sendRequest(readCO2);
  // Clear only the value band instead of the whole 320x240 framebuffer to
  // avoid the visible black flash on every loop iteration.
  M5.Lcd.fillRect(0, ypos, 320, 60, TFT_BLACK);
  M5.Lcd.setCursor(xpos, ypos);
  M5.Lcd.setTextSize(7);

  if (!ok) {
    // Don't render a stale/zero value in green -- that would look like
    // "excellent air" when the sensor is actually unreachable. Show an
    // error instead and skip the MQTT publish so consumers don't record
    // a false zero reading.
    Serial.println("Co2 read failed, skipping publish");
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.print("ERR");
    delay(5000);
    return;
  }

  unsigned long valCO2 = getValue(response);
  Serial.print("Co2 ppm = ");
  Serial.println(valCO2);
  if (valCO2 < 500) { M5.Lcd.setTextColor(TFT_GREEN); }
  if (valCO2 >= 500 && valCO2 < 700) { M5.Lcd.setTextColor(TFT_YELLOW); }
  if (valCO2 >= 700) { M5.Lcd.setTextColor(TFT_RED); }
  M5.Lcd.print(valCO2);

  // Skip MQTT entirely if WiFi is down. Otherwise mqtt.connect() drives
  // a TCP attempt through a dead stack and blocks for its full timeout
  // every iteration, starving the sensor-read cadence.
  if (ensureWiFi()) {
    MQTT_connect();
    // Publish as uint32_t so non-default valMultiplier values can't push
    // the reading past INT_MAX and produce a negative ppm on the wire.
    if (! co2.publish((uint32_t)valCO2)) {
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("OK!"));
    }
  }
  delay(5000);
}

// Re-establish WiFi if it has dropped since setup(). Without this the
// device silently becomes useless after any router reboot or signal
// glitch -- MQTT keeps failing but WiFi is never restarted.
bool ensureWiFi()
{
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println("WiFi disconnected, attempting reconnect...");
  WiFi.reconnect();

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 10) {
    delay(1000);
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi reconnected: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WiFi reconnect failed, skipping MQTT this cycle");
  return false;
}

bool sendRequest(byte packet[])
{
  // Drain any stale bytes left over from a previous exchange so we don't
  // short-circuit the write/read loops below and read a previous response.
  while (s8_Serial.available())
    s8_Serial.read();

  int writeAttempts = 0;
  while(!s8_Serial.available())  //keep sending request until we start to get a response
  {
    s8_Serial.write(packet,7);
    delay(50);
    writeAttempts++;
    if (writeAttempts > 20) {  // ~1s with no reply from the sensor
      Serial.println("S8 sensor not responding");
      return false;
    }
  }

  int timeout=0;  //set a timeout counter
  while(s8_Serial.available() < 7 ) //wait to get a 7 byte response
  {
    timeout++;
    if(timeout > 10)    //if it takes too long there was probably an error
      {
        while(s8_Serial.available())  //flush
          s8_Serial.read();

        Serial.println("S8 response timeout");
        return false;
      }
      delay(50);
  }

  for (int i=0; i < 7; i++)
  {
    response[i] = s8_Serial.read();
  }

  // Validate response packet
  if (response[0] != 0xFE || response[1] != 0x44) {
    Serial.println("Invalid S8 response packet!");
    return false;
  }

  // Verify Modbus CRC16 over bytes 0..4; bytes 5,6 are CRC low,high.
  // Without this, serial noise can flip the CO2 bytes while leaving
  // the header intact and we would report a bogus reading.
  uint16_t expectedCrc = modbusCRC16(response, 5);
  uint16_t receivedCrc = (uint16_t)response[5] | ((uint16_t)response[6] << 8);
  if (expectedCrc != receivedCrc) {
    Serial.println("S8 response CRC mismatch");
    return false;
  }

  return true;
}

uint16_t modbusCRC16(const byte *data, size_t len)
{
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

unsigned long getValue(byte packet[])
{
    int high = packet[3];   //high byte for value is 4th byte in packet in the packet
    int low = packet[4];    //low byte for value is 5th byte in the packet
    unsigned long val = high*256 + low;  //Combine high byte and low byte with this formula to get value
    return val* valMultiplier;
}

void MQTT_connect() {
  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  // Single attempt per loop iteration. The previous 3-retry loop with a
  // delay(5000) between attempts could block this task for up to 15s when
  // the broker was unreachable, on top of the trailing 5s loop delay --
  // starving the sensor read cadence. The outer loop already runs every
  // 5s, so let it provide the retry cadence instead.
  int8_t ret = mqtt.connect();
  if (ret != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("MQTT connect failed; will retry next loop");
    mqtt.disconnect();
    return;
  }

  Serial.println("MQTT Connected!");
}
