#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>      // https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi
#include <WebServer.h> // https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer
#include <ESPmDNS.h>   // https://github.com/espressif/arduino-esp32/tree/master/libraries/ESPmDNS
#endif

#include <WiFiUdp.h>    // https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi
#include <WiFiClient.h>  // https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi
#include <HTTPClient.h>  // https://github.com/espressif/arduino-esp32/tree/master/libraries/HTTPClient
#include <NTPClient.h>   //  https://github.com/arduino-libraries/NTPClient
#include <WiFiManager.h> // WifiManager by tzapu  https://github.com/tzapu/WiFiManager
#include <ArduinoOTA.h>  // ArduinoOTA by Arduino, Juraj  https://github.com/JAndrassy/ArduinoOTA
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <Adafruit_NeoPixel.h> // https://github.com/adafruit/Adafruit_NeoPixel

#include "NTP.hpp"
#include "TableEndpoint.hpp"
#if not defined(ESP8266)
#include <rom/rtc.h>
#endif

#define DBG_OUTPUT_PORT Serial
// USBSerial Serial Serial1 Serial2

unsigned long ReloadInterval = 60;
unsigned long LedInterval = 100;
unsigned long LedColor = ((uint32_t)7 << 16) | ((uint32_t)0 << 8) | 0 ;
unsigned long TemperatureInterval = 600;
bool currentLed = false ;
unsigned long PreviousLedInterval = 0;
unsigned long PreviousTemperatureInterval = TemperatureInterval;
unsigned long PreviousReloadInterval = ReloadInterval ;
unsigned long PreviousTimeSeconds;
unsigned long PreviousTimeMinutes;
unsigned long PreviousTimeHours;
unsigned long PreviousTimeDay;
uint16_t Config_Reset_Counter = 0;
int OTAUploadBusy = 0;
JsonDocument jsonDocument;

#if defined(ESP8266)
#define PIN_BOOT 0
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define PIN_BOOT 0
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define PIN_BOOT 9
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#define PIN_BOOT 9
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#define PIN_BOOT 0
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define PIN_BOOT 0
#if not defined(PIN_NEOPIXEL)
#define PIN_NEOPIXEL 48
#endif
#endif

 
#define USEWIFIMANAGER
#ifndef USEWIFIMANAGER
    const char *ssid = "SSID_ROUTER";
const char *password = "Password_Router";
#endif

#define PROJECTNAME "ESP-TABLE"

#if defined(ESP8266)
const char *host = PROJECTNAME"-12E";
#elif defined(CONFIG_IDF_TARGET_ESP32)
const char *host = PROJECTNAME"-ESP32"; 
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
const char *host = PROJECTNAME"-C3";
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
const char *host = PROJECTNAME"-C6";
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
const char *host = PROJECTNAME"-S2";
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
const char *host = PROJECTNAME"-S3";
#endif
String DeviceName = PROJECTNAME;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// WiFiClient Client;
// HTTPClient http;

String urlencode(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (c == ' ')
    {
      encodedString += '+';
    }
    else if (isalnum(c))
    {
      encodedString += c;
    }
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
      {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9)
      {
        code0 = c - 10 + 'A';
      }
      code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
      // encodedString+=code2;
    }
  }
  return encodedString;
}

String urlencode2(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    switch (c)
    {
    case ' ':
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
      {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9)
      {
        code0 = c - 10 + 'A';
      }
      code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
      break;
    default:
      encodedString += c;
      break;
    }
  }
  return encodedString;
}


