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
const char* mqtt_port = "PORT_OF_MQTT_BROKER";
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
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  
}

void loop() 
{ 
  int xpos = 105;
  int ypos = 90;
  sendRequest(readCO2);
  unsigned long valCO2 = getValue(response);
  Serial.print("Co2 ppm = ");
  Serial.println(valCO2);
  M5.Lcd.clear();
  int co2int = valCO2 & INT_MAX;
  if (valCO2 < 500) { M5.Lcd.setTextColor(TFT_GREEN); }
  if (valCO2 >= 500 & valCO2 < 700) { M5.Lcd.setTextColor(TFT_YELLOW); }
  if (valCO2 >= 700) { M5.Lcd.setTextColor(TFT_RED); }
  M5.Lcd.setCursor(xpos, ypos);
  M5.Lcd.setTextSize(255);
  M5.Lcd.print(valCO2);
  MQTT_connect();
  if (! co2.publish(co2int)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }
  delay(5000);
}

void sendRequest(byte packet[])
{
  while(!s8_Serial.available())  //keep sending request until we start to get a response
  {
    s8_Serial.write(readCO2,7);
    delay(50);
  }
  
  int timeout=0;  //set a timeout counter
  while(s8_Serial.available() < 7 ) //wait to get a 7 byte response
  {
    timeout++;  
    if(timeout > 10)    //if it takes too long there was probably an error
      {
        while(s8_Serial.available())  //flush
          s8_Serial.read();
          
          break;    //exit and try again
      }
      delay(50);
  }
  
  for (int i=0; i < 7; i++)
  {
    response[i] = s8_Serial.read();
  }  
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
         // basically die and wait for WDT to reset
         while (1);
       }
  }

  Serial.println("MQTT Connected!");
}
