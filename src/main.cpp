#include <Arduino.h>
#include "SoftwareSerial.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
const char *ssid = "forpi";
const char *password = "04qwerty";
#define RXD2 16
#define TXD2 17
String chipid;
int CO2 = 0;
struct CO2_READ_RESULT
{
  int co2 = -1;
  bool success = false;
};
// SoftwareSerial *_SoftSerial_CO2;
int readCo()
{
  int retry = 0;
  CO2_READ_RESULT result;
  const byte CO2Command[] = {0xFE, 0X44, 0X00, 0X08, 0X02, 0X9F, 0X25};
  byte CO2Response[] = {0, 0, 0, 0, 0, 0, 0};

  while (!(Serial2.available()))
  {
    retry++;
    // keep sending request until we start to get a response
    Serial2.write(CO2Command, 7);
    delay(50);
    if (retry > 40)
    {
      Serial.println("Wait response time out");
      return -1;
    }
  }

  int timeout = 0;

  while (Serial2.available() < 7)
  {
    timeout++;
    if (timeout > 10)
    {
      while (Serial2.available())
        Serial2.read();
      break;
    }
    delay(100);
  }

  for (int i = 0; i < 7; i++)
  {
    int byte = Serial2.read();
    if (byte == -1)
    {
      result.success = false;
      Serial.println("Response not complate 7 Byte");
      return -2;
    }
    CO2Response[i] = byte;
  }
  int valMultiplier = 1;
  int high = CO2Response[3];
  int low = CO2Response[4];
  unsigned long val = high * 256 + low;

  return val;
}
void readCo2(void *p)
{
  while (1)
  {
    CO2 = readCo();
    Serial.printf("CO2 value:%d chipid %s Core id %d\n",CO2,chipid,ESP.getChipCores());
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}
String getChipid()
{
  uint64_t id = ESP.getEfuseMac();

  Serial.printf("%X\n", id);
  unsigned long long1 = (unsigned long)((id & 0xFFFF0000) >> 16);
  unsigned long long2 = (unsigned long)((id & 0x0000FFFF));
  String hex = String(long1, HEX) + String(long2, HEX); // six octets
  return hex;
}
void senddata(void *p)
{
  if (WiFi.isConnected())
  {
    HTTPClient http;
    WiFiClient client;
    StaticJsonDocument<300> doc;
    char buf[300];
    String POSTURL = "http://192.168.88.225:8080/sensors/airgradient:" + chipid + "/measures";
    // Serial.println(POSTURL);
    doc.clear();
    doc["co2"] = CO2;
    JsonObject  pi  = doc.createNestedObject("pidevice");
    pi["mac"]=chipid;
    serializeJson(doc, buf);
    Serial.printf("Start send data %s core id:%d \n", buf, ESP.getChipCores());
    http.begin(client, POSTURL);
    http.addHeader("content-type", "application/json");
    int httpCode = http.POST(buf);
    http.end();
  }
  vTaskDelete(NULL);
}
void wificonnect()
{
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");
}
void setup()
{
  // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(115200);
  wificonnect();
  chipid = getChipid();
  // _SoftSerial_CO2->begin(9600, RXD2, TXD2);
  // _SoftSerial_CO2 = new SoftwareSerial(TXD2, RXD2);
  // _SoftSerial_CO2->begin(9600);
  // Serial1.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  // Serial.println("Serial Txd is on pin: " + String(TX));
  // Serial.println("Serial Rxd is on pin: " + String(RX));
  xTaskCreate(readCo2, "readco2", 2000, NULL, 1, NULL);
}
void loop()
{
  // Choose Serial1 or Serial2 as required

  delay(15000);
  xTaskCreate(senddata, "senddata", 5000, NULL, 1, NULL);
}
// senddata(NULL);