String Guid()
{
  String ret;
  uint8_t mac[8]; // length: 6 bytes for MAC-48,  8 bytes for EUI-64(used for IEEE 802.15.4)
  mac[6] = random(0xff);
  mac[7] = random(0xff);
  WiFi.macAddress(mac);
  char str[40];
  //1ba7e7dd-8d61-44a8-8a25-ab24d5e17b06
  snprintf(str, sizeof(str), "%02x%02x%02x%02x%02x%02x%02x%02x-%04x-%04x-%04x-%04x%04x%04x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mac[6], mac[7], random(0xffff), random(0xffff), random(0xffff), random(0xffff), random(0xffff), random(0xffff));
  ret = str;
  return ret;
}

void Table_append(String PartitionKey, JsonDocument doc)
{
  String output;
  doc["PartitionKey"] = PartitionKey;
  doc["RowKey"] = Guid();
  serializeJson(doc, output);
  // DBG_OUTPUT_PORT.println(output);
  // DBG_OUTPUT_PORT.println(AZURETABLEENDPOINT);
  if ((WiFi.status() == WL_CONNECTED))
  { // Check the current connection status
    HTTPClient http;  
    http.begin(AZURETABLEENDPOINT); // Specify the URL and certificate
    http.addHeader(F("Accept"), "application/json;odata=nometadata");
    http.addHeader(F("Content-Type"), "application/json");
    int httpCode = http.POST(output); // Make the request
    if (httpCode == 201)
    { // Check for the returning code
      //String payload = http.getString();
      //DBG_OUTPUT_PORT.println(httpCode);
      //DBG_OUTPUT_PORT.println(payload);
    }
    else
    {
      DBG_OUTPUT_PORT.println("Error on Table_append request " + httpCode);
      DBG_OUTPUT_PORT.println(http.getString());
      DBG_OUTPUT_PORT.println(AZURETABLEENDPOINT);
      DBG_OUTPUT_PORT.println(output);

    }
    http.end(); // Free the resources
  }
  else
  {
    DBG_OUTPUT_PORT.println("No Wifi Connection");
  }
}

int GetTableEntry(String Endpoint, String PartitionKey, String Key, String value, String *payload)
{
  int httpCode = -1;
  if ((WiFi.status() == WL_CONNECTED))
  { // Check the current connection status
    HTTPClient http;
    Endpoint += urlencode2("&$filter=PartitionKey eq '" + PartitionKey + "' and " + Key + " eq '" + value + "'&$top=1");
    //DBG_OUTPUT_PORT.println(Endpoint);
    http.begin(Endpoint); // Specify the URL and certificate 
    http.addHeader(F("Accept"), "application/json;odata=nometadata");
    http.addHeader(F("Content-Type"), "application/json");
    httpCode = http.GET(); // Make the request
    if (httpCode == 200)
    { // Check for the returning code
      *payload = http.getString();
      //DBG_OUTPUT_PORT.println(httpCode);
      //DBG_OUTPUT_PORT.println(*payload);
    }
    else
    {
      DBG_OUTPUT_PORT.println("Error on GetTableEntry request " + httpCode);
      DBG_OUTPUT_PORT.println(Endpoint);
      DBG_OUTPUT_PORT.println(http.getString());
    }
    http.end(); // Free the resources
  }
  return httpCode;
}


void get_parameters()
{
  String PartitionKey = "Parameters";
  String payload;
  int httpCode;
  JsonDocument doc;
  // Get the parameter for a specific device 
  httpCode = GetTableEntry(AZURETABLEENDPOINT, PartitionKey, "DeviceName", DeviceName, &payload);
  deserializeJson(doc, payload);
  if (httpCode != 200 || doc["value"].size()==0 ) // will return 200 even if there is no data. 
  { // Is parameter not found get the globale parameter entry
    httpCode = GetTableEntry(AZURETABLEENDPOINT, PartitionKey, "DeviceName", "", &payload);
    doc.clear();
    deserializeJson(doc, payload);
  }
  if( httpCode == 200)
  {
    LedInterval = 100;                if (doc["value"][0]["LedInterval"])         LedInterval         =doc["value"][0]["LedInterval"].as<int>();
    ReloadInterval = 60*60;           if (doc["value"][0]["ReloadInterval"])      ReloadInterval      =doc["value"][0]["ReloadInterval"].as<int>();
    LedColor = pixels.Color(7, 0, 0); if (doc["value"][0]["LedColor"])            LedColor            =doc["value"][0]["LedColor"].as<int>();
    TemperatureInterval = 5*60;       if (doc["value"][0]["TemperatureInterval"]) TemperatureInterval =doc["value"][0]["TemperatureInterval"].as<int>();
    PreviousTemperatureInterval = TemperatureInterval;
    PreviousReloadInterval = ReloadInterval;
    PreviousLedInterval = millis() + LedInterval;
    DBG_OUTPUT_PORT.println("payload " + payload);
    DBG_OUTPUT_PORT.println("LedInterval " + String(LedInterval));
    DBG_OUTPUT_PORT.println("ReloadInterval " + String(ReloadInterval));
    DBG_OUTPUT_PORT.println("LedColor " + String(LedColor));
    DBG_OUTPUT_PORT.println("TemperatureInterval " + String(TemperatureInterval));
  }
}

void LogCoreTemperature()
{
  JsonDocument doc;
  doc.clear();
  doc["DeviceName"] = DeviceName;
#if not defined(ESP8266)
  doc["CoreTemperature"] = String(temperatureRead()); // internal TemperatureSensor
#endif
  Table_append("CoreTemperatures", doc);
  DBG_OUTPUT_PORT.println("LogCoreTemperature " + doc["DeviceName"].as<String>() + " " + doc["CoreTemperature"].as<String>());
}

//*************************************************************************************************************************************************************** */

String reset_reason(int reason)
{
  switch (reason)
  {
  case 1:
    return ("Vbat power on reset");
    break;
  case 3:
    return ("Software reset digital core");
    break;
  case 4:
    return ("Legacy watch dog reset digital core");
    break;
  case 5:
    return ("Deep Sleep reset digital core");
    break;
  case 6:
    return ("Reset by SLC module, reset digital core");
    break;
  case 7:
    return ("Timer Group0 Watch dog reset digital core");
    break;
  case 8:
    return ("Timer Group1 Watch dog reset digital core");
    break;
  case 9:
    return ("RTC Watch dog Reset digital core");
    break;
  case 10:
    return ("Instrusion tested to reset CPU");
    break;
  case 11:
    return ("Time Group reset CPU");
    break;
  case 12:
    return ("Software reset CPU");
    break;
  case 13:
    return ("RTC Watch dog Reset CPU");
    break;
  case 14:
    return ("for APP CPU, reseted by PRO CPU");
    break;
  case 15:
    return ("Reset when the vdd voltage is not stable");
    break;
  case 16:
    return ("RTC Watch dog reset digital core and rtc module");
    break;
  default:
    return ("NO_MEAN");
  }
  return ("NO_MEAN");
}

void setup(void)
{
  pinMode(PIN_BOOT, INPUT_PULLUP);
  pinMode(PIN_NEOPIXEL, OUTPUT);
  pixels.begin();
#if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32)
  DBG_OUTPUT_PORT.begin(115200);
#else
  DBG_OUTPUT_PORT.begin(115200, SERIAL_8N1, RX, TX);
#endif
  DBG_OUTPUT_PORT.setDebugOutput(true);
  DBG_OUTPUT_PORT.println("");

  DeviceName = PROJECTNAME"_" +String(ESP.getChipModel()) + "_" + WiFi.macAddress();

#ifdef USEWIFIMANAGER
  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  //  wm.resetSettings();

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect(DeviceName); // anonymous ap
  // res = wm.autoConnect("AutoConnectAP","password"); // password protected ap
  if (!res)
  {
    DBG_OUTPUT_PORT.println("Failed to connect Restarting");
    delay(5000);
    if (digitalRead(PIN_BOOT) == LOW)
    {
      wm.resetSettings();
    }
    ESP.restart();
  }
  else
  {
    // if you get here you have connected to the WiFi
  }
#else
  WiFi.begin(ssid, password);
  DBG_OUTPUT_PORT.print("Connecting to ");
  DBG_OUTPUT_PORT.println(ssid);
  // Wait for connection
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20)
  { // wait 10 seconds
    delay(500);
  }
  if (i == 21)
  {
    DBG_OUTPUT_PORT.print("Could not connect to");
    DBG_OUTPUT_PORT.println(ssid);
    while (1)
      delay(500);
  }
#endif
  WiFi.setSleep(false);

#if not defined(ESP8266)
  esp_wifi_set_ps(WIFI_PS_NONE); // Esp32 enters the power saving mode by default,
#endif

  DBG_OUTPUT_PORT.print("Connected! IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());
  DBG_OUTPUT_PORT.print("Connecting to ");
  DBG_OUTPUT_PORT.println(WiFi.SSID());

  if (MDNS.begin(host))
  {
    MDNS.addService("http", "tcp", 80);
    DBG_OUTPUT_PORT.println("MDNS responder started");
    DBG_OUTPUT_PORT.print("You can now connect to http://");
    DBG_OUTPUT_PORT.print(host);
    DBG_OUTPUT_PORT.print(".local or http://");
    DBG_OUTPUT_PORT.println(WiFi.localIP());
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname(host);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  ArduinoOTA.onStart([]()
                     {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    OTAUploadBusy=20; // only do a update for 20 sec;
    DBG_OUTPUT_PORT.println("Start updating " + type); });
  ArduinoOTA.onEnd([]()
                   {
      OTAUploadBusy=0;
      DBG_OUTPUT_PORT.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
                          ;  // DBG_OUTPUT_PORT.printf("Progress: %u%%\r", (progress / (total / 100)));
                        });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    OTAUploadBusy=0;
    DBG_OUTPUT_PORT.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      DBG_OUTPUT_PORT.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      DBG_OUTPUT_PORT.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      DBG_OUTPUT_PORT.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      DBG_OUTPUT_PORT.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      DBG_OUTPUT_PORT.println("End Failed");
    } });
  ArduinoOTA.begin();

  timeClient.begin();
  timeClient.setUpdateInterval(1000 * 60 * 60 * 24); // 24 uur
  timeClient.update();

  {
    JsonDocument doc;
    doc.clear();
    doc["DeviceName"]=DeviceName;
#if defined(ESP8266)
    doc["ChipModel"] = ESP.getChipId();
#else
    doc["ChipModel"] = ESP.getChipModel();
#endif
    doc["macAddress"] = WiFi.macAddress();
    doc["LocalIpAddres"]= WiFi.localIP().toString();
    doc["SSID"]= String(WiFi.SSID());
    doc["Rssi"]= String(WiFi.RSSI());

#if not defined(ESP8266)
    doc["Totalheap"]= String(ESP.getHeapSize() / 1024);
    doc["Freeheap"] = String(ESP.getFreeHeap() / 1024);
    doc["TotalPSRAM"] = String(ESP.getPsramSize() / 1024);
    doc["FreePSRAM"] = String(ESP.getFreePsram() / 1024);
    doc["CoreTemperature"] = String(temperatureRead()) ; // internal TemperatureSensor
#if defined(CONFIG_IDF_TARGET_ESP32)
    doc["HallSensor"]= String(hallRead());
#endif
#else
    doc["FlashChipId"] = String(ESP.getFlashChipId());
    doc["FlashChipRealSize"] = String(ESP.getFlashChipRealSize());
#endif
    doc["FlashChipSize"] = String(ESP.getFlashChipSize()/1024/1024);
    doc["FlashChipSpeed"] = String(ESP.getFlashChipSpeed()/1000000);

#if not defined(ESP8266)
#if ESP_ARDUINO_VERSION != ESP_ARDUINO_VERSION_VAL(2, 0, 17)
    String message = "";
    // [ESP::getFlashChipMode crashes on ESP32S3 boards](https://github.com/espressif/arduino-esp32/issues/9816)
    switch (ESP.getFlashChipMode())
    {
    case FM_QIO:
      message = "FM_QIO";
      break;
    case FM_QOUT:
      message = "FM_QOUT";
      break;
    case FM_DIO:
      message = "FM_DIO";
      break;
    case FM_DOUT:
      message = "FM_DOUT";
      break;
    case FM_FAST_READ:
      message = "FM_FAST_READ";
      break;
    case FM_SLOW_READ:
      message = "FM_SLOW_READ";
      break;
    default:
      message = String(ESP.getFlashChipMode());
    }
    doc["FlashChipMode"] = message;
#endif
#endif
    doc["resetReason"] = reset_reason(rtc_get_reset_reason(0));
    doc["ProjectName"] = PROJECTNAME;
    doc["BuildDate"] = String(__DATE__ " " __TIME__);
    Table_append("ESPReboots", doc);
  }
  get_parameters();
}


