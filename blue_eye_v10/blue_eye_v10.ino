// Project Blue Eye
// by Pau Anta, more information at http://www.pauanta.bio
// This code has an MIT License

// Development of an open-source low-cost deep-sea drop-cam for marine exploration
// Inspired in the Drop-Cam invented by Alan Turchik, National Geographic Innovation Labs
// Advised by Brennan Phillips, Rhode Island University, https://web.uri.edu/oce/brennan-phillips/

// Started development on December 2018
// Goal: up to 500m of depth, for a maximum cost of $500. However, the Bill of Materials to build this model costs about $1,400
// The Blue Eye is a device controlled by an Artuino MKR WiFi, connected to a GoPro Hero5 Session

// Code to connect Arduino to GoPro's wifi and control its video features written by @randofo from Instructables, license CC BY-NC-SA 4.0

// January 2019 - system is activated by a contactless magnetic switch
// April 2019 - from v6, code introduces millis() instead of delay() to allow for running code while waiting (dropTime and recordingTime).
// April 2019 - integrated temp and humidity sensor
// May 2019 - integrated lightning control
// June 2019 - burnwire coded
// July 2019 - tests in the marine biosphere reserve in Menorca at 50 meters deep
// October 2019 - refinement of features and added tech specs in comments
// May 2022 - refinement of burnwire, added PWM to lights
// Aug 2022 - tests in Menorca.
// October 2022 - release of design, assembly guide, 3D parts and code in GitHub.

// Gopro Hero 5 session
// Key aspects of GoPro's statuses
// - Time it takes to Arduino to connect to GoPro: between 6 and 8 seconds
// - MAX time between turning GoPro ON and turning electronic switch of the blue eye ON: 2 minutes
// - MAX time between connection of Arduino to GoPro and wake up GoPro: 3 minutes
// - MAX time between stop recording and connecttoGoPro again: 2 minutes 
// - MAX time GoPro is awake before it sleeps down: 5 minutes

// T0 - turn GoPro on
// T1 - turn electronic switch (ES) on. T1 = T0 + max 2 minutes
// T2 - Arduino connects to GoPro. T2 = T1 + 6 seconds aprox.
// T3 - turn magnetic switch (MS) on: loop program starts and wakes up GoPro. T3 = T2 + max 3 minutes
//      droptime max 5 minutes
// T4 - GoPro starts recording. T4 = T3 + droptime


#include <RTCZero.h>
#include <SD.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include "wifi.h"
#include "DHT.h"

const int oneMinute = 60000;      // 60,000 millisecons = 1 minute
                                  // settings for the mission
int dropTime = 1;                 // estimated duration to get to the seabed in minutes
                                  // it CAN'T be more than 5m. GoPro turns into sleep mode after 5m
int recordingTime = 20;           // duration of video recording in minutes

unsigned long time_now = 0;
                                  // LEDs for signaling status
const int LEDstatus = 0;          // OUT BLUE signals the blue_eye is on (it has power)
const int LEDmagswitch = 1;       // OUT YELLOW signals the magnetic switch has been activated
const int LEDwifi = 2;            // OUT GREEN signals Arduino is connected to GoPro's WiFi.
const int DHTPin = 3;             // IN read values of humidity and temperature sensor
const int chipSelect = 4;         // selection of SD card in Arduino MKR MEM shield
const int lightsPin = 5;          // OUT turn lights on/off
const int magswitchPin = 6;       // IN read magnetic switch (MS)
const int burnwirePin = 7;        // OUT activates burnwire
bool justWait = true;             // variable to prevent repetition of loop 

int switchState;                  // status of magnetic switch
#define DHTTYPE DHT22             // AM2303 is a wired DHT22 sensor for temp and humidity             
DHT dht(DHTPin, DHTTYPE);         // Initialize DHT sensor
                                  
char ssid[] = SECRET_SSID;        // Wifi's SSID of GoPro, in wifi.h
char pass[] = SECRET_PASS;        // Wifi's password of GoPro, in wifi.h
int status = WL_IDLE_STATUS;
int localPort = 7;
byte broadCastIp[] = { 10,5,5,9 };
byte remote_MAC_ADD[] = { 0xf6, 0xdd, 0x9e, 0x82, 0x7d, 0x4e };
int wolPort = 9;
const char* host = "10.5.5.9";
const int httpPort = 80;

#define nameLog "log.txt"         // File name of Log

WiFiUDP Udp;
WiFiClient client;

