#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <DallasTemperature.h>
#include <OneWire.h>


// Token generation helper
#include "addons/TokenHelper.h"
// RealTimeDB data printing helper
#include "addons/RTDBHelper.h"

// Define sensor I/O pin & set up onewire protocol for it
#define sensor_pin 4
OneWire oneWire(sensor_pin);
DallasTemperature sensors(&oneWire);

// I/O Pin for power signal
int powerPin = 33;

// Board indentifier, for example when using multiple boards in a system
//#define DEVICE_UID "escrowESP"

// WiFi ssid & password
#define WIFI_SSID "ssid" // add network SSID    
#define WiFi_PASSWORD "password" // password

// Firebase DB access
#define API_KEY  "api_key" // add firebase API key
#define DB_URL "https://remoteaccessesp-default-rtdb.europe-west1.firebasedatabase.app/"     

// Setting up firebase obj's
FirebaseData fbdo;
FirebaseData streamfbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json;


String fuid = "";
bool isAuth = false;
bool startUp = false;

// Time intervals
unsigned long elapsedMillis = 0;
unsigned long update_interval = 10000;
unsigned long current_time = 0;
unsigned long turn_on_interval = 1100;
unsigned long turn_off_interval = 3500;

// signal action flags
bool on_signal = false;
bool off_signal = false;


// Wifi connection
void Wifi_init() {
  WiFi.begin(WIFI_SSID, WiFi_PASSWORD);
  Serial.print("Connecting to wifi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.print("Connected!  ");
  Serial.print(WiFi.localIP());
  Serial.println();
}


// Firebase connection with anonymous sign in
void FireBase_init() {
  config.api_key = API_KEY;
  config.database_url = DB_URL;
  // Connect to wifi incase lost connection
  Firebase.reconnectWiFi(true);
 
  Serial.println("Sign up anon user...");

  // Unauthorized sign up to Firebase (Depends on the rules set in Firebase)
  if(Firebase.signUp(&config, &auth, "", "")){
    Serial.println("Success");
    isAuth = true;
    fuid = auth.token.uid.c_str();   // rem
  }
  else{
    Serial.printf("Failed,  %s\n", 
    config.signer.signupError.message.c_str());
    isAuth = false;
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
}


// This sends values from sensors to DB
void update_DB() {

  // Database update interval, set to update every 10seconds
  //if (millis() - elapsedMillis > update_interval && isAuth && Firebase.ready()){
    if (isAuth && Firebase.ready()){
    //elapsedMillis = millis();
    Serial.println(elapsedMillis);
    sensors.requestTemperatures();
    // Using multiple sensors, can be accessed by index
    float temp = sensors.getTempCByIndex(0);
    float temp2 = sensors.getTempCByIndex(1);
    float temp3 = sensors.getTempCByIndex(2);

    // Setting up a firebase json object with sensor values
    json.set("sensor_1", temp);
    Serial.println("sen1");
    json.set("sensor_2", temp2);
    Serial.println("sen2");
    json.set("sensor_3", temp3);
    Serial.println("sen3");

    // Update new values to selected node(s)
    String node = "proto_loc1/Sensor_readings/";
    if(Firebase.updateNode(fbdo, node.c_str(), json)){
            Serial.println("Temperature nodes updated!");
    }else{
      Serial.println("Error updating");
      Serial.println(fbdo.errorReason());
      Serial.println();
    }
  }
}

// Function to listen changes in the DB on set location (proto_loc1/Control/heater)
void streamCallback(StreamData data) {

  Serial.print("Control set to : ");
  Serial.println(data.intData());  // streamPath() , dataType()

  // Checking for datatype and using the data to turn on/off heating
  if(data.dataType() == "int"){
    if(data.intData() == 1){
       // Call function to turn on heating
       Serial.println("Turning on the heater");
      //heater_ON();
      on_signal = true;
    }else if(data.intData() == 2){
        // Call function to trun off heating
        Serial.println("Turning the heater off");
        //heater_OFF();
        off_signal = true;
    }
  }else{
    Serial.println("Error on 'datatype'");
  }
}

void streamTimeoutCallback(bool timeout) {
  if(timeout){
    Serial.println("Stream timedout, resuming the stream...");
  }
}



void setup() {

  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(powerPin, OUTPUT);
 
  // connect to wifi & firebase
  Wifi_init();
  FireBase_init();

  // Start reading sensor data
  sensors.begin();

  // Firebase stream on heater commands
  Firebase.setStreamCallback(streamfbdo, streamCallback, streamTimeoutCallback);
  if(!Firebase.beginStream(streamfbdo, "proto_loc1/Control/heater")){
    Serial.println(streamfbdo.errorReason());
  }
}

void loop() {

  // put your main code here, to run repeatedly:

  // Set "current time"
  current_time = millis();

  // Activate heater if on_signal is active by giving power accordingly (short interval)
  // This is simulating short button press on the heater control unit PB
  if(on_signal == true){
       do{
        digitalWrite(powerPin, HIGH);
      }while(current_time + (millis()-current_time) < current_time + turn_on_interval);
      on_signal = false;
      digitalWrite(powerPin, LOW);
  }

  // Turn off heater if off_signal is called by giving power (long interval)
  // This will turn off the heater by simulating long press on the heater control unit PB
  if(off_signal == true){
      do{
        digitalWrite(powerPin, HIGH);
      }while(current_time + (millis()-current_time) < current_time + turn_off_interval);
      off_signal = false;
      digitalWrite(powerPin, LOW);
  }

  // Calls for database update when chosen time has passed (set to 10sec)
  if(millis() - elapsedMillis > update_interval){
      elapsedMillis = millis();
      update_DB();
  }
 
}