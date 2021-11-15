
/* For wifi connection please refer to this code:
https://github.com/khoih-prog/ESP_WiFiManager/blob/master/examples/ConfigOnStartup/ConfigOnStartup.ino
Barcode scanning function:
Using barcode scanner to get the package barcode and send it to the REST API of our backend server(refer to https://github.com/Porchster/backend_web_version)
Check passcode function:
Check the passcode from the keypad input. Send it to the REST API for checking whether the passcode match in our database.*/

#include <ETH.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <WiFiMulti.h>
#include <WiFiScan.h>
#include <WiFiServer.h>
#include <WiFiSTA.h>
#include <WiFiType.h>
#include <WiFiUdp.h>

#include <stdio.h>
#include <string.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <FirebaseESP32.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>


// wifi manager
#define _WIFIMGR_LOGLEVEL_    3
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>

// From v1.1.0
#include <WiFiMulti.h>
WiFiMulti wifiMulti;

#define USE_SPIFFS      true

#if USE_SPIFFS
#include <SPIFFS.h>
FS* filesystem =      &SPIFFS;
#define FileFS        SPIFFS
#define FS_Name       "SPIFFS"
#else
// Use FFat
#include <FFat.h>
FS* filesystem =      &FFat;
#define FileFS        FFat
#define FS_Name       "FFat"
#endif
//////

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

#define LED_BUILTIN       2
#define LED_ON            HIGH
#define LED_OFF           LOW

// SSID and PW for Config Portal
String ssid = "Porchster";
const char* password = "123456";

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

// From v1.1.0
// You only need to format the filesystem once
//#define FORMAT_FILESYSTEM       true
#define FORMAT_FILESYSTEM         false

#define MIN_AP_PASSWORD_SIZE    8

#define SSID_MAX_LEN            32
//From v1.0.10, WPA2 passwords can be up to 63 characters long.
#define PASS_MAX_LEN            64

typedef struct
{
  char wifi_ssid[SSID_MAX_LEN];
  char wifi_pw  [PASS_MAX_LEN];
}  WiFi_Credentials;

typedef struct
{
  String wifi_ssid;
  String wifi_pw;
}  WiFi_Credentials_String;

#define NUM_WIFI_CREDENTIALS      2

typedef struct
{
  WiFi_Credentials  WiFi_Creds [NUM_WIFI_CREDENTIALS];
} WM_Config;

WM_Config         WM_config;

#define  CONFIG_FILENAME              F("/wifi_cred.dat")
//////

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig = false;

// Use false if you don't like to display Available Pages in Information Page of Config Portal
// Comment out or use true to display Available Pages in Information Page of Config Portal
// Must be placed before #include <ESP_WiFiManager.h>
#define USE_AVAILABLE_PAGES     false

// From v1.0.10 to permit disable/enable StaticIP configuration in Config Portal from sketch. Valid only if DHCP is used.
// You'll loose the feature of dynamically changing from DHCP to static IP, or vice versa
// You have to explicitly specify false to disable the feature.
//#define USE_STATIC_IP_CONFIG_IN_CP          false

// Use false to disable NTP config. Advisable when using Cellphone, Tablet to access Config Portal.
// See Issue 23: On Android phone ConfigPortal is unresponsive (https://github.com/khoih-prog/ESP_WiFiManager/issues/23)
#define USE_ESP_WIFIMANAGER_NTP     false

// Use true to enable CloudFlare NTP service. System can hang if you don't have Internet access while accessing CloudFlare
// See Issue #21: CloudFlare link in the default portal (https://github.com/khoih-prog/ESP_WiFiManager/issues/21)
#define USE_CLOUDFLARE_NTP          false

// New in v1.0.11
#define USING_CORS_FEATURE          true
//////

// Use USE_DHCP_IP == true for dynamic DHCP IP, false to use static IP which you have to change accordingly to your network
#if (defined(USE_STATIC_IP_CONFIG_IN_CP) && !USE_STATIC_IP_CONFIG_IN_CP)
// Force DHCP to be true
#if defined(USE_DHCP_IP)
#undef USE_DHCP_IP
#endif
#define USE_DHCP_IP     true
#else
// You can select DHCP or Static IP here
//#define USE_DHCP_IP     true
#define USE_DHCP_IP     false
#endif