void loop()
{
unsigned long currentTimeSeconds = timeClient.getEpochTime();
unsigned long Time = millis();
yield();
ArduinoOTA.handle();
if (OTAUploadBusy == 0)
{ // Do not do things that take time when OTA is busy
 
  }

  if (Time >= PreviousLedInterval ) // This will fail after 71 days
  {
    PreviousLedInterval = Time + LedInterval;
    if (currentLed)
    {
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
      pixels.show();
      currentLed = false;
    }
    else
    {
      pixels.setPixelColor(0, LedColor);
      pixels.show();
      currentLed = true;
    }
  }

  if (PreviousTimeDay != (currentTimeSeconds / (60 * 60 * 24)))
  { // Day Loop
    PreviousTimeDay = (currentTimeSeconds / (60 * 60 * 24));
      timeClient.update();
  }
  if (PreviousTimeHours != (currentTimeSeconds / (60 * 60)))
  { // Hours Loop
    PreviousTimeHours = (currentTimeSeconds / (60 * 60));
  }

  if (PreviousTimeMinutes != (currentTimeSeconds / 60))
  { // Minutes loop
    PreviousTimeMinutes = (currentTimeSeconds / 60);
    if ((WiFi.status() != WL_CONNECTED))
    { // if WiFi is down, try reconnecting
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }

  if (PreviousTimeSeconds != currentTimeSeconds)
  { // 1 second loop
    PreviousTimeSeconds = currentTimeSeconds;
    if (PreviousReloadInterval-- == 0)
    {
      PreviousReloadInterval = ReloadInterval;
      get_parameters();
    }
    if (PreviousTemperatureInterval-- == 0)
    {
      PreviousTemperatureInterval = TemperatureInterval;
      LogCoreTemperature();
    }
    if (OTAUploadBusy > 0)
      OTAUploadBusy--;
#ifdef USEWIFIMANAGER
    if (digitalRead(PIN_BOOT) == LOW)
    {
      if (++Config_Reset_Counter > 5)
      {                 // press the BOOT 5 sec to reset the WifiManager Settings
        WiFiManager wm; // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
        delay(500);
        wm.resetSettings();
        ESP.restart();
      }
    }
    else
    {
      Config_Reset_Counter = 0;
    }
#endif
  }
}

  /*
  if (TemperaturesLen >= TemperaturesLenMax)
  {
    int f;
    for (f = 0; f < TemperaturesLenMax - 1; f++)
    {
      Temperatures[f] = Temperatures[f + 1];
      Temperatures_time[f] = Temperatures_time[f + 1];
    }
    TemperaturesLen--;
  }
  sensors.requestTemperatures();
  Temperatures_time[TemperaturesLen] = timeClient.getEpochTime();
  Temperatures[TemperaturesLen++] = sensors.getTempCByIndex(0) + TemperatureOffset;
  */
