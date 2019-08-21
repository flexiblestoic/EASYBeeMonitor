/***************************************************
  EasyBeeSensors
  v0.1
 ****************************************************/

// Libraries
#include <ESP8266WiFi.h>
#include <HX711.h>             //weight sensor board
#include <OneWire.h>           // support for DS18B20
#include <DallasTemperature.h> // support for DS18B20
#include <SoftwareSerial.h>
#include "conversions.h"
#include "credentials.h"

ADC_MODE(ADC_VCC); //Allow getVcc

/*
Pin map:
D0: connected to RST to be able to wake from deep deepSleep
D1/GPIO5: HX711 DOUT, DT
D2/GPIO4: HX711 CLK, SCK
D3/GPIO0: temperature sensor 1 (int)
D4/GPIO2: temperature sensor 2 (ext)
D5/GPIO14: Sigfox, WISOL, TX
D6/GPIO12: Sigfox, WISOL, RX
D7/GPIO13: calibration mode
D8/GPIO15: do not use during config (must be pulled or upload error)

*/
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define D9 3
#define D10 1

// Network selection
#define WIFIACTIVE 0
#define SIGFOXACTIVE 1
#define CALIBRATIONACTIVE 0

#define WIFISLEEPTIME 10e6                          // seconds e6
#define SIGFOXSLEEPTIME 15 * 60e6, WAKE_RF_DISABLED //in seconds e6, deactivates wifi for energy savings

// Weight sensor parameters
#define DOUT D1
#define CLK D2
float calibration_ratio = -21757.0;
float calibration_offset = -446492.0;
HX711 scale(DOUT, CLK);

// Temperature sensors parameters one is internal temperature, 2 is external temperature
#define ONE_WIRE_BUS1 D3
#define ONE_WIRE_BUS2 D4
// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire1(ONE_WIRE_BUS1);
OneWire oneWire2(ONE_WIRE_BUS2);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensorInt(&oneWire1);
DallasTemperature sensorExt(&oneWire2);

// Sigfox parameters (WISOL)
#define rxPin D5
#define txPin D6
SoftwareSerial Sigfox = SoftwareSerial(rxPin, txPin);

// setup for calibration mode using flash button
#define calibrationBtn D7

/* credentials.h format
// WiFi parameters
#define WLAN_SSID "XXXX"
#define WLAN_PASS "XXXX"

// ThingSpeak Settings
const int channelID = XXXX;
String writeAPIKey = "XXXXXXX"; // write API key for your ThingSpeak Channel
const char *thingSpeakServer = "api.thingspeak.com";
*/

// Create an ESP8266 WiFiClient class
WiFiClient client;

// Global vars

float temperature_data;
float weight_data;

typedef struct __attribute__((packed)) messages
{
  uint16_t weight;    // 2 bytes
  uint16_t tempInt;   // 2 bytes
  uint16_t tempExt;   // 2 bytes
  uint8_t espVoltage; // 1 byte

  /*
 * Sigfox custom payload config
 * weight::uint:16:little-endian tempInt::int:16:little-endian tempExt::int:16:little-endian espVoltage::uint:8 
 */

} Message;

Message msg;
Message msgEmpty;

/*************************************************************************************************/

void setup()
{

  Serial.begin(9600);
  Serial.println("In setup");

  //start scale
  scale.power_up();
  scale.begin(DOUT, CLK);
  scale.set_offset(calibration_offset);
  scale.set_scale(calibration_ratio);

  //start temperature sensors
  sensorInt.begin();
  sensorExt.begin();

  //Sigfox
  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);

  Sigfox.begin(9600);

  // Calibration mode
  pinMode(calibrationBtn, INPUT_PULLUP);
  bool calibrating = !digitalRead(calibrationBtn);
  //bool calibrating = CALIBRATIONACTIVE;
  Serial.print("calibrationBtn: ");
  Serial.print(digitalRead(calibrationBtn));
  
  if (calibrating)
  {
    Serial.println("Calibrating");

    while (true)
    {
      // forever loop. Exits only upon reset

      scale.set_offset(0);
      scale.set_scale(1);

      Serial.print("Weight: ");
      Serial.print(scale.get_value(5), 1);
      Serial.print(" units;");

      scale.set_offset(calibration_offset);
      scale.set_scale(calibration_ratio);

      Serial.print(scale.get_units(5), 1);
      Serial.print(" kg; ");

      Serial.print("TempInt: ");
      sensorInt.requestTemperatures(); // Send the command to get temperature readings
      Serial.print(sensorInt.getTempCByIndex(0));
      Serial.print(" C; ");

      Serial.print("TempExt: ");
      sensorExt.requestTemperatures(); // Send the command to get temperature readings
      Serial.print(sensorExt.getTempCByIndex(0));
      Serial.print(" C; ");

      Serial.print("Vcc: ");
      Serial.print(ESP.getVcc());
      Serial.print(" V");

      Serial.println();

      delay(500);
    }
  }

  delay(5000); // allows user to start serial monitor
}

float voltage;