// variables for timer
const byte seconds = 0;
const byte minutes = 0;
const byte hours = 0;

RTCZero rtc;

// SET UP of the program
void setup() {

  // WARNING - Serial communications only activated when testing and debuggind with serial monitor.
  // Serial.begin(9600);
  // while (!Serial) {
  //  }

  // setup pins
  pinMode (magswitchPin, INPUT_PULLUP);
  pinMode (lightsPin, OUTPUT);
  pinMode (burnwirePin, OUTPUT);  
  
  digitalWrite (burnwirePin, LOW);

  pinMode (LEDstatus, OUTPUT);
  pinMode (LEDmagswitch, OUTPUT);
  pinMode (LEDwifi, OUTPUT);

  // turns on LED blue: blue_eye is ON
  digitalWrite (LEDstatus, HIGH);

  printInLog("Hello! Welcome to the blue_eye 2.0");

  // initialize and set up timer
  rtc.begin();
  rtc.setHours(hours);
  rtc.setMinutes(minutes);
  rtc.setSeconds(seconds);

  dht.begin();

  // check if SD card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    printInLog("SD card failed, or not present in MEM Shield");
    while (1);
  }
  else{
    initializeLog();
  }

  // check for the presence of the wifi in Arduino
  if (WiFi.status() == WL_NO_SHIELD) {
      printInLog("WiFi chipset not present in Arduino");
     while (true);
  }

  ConnectToGoPro();

}


void loop() {

  MagneticSwitch();         // user turns MS only when is ready to drop the blue eye. MAX time between ES and MS is 3m. After that, GoPro sleeps down

  WakeupGoPro();
  
  time_now = millis();
  
  printInLog("blue_eye on its way to the deep sea");

  while(millis() < time_now + (dropTime * oneMinute)){
     readDHT22Values();
     delay(oneMinute);
  }

  printInLog("blue_eye arrived to the seabed");

  printInLog("Turning lights on");

  // turns on lights gradually 
  for(int i=0; i<255; i++){
    analogWrite(lightsPin, i);
    delay(5);
  }

  StartRecording();         // it may be nice to start recording soon after you drop the camera so you can record how light change as the camera goes down 

  time_now = millis();

  // during recording time, checks humidity and temperature every minute
  while(millis() < time_now + (recordingTime * oneMinute)){
    readDHT22Values();
    delay(oneMinute);
  }

  // if you want to play with lights intensity, on or off, this is the place to do it 

  StopRecording();
  
  // turns off lights gradually 
  for(int i=255; i>0; i--){
    analogWrite(lightsPin, i);
    delay(50);
  }

  digitalWrite(lightsPin, LOW);
 
  printInLog("Turning lights off");

  BurnWire();

  DisconnectFromGoPro();

  digitalWrite (LEDmagswitch, LOW);

  while(justWait == true) {
  }

}

// Arduino attempts to connect to GoPro's wifi network, developed by @randofo from Instructables

void ConnectToGoPro(){
  
  status = WiFi.begin(ssid, pass);

  while ( status != WL_CONNECTED) {
    printInLog("Arduino is attempting to connect to GoPro's WiFi ...");
    printInLog(String(status));
    for (int i = 0; i <= 1; i++){
      digitalWrite(LEDwifi, HIGH);    // blinking LED green
      delay(1000);
      digitalWrite(LEDwifi, LOW);
      delay(1000);
    }
  }

  printInLog("Arduino is connected to GoPro's WiFi network");
  
  digitalWrite (LEDwifi, HIGH);       // LED green turns on after GoPro is connected

  for (int i = 0; i <= 2; i++) {      // lights blink three times after Arduino connects to gopro
     digitalWrite(lightsPin, HIGH);
     delay(500);
     digitalWrite(lightsPin, LOW);
     delay(500);
  }
  
  // printWifiStatus();

}


// Arduino disconnects from GoPro's WiFi, developed by @randofo from Instructables
void DisconnectFromGoPro(){

  WiFi.disconnect();

  digitalWrite (LEDwifi, LOW);

}


//controls activation with magnetic switch 
void MagneticSwitch(){

  digitalWrite(LEDmagswitch, LOW);

  switchState = digitalRead(magswitchPin);
  
  while (switchState == HIGH){
     switchState = digitalRead(magswitchPin);
   }

  digitalWrite(LEDmagswitch, HIGH);

  printInLog("Magnetic switch activated");
  
}


