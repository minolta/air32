#include <Arduino.h>
#include "SoftwareSerial.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Ticker.h>
#include <ESPAsyncWebServer.h>
#include "Configfile.h"
AsyncWebServer server(80);
Configfile cfg("/cfg.cfg");
#include "SHTSensor.h"
const char *ssid = "forpi";
const char *password = "04qwerty";
#define size 300
#define RXD2 16
#define TXD2 17
String chipid;
unsigned long uptime = 0;
int senddatatime = 0;
int CO2 = 0;
boolean havesht = false;
Ticker tk;
SHTSensor sht;
float tvalue;
float hvalue;
struct CO2_READ_RESULT
{
  int co2 = -1;
  bool success = false;
};

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<style>
#customers {
  font-family: Arial, Helvetica, sans-serif;
  border-collapse: collapse;
  width: 100%%;
}

#customers td, #customers th {
  border: 1px solid #ddd;
  padding: 8px;
}

#customers tr:nth-child(even){background-color: #f2f2f2;}

#customers tr:hover {background-color: #ddd;}

#customers th {
  padding-top: 12px;
  padding-bottom: 12px;
  text-align: left;
  background-color: #04AA6D;
  color: white;
}
</style>
</head>
<body>

<h1>DATA</h1>

%FORINPUTNEWCONFIG%
<table id="customers">
  <tr>
    <th>Paramter</th>
    <th>Value</th>
    <th>Option</th>
  </tr>
  %DATATABLE%
  %CONFIG%
</table>
<script>
function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update?output="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update?output="+element.id+"&state=0", true); }
  xhr.send();
}
function setvalue(config,value){
  console.log('Call setvalue',config,value)
  var xhr = new XMLHttpRequest();
  let inputValue = document.getElementById(config).value; 

  let url ="/setconfig?configname="+config+"&value="+inputValue;
  console.log('Call url',url);
  xhr.open("GET",url);

  xhr.onreadystatechange = function()
    {
        if (xhr.readyState == 4 && xhr.status == 200)
        {
            alert(xhr.responseText); // Another callback here
            console.log(xhr.responseText)
        }
        else if(xhr.status != 200)
        {
          alert('ERROR');
          console.error(xhr.responseText);
        }
    }; 
  xhr.send();
}

 