void loop()
{

  // read sensors
  voltage = ESP.getVcc() / 1024.0;
  weight_data = scale.get_units(10);

  /*
 * https://www.letscontrolit.com/forum/viewtopic.php?t=1357
 * WeMos has, just as the NodeMCU board, 2 resistors on the ADC/Tout pin, 100k to ground and 220k to the external ADC pin.
This means the ADC is always loaded with 100k which is not allowed when measuring VCC-internal. It results in a measured value 
being about 10% too low. Correction it is easy, in Optional Settings the field Formula needs this formula: %value%*1.1
 */


  msg = msgEmpty;
  msg.weight = convertFloatToUInt16(weight_data, 0, 300); 
    sensorInt.requestTemperatures();               // Send the command to get temperature readings
  msg.tempInt = convertFloatToUInt16(sensorInt.getTempCByIndex(0), -50, 120);
    sensorExt.requestTemperatures();   
  msg.tempExt = convertFloatToUInt16(sensorExt.getTempCByIndex(0), -50, 120);
  msg.espVoltage = convertFloatToUInt8(voltage, 1, 8);

  // Dummy payload
  /*
  msg.weight = (uint16_t)50;
  msg.tempInt = (uint16_t)37;
  msg.tempExt = (uint16_t)20;
  msg.espVoltage = (uint8_t)3;

  msg.weight = convertFloatToUInt16(50, 0, 300);
  msg.tempInt = convertFloatToUInt16(37, -50, 120);
  msg.tempExt = convertFloatToUInt16(20, -50, 120);
  msg.espVoltage = convertFloatToUInt8(3.3, 1, 8);
  */

  // Send over WiFi
  if (WIFIACTIVE)
    sendWifi();

  // Send over Sigfox
  if (SIGFOXACTIVE)
    sendSigfox();

  putToSleep();

  //add sigfox sleep routine
}

void sendWifi()
{
  // Connect to WiFi access point. ONLY IF WIFI
  Serial.println(); //serial interface flush
  delay(10);

  Serial.print(F("Connecting to "));
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();

  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());

  // Publish data to ThingSpeak
  if (client.connect(thingSpeakServer, 80))
  {

    Serial.println(F("ThingSpeak publishing starts"));

    // Construct API request body
    String body = "field1=";
    body += String(msg.weight);
    body += "&field2=";
    body += String(msg.tempInt);
    body += "&field3=";
    body += String(msg.tempExt);
    body += "&field4=";
    body += String(msg.espVoltage);

    Serial.println(body);

    client.println("POST /update HTTP/1.1");
    client.println("Host: api.thingspeak.com");
    client.println("User-Agent: ESP8266 (nothans)/1.0");
    client.println("Connection: close");
    client.println("X-THINGSPEAKAPIKEY: " + writeAPIKey);
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Content-Length: " + String(body.length()));
    client.println("");
    client.print(body);

    Serial.println(F("ThingSpeak publishing ends"));
    delay(500);
  }
  client.stop();
}

void sendSigfox()
{

  Sigfox.println("AT$P=0"); // AT$P=0 to wake up Wisol
  delay(100);
  getPAC();

  sendMessage((uint8_t *)&msg, 7);

  Sigfox.println("AT$P=1"); // sleep mode for Wisol Sigfox, AT$P=0 to wake up (automatic)
  ESP.deepSleep(12 * 60e6, WAKE_RF_DISABLED);
}

//Get Sigfox ID
String getID()
{
  Serial.println("getID");
  String id = "";
  char output;

  Sigfox.print("AT$I=10\r");
  while (!Sigfox.available())
  {
    Serial.println("Sigfox not available");
    delay(1000);
  }

  while (Sigfox.available())
  {
    output = Sigfox.read();
    id += output;
    delay(10);
  }

  Serial.println("Sigfox Device ID: ");
  Serial.println(id);

  return id;
}

//Get PAC number
String getPAC()
{
  Serial.println("getPAC");
  String pac = "";
  char output;

  Sigfox.print("AT$I=11\r");
  while (!Sigfox.available())
  {
    delay(1000);
  }

  while (Sigfox.available())
  {
    output = Sigfox.read();
    pac += output;
    delay(10);
  }

  Serial.println("PAC number: ");
  Serial.println(pac);

  return pac;
}

//Send Sigfox Message
void sendMessage(uint8_t msg[], int size)
{
  Serial.println("Inside sendMessage");

  String status = "";
  String hexChar = "";
  String sigfoxCommand = "";
  char output;

  sigfoxCommand += "AT$SF=";

  for (int i = 0; i < size; i++)
  {
    hexChar = String(msg[i], HEX);

    //padding
    if (hexChar.length() == 1)
    {
      hexChar = "0" + hexChar;
    }

    sigfoxCommand += hexChar;
  }

  Serial.println("Sending...");
  Serial.println(sigfoxCommand);
  Sigfox.println(sigfoxCommand);

  while (!Sigfox.available())
  {
    Serial.println("Waiting for response");
    delay(1000);
  }

  while (Sigfox.available())
  {
    output = (char)Sigfox.read();
    status += output;
    delay(10);
  }

  Serial.println();
  Serial.print("Status \t");
  Serial.println(status);
}

void putToSleep()
{

  scale.power_down();       // HX711 to sleep
  Sigfox.println("AT$P=1"); // sleep mode for Wisol Sigfox
  // no need to put DS18B20 to sleep, automatic standby mode

  delay(1000); // wait for tasks to complete

  if (WIFIACTIVE)
  {
    ESP.deepSleep(WIFISLEEPTIME);
  }
  else
  {
    ESP.deepSleep(SIGFOXSLEEPTIME);
  }
}
