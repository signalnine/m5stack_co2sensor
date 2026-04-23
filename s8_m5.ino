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
  M5.Lcd.clear();
  M5.Lcd.setCursor(xpos, ypos);
  M5.Lcd.setTextSize(255);

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
  int co2int = (int)valCO2;
  if (valCO2 < 500) { M5.Lcd.setTextColor(TFT_GREEN); }
  if (valCO2 >= 500 && valCO2 < 700) { M5.Lcd.setTextColor(TFT_YELLOW); }
  if (valCO2 >= 700) { M5.Lcd.setTextColor(TFT_RED); }
  M5.Lcd.print(valCO2);
  MQTT_connect();
  if (! co2.publish(co2int)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }
  delay(5000);
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

  return true;
}

unsigned long getValue(byte packet[])
{
    int high = packet[3];   //high byte for value is 4th byte in packet in the packet
    int low = packet[4];    //low byte for value is 5th byte in the packet
    unsigned long val = high*256 + low;  //Combine high byte and low byte with this formula to get value
    return val* valMultiplier;
}

void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         Serial.println("MQTT connection failed after 3 retries. Continuing without MQTT...");
         return;  // Give up and continue without MQTT
       }
  }

  Serial.println("MQTT Connected!");
}