</script>
</body>
</html>



)rawliteral";
// SoftwareSerial *_SoftSerial_CO2;
void scani2c()
{
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++)
  {

    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");

      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print("Unknow error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}
void listfile()
{
  // LITTLEFS.begin();

  // Dir dir = LittleFS.openDir("/");
}
void status(AsyncWebServerRequest *request)
{
  StaticJsonDocument<size> doc;
  char buf[size];
  doc["t"] = tvalue;
  doc["h"] = hvalue;
  doc["co2"] = CO2;
  doc["uptime"] = uptime;
  serializeJson(doc, buf);
  request->send(200, "application/json", buf);
}
String outputState(int output)
{
  if (digitalRead(output))
  {
    return "checked";
  }
  else
  {
    return "";
  }
}
String processor(const String &var)
{
  // Serial.println(var);
  if (var == "BUTTONPLACEHOLDER")
  {
    String buttons = "";
    buttons += "<h4>Output - GPIO 2</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"2\" " + outputState(2) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Output - GPIO 4</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"4\" " + outputState(4) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Output - GPIO 33</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"33\" " + outputState(33) + "><span class=\"slider\"></span></label>";
    return buttons;
  }
  if (var == "HEAD")
  {
    String buf = "";
    buf += "<tr><th>T</th></tr>";
    buf += "<tr><th>T</th></tr>";
    buf += "<tr><th>CO2</th></tr>";
    return buf;
  }
  if (var == "DATATABLE")
  {
    String buf = "";
    buf += "<tr><td>T</td><td>" + String(tvalue) + "</td><td></td></tr>";
    buf += "<tr><td>H</td><td>" + String(hvalue) + "</td><td></td></tr>";
    buf += "<tr><td>CO2</td><td>" + String(CO2) + "</td><td></td></tr>";
    return buf;
  }
  if (var == "FORINPUTNEWCONFIG")
  {
    String buf = "";

    buf += "<tr><td>New value</td><td></td><td>Configname:<input type=text id=configname  > - Value:<input type=text id=newvalue ><button onclick=\"setvalue( document.getElementById('configname').value; ,'')\">New config</button></td></tr>";
    return buf;
  }
  if (var == "CONFIG")
  {
    String buf = "";
    DynamicJsonDocument doc = cfg.getAll();
    JsonObject documentRoot = doc.as<JsonObject>();
    for (JsonPair keyValue : documentRoot)
    {
      String key = keyValue.key().c_str();
      String value = keyValue.value();
      buf += "<tr><td>" + key + "</td><td>" + value + "</td><td><input type=text id=" + key + " value=\"" + value + "\"><button onclick=\"setvalue('" + key + "','" + value + "')\">Set</button></td></tr>";
    }
    return buf;
  }
  return String();
}
void setconfigwww(AsyncWebServerRequest *request)
{
  cfg.setconfigwww(request);
}
void allconfig(AsyncWebServerRequest *request)
{
  cfg.allconfigwww(request);
}
void setupHttp()
{
  server.on("/web", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", index_html, processor); });
  server.on("/", HTTP_GET, status);
  server.on("/setconfig", HTTP_GET, setconfigwww);
  server.on("/allconfig", HTTP_GET, allconfig);
  server.begin();
}
void uptimefun()
{
  digitalWrite(2, !digitalRead(2));
  uptime++;
  senddatatime++;
}
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
    Serial.printf("CO2 value:%d chipid %s Core id %d\n", CO2, chipid, ESP.getChipCores());
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}
void readSht(void *p)
{
  while (1)
  {
    hvalue = sht.getHumidity();
    tvalue = sht.getTemperature();
    // Serial.printf("CO2 value:%d chipid %s Core id %d\n", CO2, chipid, ESP.getChipCores());
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
    String POSTURL = cfg.getConfig("senddataurl", "http://192.168.88.225:8080/sensors/airgradient:") + chipid + "/measures";
    // Serial.println(POSTURL);
    doc.clear();
    doc["co2"] = CO2;
    doc["t"] = tvalue;
    doc["h"] = hvalue;
    JsonObject pi = doc.createNestedObject("pidevice");
    pi["mac"] = chipid;

    serializeJson(doc, buf);
    Serial.printf("Start send data %s core id:%d \n", buf, ESP.getChipCores());
    http.begin(client, POSTURL);
    http.addHeader("content-type", "application/json");
    int httpCode = http.POST(buf);
    http.end();
  }
  vTaskDelete(NULL);
}
void setSHT()
{
  Wire.begin(21, 22);
  havesht = false;
  if (sht.init())
  {
    havesht = true;
    Serial.print("init(): success\n");
  }
  else
  {
    Serial.print("init(): failed\n");
  }
  sht.setAccuracy(SHTSensor::SHT_ACCURACY_HIGH); // only supported by SHT3x
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
  cfg.setbuffer(512);
  cfg.openFile();
  cfg.load();
  wificonnect();
  setupHttp();
  setSHT();
  tk.attach(1, uptimefun);
  chipid = getChipid();
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  xTaskCreate(readCo2, "readco2", 2000, NULL, 1, NULL);
  if (havesht)
    xTaskCreate(readSht, "readsht", 2000, NULL, 1, NULL);
  pinMode(2, OUTPUT);
}
void loop()
{

  if (Serial.available())
  {
    char key = Serial.read();
    if (key == 'i')
      scani2c();
  }

  if (senddatatime > 15)
  {
    senddatatime = 0;
    xTaskCreate(senddata, "senddata", 5000, NULL, 1, NULL);
  }
}
// senddata(NULL);