// Use USE_DHCP_IP == true for dynamic DHCP IP, false to use static IP which you have to change accordingly to your network
//#if (defined(USE_STATIC_IP_CONFIG_IN_CP) && !USE_STATIC_IP_CONFIG_IN_CP)
// Force DHCP to be true
//#if defined(USE_DHCP_IP)
//#undef USE_DHCP_IP
//#endif
//#define USE_DHCP_IP     true
//#else
// You can select DHCP or Static IP here
//#define USE_DHCP_IP     true
//#define USE_DHCP_IP     false
//#endif

#if ( USE_DHCP_IP || ( defined(USE_STATIC_IP_CONFIG_IN_CP) && !USE_STATIC_IP_CONFIG_IN_CP ) )
// Use DHCP
#warning Using DHCP IP
IPAddress stationIP   = IPAddress(0, 0, 0, 0);
IPAddress gatewayIP   = IPAddress(192, 168, 2, 1);
IPAddress netMask     = IPAddress(255, 255, 255, 0);
#else
// Use static IP
#warning Using static IP
#ifdef ESP32
IPAddress stationIP   = IPAddress(192, 168, 2, 232);
#else
IPAddress stationIP   = IPAddress(192, 168, 2, 186);
#endif

IPAddress gatewayIP   = IPAddress(192, 168, 2, 1);
IPAddress netMask     = IPAddress(255, 255, 255, 0);
#endif

#define USE_CONFIGURABLE_DNS      true

IPAddress dns1IP      = gatewayIP;
IPAddress dns2IP      = IPAddress(8, 8, 8, 8);

#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager

// Onboard LED I/O pin on NodeMCU board
const int PIN_LED = 2; // D4 on NodeMCU and WeMos. GPIO2/ADC12 of ESP32. Controls the onboard LED.

// wifi manager
HardwareSerial MySerial(1);
WiFiMulti WiFiMulti;
HTTPClient http;
DynamicJsonDocument doc(32);

//Define FirebaseESP32 data object
FirebaseData firebaseData;
FirebaseJson json;
char rx_byte = 0;

const char* mqtt_server = "broker.hivemq.com";
#define mqtt_port 1883
//Save these for later when its time to wrap in security
//#define MQTT_USER ""
//#define MQTT_PASSWORD ""
char MQTT_SERIAL_RECEIVER_CH[100];
char MQTT_SERIAL_PUBLISH_CH[100];
char MQTT_GETUSERID[100];
char UID[50];

const byte numChars = 32;
char receivedChars[numChars]; // an array to store the received data
boolean newData = false;
uint32_t value = 0;

//These will need to be updated to the GPIO pins for each control circuit.

int RELAY_SWITCH = 14;
int WIFI_CLIENT_CONNECTED = 2;
int LOCK_SENSOR = 17;
int locked_flag = 0;

WiFiClient wifiClient;

PubSubClient client(wifiClient);

//Keypad setup
const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
//byte rowPins[ROWS] = {33, 32, 22, 23}; //connect to the row pinouts of the keypad
//byte colPins[COLS] = {27, 26, 25}; //connect to the column pinouts of the keypad
// if connect the pins from left to right we should define the rowPins and colPins like this order:
byte rowPins[ROWS] = {23, 22, 32, 33};
byte colPins[COLS] = {25, 26, 27};
// String pinCode = "";

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
int key_pressed_count = 0;
String key_value = "";

const byte interruptPin = 17;

unsigned long lastDetection = 0;
unsigned long debounceTime = 250;
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;
// wifi manager

uint8_t connectMultiWiFi(void);

//prints to Serial show status while testing
void heartBeatPrint(void)
{
  static int num = 1;

  if (WiFi.status() == WL_CONNECTED)
    Serial.print("H");        // H means connected to WiFi
  else
    Serial.print("F");        // F means not connected to WiFi

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(" ");
  }
}