// wake up GoPro, developed by @randofo from Instructables
void WakeupGoPro(){

  //Begin UDP communication
  Udp.begin(localPort);

  //Send the magic packet to wake up the GoPro out of sleep
  delay(2000);
  SendMagicPacket();
  delay(6000);  

  // Absolutely necessary to flush port of UDP junk for Wifi client communication
  Udp.flush();
  delay(1000);

  //Stop UDP communication
  Udp.stop();
  delay(1000);

  printInLog("Waking up GoPro");
  
}


// Function to create and send magic packet, developed by @randofo from Instructables
// https://www.logicaprogrammabile.it/wol-accendere-computer-arduino-wake-on-lan/

void SendMagicPacket(){

  //Create a 102 byte array
  byte magicPacket[102];

  // Variables for cycling through the array
  int Cycle = 0, CycleMacAdd = 0, IndexArray = 0;

  // This for loop cycles through the array
  for( Cycle = 0; Cycle < 6; Cycle++){

    // The first 6 bytes of the array are set to the value 0xFF
    magicPacket[IndexArray] = 0xFF;

    // Increment the array index
    IndexArray++;
  }

  // Now we cycle through the array to add the GoPro address
  for( Cycle = 0; Cycle < 16; Cycle++ ){

    //eseguo un Cycle per memorizzare i 6 byte del mac address
    for( CycleMacAdd = 0; CycleMacAdd < 6; CycleMacAdd++){
      
      magicPacket[IndexArray] = remote_MAC_ADD[CycleMacAdd];
      
      // Increment the array index
      IndexArray++;
    }
  }

  //The magic packet is now broadcast to the GoPro IP address and port
  Udp.beginPacket(broadCastIp, wolPort);
  Udp.write(magicPacket, sizeof magicPacket);
  Udp.endPacket();

}


void StartRecording(){
  
  if (!client.connect("10.5.5.9", httpPort)) {
    printInLog("Start recording failed");
    return;
  }

  //Command for start recording
  String StartUrl = "/gp/gpControl/command/shutter?p=1";
  client.print(String("GET ") + StartUrl + " HTTP/1.1\r\n" +
  "Host: " + host + "\r\n" +
  "Connection: close\r\n\r\n");

  printInLog("GoPro started recording");
  
}


void StopRecording(){
  
  if (!client.connect("10.5.5.9", httpPort)) {
    printInLog("Stop recording failed");
    return;
  }

  //Command for stopping recording, developed by @randofo from Instructables
  
  String StopUrl = "/gp/gpControl/command/shutter?p=0";
  client.print(String("GET ") + StopUrl + " HTTP/1.1\r\n" +
  "Host: " + host + "\r\n" +
  "Connection: close\r\n\r\n");

  printInLog("GoPro stopped recording");

}


void initializeLog(){

  if (SD.exists(nameLog)) {     // removes previous file in order to start a new log
    SD.remove(nameLog);       
  }

}


// coding for logging in a text file stored in microSD card 
void printInLog(String dataString){

  File dataFile = SD.open(nameLog, FILE_WRITE);

  if (dataFile) {
    dataFile.println(String(rtc.getHours()) 
    + ":" + String(rtc.getMinutes()) 
    + ":" + String(rtc.getSeconds()) 
    + "\t" + dataString);
    dataFile.close();

    // WARNING :: need to comment this part for real deployment

    // Serial.println(String(rtc.getHours()) 
    //  + ":" + String(rtc.getMinutes()) 
    //  + ":" + String(rtc.getSeconds()) 
    //  + "\t" + dataString);
    
  }
  else {
//    Serial.println("Error opening log");
  }
}


void printWifiStatus() {

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  printInLog("IP Address: " + String(ip));

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  printInLog("signal strength:" + String(rssi) + "dBm");

}


// code for reading values of DHT22 sensors: humidity and temperature
void readDHT22Values(){

  // Reading temperature or humidity takes about 250 milliseconds
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  if (t > 80.0){
    BurnWire();
  }

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    printInLog("Failed to read from DHT sensor");
    return;
  }
  
  printInLog("Humidity:  " + String(h) + "%   Temperature:  " + String(t) + "°C");

}

// burn wire
void BurnWire(){

  printInLog("Burning wire");

  // turns burnwire on to cut the line to the ballast
  digitalWrite (burnwirePin, HIGH);
  delay(oneMinute*10);
  digitalWrite (burnwirePin, LOW);

  printInLog("End of mission. blue_eye is coming up!");
  
}