//show wifi status
void check_WiFi(void)
{
  if ( (WiFi.status() != WL_CONNECTED) )
  {
    Serial.println("\nWiFi lost. Call connectMultiWiFi in loop");
    connectMultiWiFi();
  }
}

//call check functions
void check_status(void)
{
  static ulong checkstatus_timeout  = 0;
  static ulong checkwifi_timeout    = 0;
  static ulong current_millis;

#define WIFICHECK_INTERVAL    1000L
#define HEARTBEAT_INTERVAL    10000L

  current_millis = millis();

  // Check WiFi every WIFICHECK_INTERVAL (1) seconds.
  if ((current_millis > checkwifi_timeout) || (checkwifi_timeout == 0))
  {
    check_WiFi();
    checkwifi_timeout = current_millis + WIFICHECK_INTERVAL;
  }

  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((current_millis > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = current_millis + HEARTBEAT_INTERVAL;
  }
}

void loadConfigData(void)
{
  File file = FileFS.open(CONFIG_FILENAME, "r");
  LOGERROR(F("LoadWiFiCfgFile "));

  if (file)
  {
    file.readBytes((char *) &WM_config, sizeof(WM_config));
    file.close();
    LOGERROR(F("OK"));
  }
  else
  {
    LOGERROR(F("failed"));
  }
}

void saveConfigData(void)
{
  File file = FileFS.open(CONFIG_FILENAME, "w");
  LOGERROR(F("SaveWiFiCfgFile "));

  if (file)
  {
    file.write((uint8_t*) &WM_config, sizeof(WM_config));
    file.close();
    LOGERROR(F("OK"));
  }
  else
  {
    LOGERROR(F("failed"));
  }
}

uint8_t connectMultiWiFi(void)
{
#if ESP32
  // For ESP32, this better be 0 to shorten the connect time
#define WIFI_MULTI_1ST_CONNECT_WAITING_MS       0
#else
  // For ESP8266, this better be 2200 to enable connect the 1st time
#define WIFI_MULTI_1ST_CONNECT_WAITING_MS       2200L
#endif

#define WIFI_MULTI_CONNECT_WAITING_MS           100L

  uint8_t status;
  LOGERROR(F("ConnectMultiWiFi with :"));

  if ( (Router_SSID != "") && (Router_Pass != "") )
  {
    LOGERROR3(F("* Flash-stored Router_SSID = "), Router_SSID, F(", Router_Pass = "), Router_Pass );
  }

  for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
  {
    // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
    if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
    {
      LOGERROR3(F("* Additional SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
    }
  }

  LOGERROR(F("Connecting MultiWifi..."));
  WiFi.mode(WIFI_STA);

#if !USE_DHCP_IP
#if USE_CONFIGURABLE_DNS
  // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
  WiFi.config(stationIP, gatewayIP, netMask, dns1IP, dns2IP);
#else
  // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
  WiFi.config(stationIP, gatewayIP, netMask);
#endif
#endif

  int i = 0;
  status = wifiMulti.run();
  delay(WIFI_MULTI_1ST_CONNECT_WAITING_MS);

  while ( ( i++ < 10 ) && ( status != WL_CONNECTED ) )
  {
    status = wifiMulti.run();
    if ( status == WL_CONNECTED )
      break;
    else
      delay(WIFI_MULTI_CONNECT_WAITING_MS);
  }

  if ( status == WL_CONNECTED )
  {
    LOGERROR1(F("WiFi connected after time: "), i);
    LOGERROR3(F("SSID:"), WiFi.SSID(), F(",RSSI="), WiFi.RSSI());
    LOGERROR3(F("Channel:"), WiFi.channel(), F(",IP address:"), WiFi.localIP() );
  }
  else
    LOGERROR(F("WiFi not connected"));

  return status;
}

// wifi manager
void setup_wifi() {

  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  digitalWrite(WIFI_CLIENT_CONNECTED, HIGH);

  // setup mac based channel for MQTT
  int first_char, second_char, quotient;
  byte MAC[6];
  WiFi.macAddress(MAC);
  char macAdd[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  for (int i = 0; i < 6; i++) {
    quotient = MAC[i];
    second_char = quotient % 16;
    first_char = quotient / 16;

    //convert to readable value
    if (first_char < 10)
      macAdd[2 * i] = 48 + first_char;
    else
      macAdd[2 * i] = 55 + first_char;

    if (second_char < 10)
      macAdd[2 * i + 1] = 48 + second_char;
    else
      macAdd[2 * i + 1] = 55 + second_char;
  }
  macAdd[12] = 0;
  strcat(strcat(strcat(MQTT_SERIAL_PUBLISH_CH, "/"), macAdd), "/lockstate");
  strcat(strcat(strcat(MQTT_SERIAL_RECEIVER_CH, "/"), macAdd), "/lockstate");
  strcat(strcat(strcat(MQTT_GETUSERID, "/"), macAdd), "/uid");
}

//unlock porchster
void Unlock()
{
  digitalWrite(RELAY_SWITCH, HIGH); //We only need to set TIMER_SWITCH once....set pumpOn to FALSE in prep for end of OFFTIME.
  Serial.println("Unlock triggered");
  delay(2000);
  digitalWrite(RELAY_SWITCH, LOW);
}

//continuously attempt to reconnect if signal lost
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected!!!");
      // ... and resubscribe
      client.subscribe(MQTT_SERIAL_RECEIVER_CH);
      client.subscribe(MQTT_GETUSERID);
      Serial.println(MQTT_SERIAL_RECEIVER_CH);
      Serial.println(MQTT_GETUSERID);
      //Once connected, publish an announcement...
      //client.publish("/3GRXcEcbhsaKhScugOXhuU9VIVk1/doorstate", "Testing MQTT");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//callback on message received over MQTT
void callback(char* topic, byte *payload, unsigned int length) {
  char * strtokIndx;

  Serial.println("-------new message from broker-----");
  Serial.print("channel:");
  Serial.println(topic);
  Serial.print("data:");
  Serial.write(payload, length);
  Serial.println();
  if (!strncmp(topic, MQTT_SERIAL_RECEIVER_CH, strlen(MQTT_SERIAL_RECEIVER_CH))) {
    if (!strncmp((char *)payload, "UNLOCK", length ) && locked_flag) {
      Serial.print("Unlocking.....");
      Unlock();
    }
  }

  if (!strncmp(topic, MQTT_GETUSERID, strlen(MQTT_GETUSERID))) {
    memset(UID, 0, sizeof(UID));
    for (int i = 0; i < length; ++i) {
      UID[i] = ((char *)payload)[i];
    }
    Serial.print("UID set as: ");
    Serial.println(UID);
  }
}

//send serial data over mqtt
void publishSerialData(char *serialData) {
  if (!client.connected()) {
    reconnect();
  }
  client.publish(MQTT_SERIAL_PUBLISH_CH, serialData);
}

//check if scanned barcode exists in database via backend
void checkBarcode(String barCode) {
  String URL = "https://us-central1-porchster-web.cloudfunctions.net/api/scanning/" + barCode + "/" + UID;
  Serial.println(URL);
  String Data = httpGETRequest(URL);
  JSONVar Object = JSON.parse(Data);
  String check = JSON.stringify(Object["check"]);
  if (check == "true") {
    Unlock();
  } else {
    Serial.println("barcode do not exist");
  }
}

//check if passcode is correct via backend
void checkPasscode(String passCode) {
  passCode.remove(passCode.length() - 1);
  String URL = "https://us-central1-porchster-web.cloudfunctions.net/api/checkpasscode/" + passCode + "/" + UID;
  Serial.println(URL);
  String Data = httpGETRequest(URL);
  JSONVar Object = JSON.parse(Data);
  String check = JSON.stringify(Object["check"]);
  if (check == "true") {
    Unlock();
  } else {
    Serial.println("passcode does not match");
  }
}

// helps with testing in serial console
void recvWithEndMarker() {
  static byte ndx = 0;
  char carriageReturn = '\r';
  char rc;
  while (Serial2.available() > 0) {   //Changing this from "MySerial" to "Serial" so that devs can input directly via Serial terminal...
    rc = Serial2.read();
    if (rc != carriageReturn) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    } else {
      ndx = 0;
      Serial.println(receivedChars);
      // Pass the barcode the our REST API
      checkBarcode(String(receivedChars));
    }
  }
}

//continuously monitor keypad to receive user input, call checkpasscode
void check_keypad() {
  char key = keypad.getKey();

  if (key) {
    key_pressed_count++;
    Serial.print(key_pressed_count);
    Serial.println(key);
    key_value += key;
    Serial.println(key_value);
    if (key == '*')
    {
      Serial.println("Cancel entered, resetting...");
      key_value = "";
      key_pressed_count = 0;
    }
    else if (key_pressed_count < 5)
    {
      return;
    }
    else {
      if (key == '#') {
        Serial.println("Enter key pressed, checking...");
        checkPasscode(key_value);
        key_value = "";
        key_pressed_count = 0;
      }
      else
      {
        return;
      }
    }
  }
}

//create http GET easily
String httpGETRequest(const String serverName) {
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();
  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
  return payload;
}

// this function first to run
void setup() {
  WiFi.mode(WIFI_STA);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  Serial.setDebugOutput(false);
  //  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  //4. Enable auto reconnect the WiFi when connection lost
  Firebase.reconnectWiFi(true);

  //5. Try to set int data to Firebase
  //The set function returns bool for the status of operation
  //firebaseData requires for sending the data
  if (Firebase.setInt(firebaseData, "/LED_Status", 1))
  {
    //Success
    Serial.println("Set int data success");

  } else {
    //Failed?, get the error reason from firebaseData
    Serial.print("Error in setInt, ");
    Serial.println(firebaseData.errorReason());
  }

  pinMode(WIFI_CLIENT_CONNECTED, OUTPUT);
  pinMode(RELAY_SWITCH, OUTPUT);
  pinMode(LOCK_SENSOR, INPUT_PULLUP);

  digitalWrite(WIFI_CLIENT_CONNECTED, LOW);
  digitalWrite(RELAY_SWITCH, LOW);
  digitalWrite(LOCK_SENSOR, HIGH);

  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, 15, 4);
  Serial2.print("My Serial started!");

  digitalWrite(RELAY_SWITCH, LOW);

  // wifi manager
  // initialize the LED digital pin as an output.
  pinMode(PIN_LED, OUTPUT);

  while (!Serial);
  Serial.print("\nStarting ConfigOnStartup with DoubleResetDetect using " + String(FS_Name));
  Serial.println(" on " + String(ARDUINO_BOARD));
  Serial.setDebugOutput(false);

  if (FORMAT_FILESYSTEM)
    FileFS.format();

  // Format FileFS if not yet
#ifdef ESP32
  if (!FileFS.begin(true))
#else
  if (!FileFS.begin())
#endif
  {
    Serial.print(FS_Name);
    Serial.println(F(" failed! AutoFormatting."));

#ifdef ESP8266
    FileFS.format();
#endif
  }
  digitalWrite(PIN_LED, LED_ON); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.
  unsigned long startedAt = millis();

  //Local intialization. Once its business is done, there is no need to keep it around
  // Use this to default DHCP hostname to ESP8266-XXXXXX o  r ESP32-XXXXXX
  ESP_WiFiManager ESP_wifiManager;
  // Use this to personalize DHCP hostname (RFC952 conformed)
  //ESP_WiFiManager ESP_wifiManager("ConfigOnStartup");

  //set custom ip for portal
  //ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 100, 1), IPAddress(192, 168, 100, 1), IPAddress(255, 255, 255, 0));

  ESP_wifiManager.setMinimumSignalQuality(-1);

  // From v1.0.10 only
  // Set config portal channel, default = 1. Use 0 => random channel from 1-13
  ESP_wifiManager.setConfigPortalChannel(0);
  //////

#if !USE_DHCP_IP
#if USE_CONFIGURABLE_DNS
  // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
  ESP_wifiManager.setSTAStaticIPConfig(stationIP, gatewayIP, netMask, dns1IP, dns2IP);
#else
  // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
  ESP_wifiManager.setSTAStaticIPConfig(stationIP, gatewayIP, netMask);
#endif
#endif

  // New from v1.1.1
#if USING_CORS_FEATURE
  ESP_wifiManager.setCORSHeader("Your Access-Control-Allow-Origin");
#endif

  // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();

  //Remove this line if you do not want to see WiFi password printed
  Serial.println("Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  //Check if there is stored WiFi router/password credentials.
  //If not found, device will remain in configuration mode until switched off via webserver.
  Serial.println("Opening configuration portal.");

  // From v1.1.0, Don't permit NULL password
  if ( (Router_SSID != "") && (Router_Pass != "") )
  {
    LOGERROR3(F("* Add SSID = "), Router_SSID, F(", PW = "), Router_Pass);
    wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());

    ESP_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
    Serial.println("Got stored Credentials. Timeout 120s for Config Portal");
  }
  else
  {
    Serial.println("Open Config Portal without Timeout: No stored Credentials.");
    initialConfig = true;
  }

  // SSID to uppercase
  ssid.toUpperCase();
  Serial.println("Starting configuration portal.");
  digitalWrite(PIN_LED, LED_ON); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.

  // Starts an access point
  if (!ESP_wifiManager.startConfigPortal((const char *) ssid.c_str(), password))
    Serial.println("Not connected to WiFi but continuing anyway.");
  else
  {
    Serial.println("WiFi connected...yeey :)");
  }

  // Only clear then save data if CP entered and with new valid Credentials
  // No CP => stored getSSID() = ""
  if ( String(ESP_wifiManager.getSSID(0)) != "" && String(ESP_wifiManager.getSSID(1)) != "" )
  {
    // Stored  for later usage, from v1.1.0, but clear first
    memset(&WM_config, 0, sizeof(WM_config));
    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
    {
      String tempSSID = ESP_wifiManager.getSSID(i);
      String tempPW   = ESP_wifiManager.getPW(i);

      if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);

      if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);

      // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
      {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }
    saveConfigData();
    initialConfig = true;
  }

  digitalWrite(PIN_LED, LED_OFF); // Turn led off as we are not in configuration mode.
  startedAt = millis();

  if (!initialConfig)
  {
    // Load stored data, the addAP ready for MultiWiFi reconnection
    loadConfigData();
    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
    {
      // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
      {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    if ( WiFi.status() != WL_CONNECTED )
    {
      Serial.println("ConnectMultiWiFi in setup");
      connectMultiWiFi();
    }
  }

  Serial.print("After waiting ");
  Serial.print((float) (millis() - startedAt) / 1000L);
  Serial.print(" secs more in setup(), connection result is ");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("connected. Local IP: ");
    Serial.println(WiFi.localIP());
    setup_wifi();
  }
  else
    Serial.println(ESP_wifiManager.getStatus(WiFi.status()));
}

//setup is complete, loop indefinitely to check for inputs and signals to unlock
void loop() {
  //Ensure Wifi is still connected...notify user by turning off BLUE LED.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Lost Wifi Connection...");
    digitalWrite(WIFI_CLIENT_CONNECTED, LOW);
  }
  if (WiFi.status() == WL_CONNECTED) {
    client.loop();
    if (!client.connected()) {
      reconnect();
    }
  }

  //Check scanner
  recvWithEndMarker();

  //Check keypad
  check_keypad();

  //Check is Porchster door was open without known event
  //checkdoor();
  int reading = digitalRead(LOCK_SENSOR);

  if ((reading == LOW) && (locked_flag == 1)) {
    Serial.println("MQTT Latch is OPEN");
    client.publish(MQTT_SERIAL_PUBLISH_CH, "UNLOCK");
    locked_flag = 0;
  }
  if ((reading == HIGH) && (locked_flag == 0)) {
    Serial.println("Latch is CLOSED");
    client.publish(MQTT_SERIAL_PUBLISH_CH, "LOCK");
    locked_flag = 1;
  }
  check_status();
  lastTime = millis();
}
