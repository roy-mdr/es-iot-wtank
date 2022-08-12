/*****************************************************************************/
/*****************************************************************************/
/*               STATUS: WORKING                                             */
/*            TESTED IN: WeMos D1 mini                                       */
/*                   AT: 2022.08.11                                          */
/*     LAST COMPILED IN: PHI                                                 */
/*****************************************************************************/
/*****************************************************************************/

#define clid "hc-sr04"





#include <ESP8266HTTPClient.h> // Uses core ESP8266WiFi.h internally
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>



// *************************************** CONFIG "config_wifi_roy"

#include <config_wifi_roy.h>

#define EEPROM_ADDR_CONNECTED_SSID 1       // Start saving connected network SSID from this memory address
#define EEPROM_ADDR_CONNECTED_PASSWORD 30  // Start saving connected network Password from this memory address
#define AP_SSID clid                       // Set your own Network Name (SSID)
#define AP_PASSWORD "12345678"             // Set your own password

ESP8266WebServer server(80);
// WiFiServer wifiserver(80);

// ***************************************



// *************************************** CONFIG OTA LAN SERVER

ESP8266WebServer serverOTA(5000);
const String serverIndex = String("<div><p>Current sketch size: ") + ESP.getSketchSize() + " bytes</p><p>Max space available: " + ESP.getFreeSketchSpace() + " bytes</p></div><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

// ***************************************



// *************************************** CONFIG "no_poll_subscriber"

#include <NoPollSubscriber.h>

#define SUB_HOST "notify.estudiosustenta.myds.me"
#define SUB_PORT 80
// #define SUB_HOST "192.168.1.72"
// #define SUB_PORT 1010
#define SUB_PATH "/"

// Declaramos o instanciamos un cliente que se conectará al Host
WiFiClient sub_WiFiclient;

// ***************************************



// *************************************** CONFIG THIS SKETCH

// Define connections to sensor
#define TRIGPIN 13
#define ECHOPIN 12
 
// Floats to calculate distance
float duration, distance;

#define DISTANCE_THRESHOLD 100 // distance in mm. When measured a distance higher than this threshold it will update the value in the server
int distanceRange;

float HARD_MIN; // distance in mm. to set 0%
float HARD_MAX; // distance in mm. to set 100%
float SOFT_MIN; // distance in mm. to set the pump ON
float SOFT_MAX; // distance in mm. to set the pump OFF
#define PUMP_PIN 14 // Pin to trigger pump relay

/***** MEASURE TIMEOUT *****/
unsigned long me_timestamp = millis();
/*unsigned int  me_times = 0;*/
unsigned int  me_timeout = 1000; // 1 second. Delay between each measurement in loop
unsigned int  me_track = me_timeout; // me_track = me_timeout; TO: execute function and then start counting

ESP8266WebServer serverConfig(5050);

#define PIN_CTRL 15 // Pin que hará una medición manualmente, si se presiona mas de 2 degundos TOGGLE el modo AP y STA+AP, si se presiona 10 segundos se borra toda la memoria EEPROM y se resetea el dispositivo.

byte PIN_CTRL_VALUE;

#define PUB_HOST "estudiosustenta.myds.me"
// #define PUB_HOST "192.168.1.72"

/***** SET PIN_CTRL TIMER TO PRESS *****/
unsigned long lc_timestamp = millis();
unsigned int  lc_track = 0;
unsigned int  lc_times = 0;
unsigned int  lc_timeout = 10 * 1000; // 10 seconds

// ***************************************





///////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// CONNECTION ///////////////////////////////////////////

HTTPClient http;
WiFiClient wifiClient;

/////////////////////////////////////////////////
/////////////////// HTTP GET ///////////////////

// httpGet("http://192.168.1.80:69/");
String httpGet(String url) {

  String response;

  http.begin(wifiClient, url.c_str());
  
  // Send HTTP GET request
  http.addHeader("X-Auth-Bearer", clid);
  int httpResponseCode = http.GET();
  
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  
  if ( httpResponseCode > 0 ) {
    if ( httpResponseCode >= 200 && httpResponseCode < 300 ) {
      response = http.getString();
    } else {
      response = "";
    }

  } else {
    response = "";
    Serial.print("[HTTP] GET... failed, error: ");
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  // Free resources
  http.end();

  return response;
}

/////////////////////////////////////////////////
/////////////////// HTTP POST ///////////////////

// httpPost("http://192.168.1.80:69/", "application/json", "{\"hola\":\"mundo\"}");
String httpPost(String url, String contentType, String data) {

  String response;

  http.begin(wifiClient, url.c_str());
  
  // Send HTTP GET request
  http.addHeader("Content-Type", contentType);
  http.addHeader("X-Auth-Bearer", clid);
  int httpResponseCode = http.POST(data);
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    response = http.getString();
  } else {
    Serial.print("HTTP Request error: ");
    Serial.println(httpResponseCode);
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    response = String("HTTP Response code: ") + httpResponseCode;
  }
  // Free resources
  http.end();

  return response;
}





///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////// HELPERS /////////////////////////////////////////////

/////////////////////////////////////////////////
//////////////// KEEP-ALIVE LOOP ////////////////

int connId;
String connSecret;
long connTimeout;
bool runAliveLoop = false;

/***** SET TIMEOUT *****/
unsigned long al_timestamp = millis();
/*unsigned int  al_times = 0;*/
unsigned long  al_timeout = 0;
unsigned int  al_track = 0; // al_track = al_timeout; TO: execute function and then start counting

void handleAliveLoop() {

  if (!runAliveLoop) {
    return;
  }

  al_timeout = connTimeout - 3000; // connTimeout - 3 seconds

  if ( millis() != al_timestamp ) { // "!=" intead of ">" tries to void possible bug when millis goes back to 0
    al_track++;
    al_timestamp = millis();
  }

  if ( al_track > al_timeout ) {
    // DO TIMEOUT!
    /*al_times++;*/
    al_track = 0;

    // RUN THIS FUNCTION!
    String response = httpPost(String("http://") + SUB_HOST + ":" + SUB_PORT + "/alive", "application/json", String("{\"connid\":") + connId + ",\"secret\":\"" + connSecret + "\"}");

    ///// Deserialize JSON. from: https://arduinojson.org/v6/assistant

    // Stream& response;

    StaticJsonDocument<96> doc;

    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      runAliveLoop = false;
      return;
    }

    int connid = doc["connid"]; // 757
    const char* secret = doc["secret"]; // "zbUIE"
    long timeout = doc["timeout"]; // 300000

    ///// End Deserialize JSON

    connId = connid;
    connSecret = (String)secret;
    connTimeout = timeout;
    runAliveLoop = true;


    updateControllerData();
  }

}

/////////////////////////////////////////////////
//////////////// DETECT CHANGES ////////////////

bool changed(byte gotStat, byte &compareVar) {
  if(gotStat != compareVar){
    /*
    Serial.print("CHANGE!. Before: ");
    Serial.print(compareVar);
    Serial.print(" | After: ");
    Serial.println(gotStat);
    */
    compareVar = gotStat;
    return true;
  } else {
    /*
    Serial.print("DIDN'T CHANGE... Before: ");
    Serial.print(compareVar);
    Serial.print(" | After: ");
    Serial.println(gotStat);
    */
    //compareVar = gotStat;
    return false;
  }
}

/////////////////////////////////////////////////
//////////// UPDATE CONTROLLER DATA ////////////

void updateControllerData() {
  Serial.print("Updating controller data in server... ");
  Serial.println(httpPost(String("http://") + PUB_HOST + "/controll/controller.php", "application/json", String("{\"controller\":\"") + clid + "\",\"ipv4_interface\":\"" + WiFi.localIP().toString().c_str() + "\"}"));
}

/////////////////////////////////////////////////
//////////// DO DISTANCE MEASUREMENT ////////////

void doMeasurement(bool log = true) {
  // Set the trigger pin LOW for 2uS
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);
 
  // Set the trigger pin HIGH for 20us to send pulse
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(20);
 
  // Return the trigger pin to LOW
  digitalWrite(TRIGPIN, LOW);
 
  // Measure the width of the incoming pulse
  duration = pulseIn(ECHOPIN, HIGH);
 
  // Determine distance from duration
  // Use 343 metres per second as speed of sound
  // Divide by 1000 as we want millimeters
 
  distance = (duration / 2) * 0.343;
 
  if (log) {
    // Print result to serial monitor
    Serial.print("distance: ");
    Serial.print(distance);
    Serial.println(" mm");
  }

}

/////////////////////////////////////////////////
/////// DO DISTANCE MEASUREMENT AVERAGED ///////

float doAvgMeasurement(int lectures, int msDelay) {

  float avgDist = 0;

  for (int i = 0; i < lectures; i++) {
    doMeasurement();
    avgDist = avgDist + distance;
    delay(msDelay);
  }

  avgDist = avgDist / lectures;

  // Print result to serial monitor
  Serial.print("average of ");
  Serial.print(lectures);
  Serial.print("lectures in ");
  Serial.print(msDelay * lectures);
  Serial.print("ms: ");
  Serial.print(avgDist);
  Serial.println("mm");

  return avgDist;

}

/////////////////////////////////////////////////
////////// DISTANCE TO % OF WATER TANK //////////

float distToPercent(bool log = true) {

  float perc = map(distance, HARD_MIN, HARD_MAX, 0, 100);

  if (log) {
    // Print result to serial monitor
    Serial.print("distance in perc: ");
    Serial.print(perc);
    Serial.println("%");
  }

  return perc;

}

/////////////////////////////////////////////////
/////////////// FUNCTIONS IN LOOP ///////////////

void doInLoop() {

  //Serial.println("loop");

  handleAliveLoop();
  
  wifiConfigLoop();

  serverOTA.handleClient();
  serverConfig.handleClient();

  if ( digitalRead(PIN_CTRL) == 1 ) { // IF PIN_CTRL IS PRESSED
    
    // START TIMER
    if ( millis() != lc_timestamp ) { // "!=" intead of ">" tries to void possible bug when millis goes back to 0
      lc_track++;
      lc_timestamp = millis();
    }

    if ( lc_track > lc_timeout ) {
      // DO TIMEOUT!
      lc_times++;
      lc_track = 0;

      if (lc_times == 1) {
        // ERASE ALL EEPROM MEMORY AND RESTART
        EEPROM_CLEAR();
        ESP.reset();
      }
    }
  }
  
  bool ledCtrlChanged = changed(digitalRead(PIN_CTRL), PIN_CTRL_VALUE);

  if (ledCtrlChanged) {
    
    if (PIN_CTRL_VALUE == 0) { // IF PIN_CTRL WAS RELEASED

      unsigned int pushedTime = lc_track;
      
      lc_times = 0; // RESET COUNTER
      lc_track = 0; // RESET TIMER

      if (pushedTime < 2000) { // IF PIN_CTRL WAS PRESSED LESS THAN 2 SECONDS...
        
        // UPDATE LECTURE IN SERVER
        doMeasurement();
      
        if (WiFi.status() == WL_CONNECTED) {
          Serial.print(httpPost(String("http://") + PUB_HOST + "/controll/res.php?device=" + "tinaco_0001" + "&shout=true&log=manual_measure_on_controller&state_changed=true&record=true", "application/json", "{\"type\":\"measure\", \"data\":" + (String)distToPercent() + "}"));
        }
      }

      if ( (pushedTime >= 2000) && (pushedTime < lc_timeout) ) { // IF PIN_CTRL WAS PRESSED FOR 2 SECONDS BUT LESS THAN THE TIMEOUT...
        // TOGGLE SERVER STATUS
        ESP_AP_TOGGLE(true);
      }
    }
  }



  if ( millis() != me_timestamp ) { // "!=" intead of ">" tries to void possible bug when millis goes back to 0
    me_track++;
    me_timestamp = millis();
  }

  if ( me_track > me_timeout ) {
    // DO TIMEOUT!
    /*me_times++;*/
    me_track = 0;
    doMeasurement(false); // RUN THIS FUNCTION!
  }


  bool dThreshChanged;
  int dThreshCurrRange = (int)(distance / DISTANCE_THRESHOLD);

  if( dThreshCurrRange != distanceRange ){
    distanceRange = dThreshCurrRange;
    dThreshChanged =  true;
  } else {
    dThreshChanged =  false;
  }

  if (dThreshChanged) {
    Serial.println("Distance threshold changed");

    if (distance >= SOFT_MIN) {
      Serial.println("Pump ON!");
      digitalWrite(PUMP_PIN, HIGH);
    }

    if (distance <= SOFT_MAX) {
      Serial.println("Pump OFF");
      digitalWrite(PUMP_PIN, LOW);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(httpPost(String("http://") + PUB_HOST + "/controll/res.php?device=" + "tinaco_0001" + "&shout=true&log=distance_threshold_changed&state_changed=true&record=true", "application/json", "{\"type\":\"measure\", \"data\":" + (String)distToPercent() + "}"));
    }
  }

}

/////////////////////////////////////////////////
//////////////// ON-PARSED LOGIC ////////////////

void onParsed(String line) {

  Serial.print("Got JSON: ");
  Serial.println(line);



  ///// Deserialize JSON. from: https://arduinojson.org/v6/assistant
  
  // Stream& line;

  StaticJsonDocument<0> filter;
  filter.set(true);

  StaticJsonDocument<384> doc;

  DeserializationError error = deserializeJson(doc, line, DeserializationOption::Filter(filter));

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  long long iat = doc["iat"]; // 1659077886875

  const char* ep_requested = doc["ep"]["requested"]; // "@SERVER@"
  const char* ep_emitted = doc["ep"]["emitted"]; // "@SERVER@"

  const char* e_type = doc["e"]["type"]; // "info"

  JsonObject e_detail = doc["e"]["detail"];
  int e_detail_connid = e_detail["connid"]; // 757
  const char* e_detail_secret = e_detail["secret"]; // "zbUIE"
  long e_detail_timeout = e_detail["timeout"]; // 300000
  const char* e_detail_device = e_detail["device"]; // "led_wemos0001"
  int e_detail_whisper = e_detail["whisper"]; // 6058
  const char* e_detail_data = e_detail["data"]; // "OFF"

  ///// End Deserialize JSON



  if (strcmp(e_type, "do_measure") == 0) {
    Serial.print("Servidor solicita hacer medición. Distancia actual: ");
    doMeasurement();
    Serial.print(httpPost(String("http://") + PUB_HOST + "/controll/res.php?device=" + e_detail_device + "&shout=true&log=current_distance_requested&state_changed=true&record=true", "application/json", String("{\"type\":\"change\", \"data\":" + (String)distToPercent() + ", \"whisper\":") + String(e_detail_whisper) + "}"));
  }



  if (strcmp(ep_emitted, "@SERVER@") == 0) {
    connId = e_detail_connid;
    connSecret = (String)e_detail_secret;
    connTimeout = e_detail_timeout;
    runAliveLoop = true;
  }

/* PARA COMPROBAR QUE PASA SI SE REINICIA EL MODULO Y EN EL INTER CAMBIA EL ESTADO (EN WEB)
Serial.println("restarting...");
ESP.restart(); // tells the SDK to reboot, not as abrupt as ESP.reset()
*/

}

/////////////////////////////////////////////////
////////////// ON-CONNECTED LOGIC //////////////

void onConnected() {
  updateControllerData();
}





///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////// SETUP //////////////////////////////////////////////

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  EEPROM.begin(4096);
  Serial.print("...STARTING...");
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT); // Initialize LED_BUILTIN as output
  pinMode(PIN_CTRL, INPUT);     // Initialize as an input // To measure manually and TOGGLE AP/STA+AP MODE (long press)

  // Set pinmodes for sensor connections
  pinMode(ECHOPIN, INPUT);
  pinMode(TRIGPIN, OUTPUT);

  // Relay for pump
  pinMode(PUMP_PIN, OUTPUT);

  setupWifiConfigServer(server, EEPROM_ADDR_CONNECTED_SSID, EEPROM_ADDR_CONNECTED_PASSWORD, AP_SSID, AP_PASSWORD);

  /*** START SERVER ANYWAY XD ***/
  //Serial.println("Starting server anyway xD ...");
  //ESP_AP_STA();
  /******************************/

  setLedModeInverted(true);





  serverConfig.on("/", HTTP_GET, []() {
    serverConfig.sendHeader("Connection", "close");
    serverConfig.send(200, "text/html", "<!DOCTYPE html><html><head><style type=\"text\/css\">* {font-family: \"Trebuchet MS\";}body {margin: 0;padding: 0;background-color: #F1F2F4;}#main {display: flex;flex-direction: column;align-items: center;justify-content: center;}#main * {margin-top: 2em;}img {width: 80%;max-width: 500px;}a {background-color: white;border: 1px solid lightblue;color: lightblue;border-radius: 5px;padding-top: 0.5em;padding-bottom: 0.5em;padding-left: 1em;padding-right: 1em;cursor: pointer;transition: all 0.2s;text-decoration: none;font-weight: bold;}a {box-shadow: 0px 5px 10px 0 rgb(0 0 0 \/ 10%);}<\/style><meta charset=\"utf-8\"><title>Config Sketch<\/title><\/head><body><div id=\"main\"><img src=\"data:image\/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAzMTYuMzQgMjA4LjM4Ij48ZGVmcz48c3R5bGU+LmNscy0xLC5jbHMtM3tmaWxsOiM2NjY7fS5jbHMtMntmaWxsOm5vbmU7c3Ryb2tlOiM2NjY7c3Ryb2tlLW1pdGVybGltaXQ6MTA7fS5jbHMtM3tmb250LXNpemU6MTJweDtmb250LWZhbWlseTpJQk1QbGV4TW9uby1Cb2xkLCBJQk0gUGxleCBNb25vO2ZvbnQtd2VpZ2h0OjcwMDt9PC9zdHlsZT48L2RlZnM+PGcgaWQ9IkxheWVyXzIiIGRhdGEtbmFtZT0iTGF5ZXIgMiI+PGcgaWQ9IkxheWVyXzEtMiIgZGF0YS1uYW1lPSJMYXllciAxIj48cGF0aCBjbGFzcz0iY2xzLTEiIGQ9Ik0xNTUsNTEsMTEzLDIxVjBINDNWMjFMMSw1MUgwVjIwMEgxNTVWNTFaIi8+PGxpbmUgY2xhc3M9ImNscy0yIiB4MT0iMTIzIiB5MT0iMjEiIHgyPSIxOTEuMDkiIHkyPSIyMSIvPjx0ZXh0IGNsYXNzPSJjbHMtMyIgdHJhbnNmb3JtPSJ0cmFuc2xhdGUoMjQ0LjM0IDI1LjE4KSI+MTAwJTwvdGV4dD48dGV4dCBjbGFzcz0iY2xzLTMiIHRyYW5zZm9ybT0idHJhbnNsYXRlKDI0NC4zNCA1NS4xOCkiPlN0b3AgcHVtcDwvdGV4dD48dGV4dCBjbGFzcz0iY2xzLTMiIHRyYW5zZm9ybT0idHJhbnNsYXRlKDI0NC4zNCAxNzQuMTgpIj5TdGFydCBwdW1wPC90ZXh0Pjx0ZXh0IGNsYXNzPSJjbHMtMyIgdHJhbnNmb3JtPSJ0cmFuc2xhdGUoMjQ0LjM0IDIwNC4xOCkiPjAlPC90ZXh0PjxwYXRoIGNsYXNzPSJjbHMtMSIgZD0iTTE5OC4xMSwyMy4wOXYtLjcyaDEuMTNWMTkuNTZoLS4wNmwtLjg1LDEuMTItLjU3LS40NSwxLTEuMzJoMS4zOHYzLjQ2SDIwMXYuNzJaIi8+PGNpcmNsZSBjbGFzcz0iY2xzLTIiIGN4PSIxOTkuMzkiIGN5PSIyMSIgcj0iNC4yNCIvPjxwYXRoIGNsYXNzPSJjbHMtMSIgZD0iTTIwMC45NCw1My4xM2gtM3YtLjgybDEuMzItMS4xYy40Ni0uMzkuNjQtLjY1LjY0LTF2LS4wNmEuNTEuNTEsMCwwLDAtLjU3LS41My42OS42OSwwLDAsMC0uNjkuNTlsLS43OS0uM2ExLjUzLDEuNTMsMCwwLDEsMS41Ny0xLjA3LDEuMjcsMS4yNywwLDAsMSwxLjQ0LDEuMjZjMCwuNy0uNSwxLjE1LTEuMTMsMS42M2wtLjc5LjYxaDJaIi8+PGNpcmNsZSBjbGFzcz0iY2xzLTIiIGN4PSIxOTkuMzkiIGN5PSI1MSIgcj0iNC4yNCIvPjxwYXRoIGNsYXNzPSJjbHMtMSIgZD0iTTE5OS4yOSwxNjkuNTZjLjQ2LDAsLjY3LS4yLjY3LS40NXYtLjA1YzAtLjMtLjIyLS40OS0uNTktLjQ5YTEsMSwwLDAsMC0uODYuNDlsLS41Ny0uNTJhMS42NywxLjY3LDAsMCwxLDEuNDctLjdjLjksMCwxLjQ5LjQzLDEuNDksMS4xYS45NC45NCwwLDAsMS0uODEuOTV2MGExLDEsMCwwLDEsLjg5LDFjMCwuNzUtLjY0LDEuMjMtMS42MywxLjIzYTEuNjksMS42OSwwLDAsMS0xLjU1LS44bC42Ni0uNTFhMSwxLDAsMCwwLC45LjU4Yy40MywwLC42OC0uMjEuNjgtLjU2djBjMC0uMzQtLjI5LS41Mi0uNzUtLjUyaC0uMzl2LS43NVoiLz48Y2lyY2xlIGNsYXNzPSJjbHMtMiIgY3g9IjE5OS4zOSIgY3k9IjE3MCIgcj0iNC4yNCIvPjxwYXRoIGNsYXNzPSJjbHMtMSIgZD0iTTE5OS42MywyMDIuMDl2LS44aC0xLjg4di0uNzdsMS42Ni0yLjYxaDEuMDh2Mi42OEgyMDF2LjdoLS41NHYuOFptLTEuMTYtMS41aDEuMTZ2LTEuNzVoMFoiLz48Y2lyY2xlIGNsYXNzPSJjbHMtMiIgY3g9IjE5OS4zOSIgY3k9IjIwMCIgcj0iNC4yNCIvPjxsaW5lIGNsYXNzPSJjbHMtMiIgeDE9IjIzMy43OCIgeTE9IjUxIiB4Mj0iMjA3LjY5IiB5Mj0iNTEiLz48bGluZSBjbGFzcz0iY2xzLTIiIHgxPSIyMzMuNzgiIHkxPSIyMSIgeDI9IjIwNy42OSIgeTI9IjIxIi8+PGxpbmUgY2xhc3M9ImNscy0yIiB4MT0iMjMzLjc4IiB5MT0iMTcwIiB4Mj0iMjA3LjY5IiB5Mj0iMTcwIi8+PGxpbmUgY2xhc3M9ImNscy0yIiB4MT0iMjMzLjc4IiB5MT0iMjAwIiB4Mj0iMjA3LjY5IiB5Mj0iMjAwIi8+PGxpbmUgY2xhc3M9ImNscy0yIiB4MT0iMTY1IiB5MT0iNTEiIHgyPSIxOTEuMDkiIHkyPSI1MSIvPjxsaW5lIGNsYXNzPSJjbHMtMiIgeDE9IjE2NSIgeTE9IjE3MCIgeDI9IjE5MS4wOSIgeTI9IjE3MCIvPjxsaW5lIGNsYXNzPSJjbHMtMiIgeDE9IjE2NSIgeTE9IjIwMCIgeDI9IjE5MS4wOSIgeTI9IjIwMCIvPjwvZz48L2c+PC9zdmc+\"><a href=\"\/set-hard_max\">1) Set current level as 100%<\/a><a href=\"\/set-soft_max\">2) Stop pump at the current level<\/a><a href=\"\/set-soft_min\">3) Start pump at the current level<\/a><a href=\"\/set-hard_min\">4) Set current level as 0%<\/a><\/div><\/body><\/html>");
  });

  serverConfig.on("/set-hard_max", HTTP_GET, []() {
    HARD_MAX = doAvgMeasurement(10, 100);
    serverConfig.sendHeader("Location", String("/"), true); // Redirecto to home /
    serverConfig.send ( 302, "text/plain", "");
  });

  serverConfig.on("/set-soft_max", HTTP_GET, []() {
    SOFT_MAX = doAvgMeasurement(10, 100);
    serverConfig.sendHeader("Location", String("/"), true); // Redirecto to home /
    serverConfig.send ( 302, "text/plain", "");
  });

  serverConfig.on("/set-soft_min", HTTP_GET, []() {
    SOFT_MIN = doAvgMeasurement(10, 100);
    serverConfig.sendHeader("Location", String("/"), true); // Redirecto to home /
    serverConfig.send ( 302, "text/plain", "");
  });

  serverConfig.on("/set-hard_min", HTTP_GET, []() {
    HARD_MIN = doAvgMeasurement(10, 100);
    serverConfig.sendHeader("Location", String("/"), true); // Redirecto to home /
    serverConfig.send ( 302, "text/plain", "");
  });

  serverConfig.begin();



  serverOTA.on("/", HTTP_GET, []() {

    // /*
    Serial.print("ESP.getBootMode(): ");
    Serial.println(ESP.getBootMode());
    Serial.print("ESP.getSdkVersion(): ");
    Serial.println(ESP.getSdkVersion());
    Serial.print("ESP.getBootVersion(): ");
    Serial.println(ESP.getBootVersion());
    Serial.print("ESP.getChipId(): ");
    Serial.println(ESP.getChipId());
    Serial.print("ESP.getFlashChipSize(): ");
    Serial.println(ESP.getFlashChipSize());
    Serial.print("ESP.getFlashChipRealSize(): ");
    Serial.println(ESP.getFlashChipRealSize());
    Serial.print("ESP.getFlashChipSizeByChipId(): ");
    Serial.println(ESP.getFlashChipSizeByChipId());
    Serial.print("ESP.getFlashChipId(): ");
    Serial.println(ESP.getFlashChipId());
    Serial.print("ESP.getFreeHeap(): ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("ESP.getSketchSize(): ");
    Serial.println(ESP.getSketchSize());
    Serial.print("ESP.getFreeSketchSpace(): ");
    Serial.println(ESP.getFreeSketchSpace());
    Serial.print("ESP.getSketchMD5(): ");
    Serial.println(ESP.getSketchMD5());
    // */

    serverOTA.sendHeader("Connection", "close");
    serverOTA.send(200, "text/html", serverIndex);
  });
  serverOTA.on("/update", HTTP_POST, []() {
    serverOTA.sendHeader("Connection", "close");
    serverOTA.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = serverOTA.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
        serverOTA.send(200, "text/plain", "ERROR: Sketch too big?");
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
        serverOTA.send(200, "text/plain", "ERROR: Error writing sketch");
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success!\nNew sketch using: %u\n bytes.\nRebooting...\n", upload.totalSize);
        serverOTA.send(200, "text/plain", "OK");
      } else {
        Update.printError(Serial);
        serverOTA.send(200, "text/plain", "ERROR: No update.end() ?");
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });
  serverOTA.begin();

}





///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////// LOOP //////////////////////////////////////////////

void loop() {
  // put your main code here, to run repeatedly:

  handleNoPollSubscription(sub_WiFiclient, SUB_HOST, SUB_PORT, SUB_PATH, "POST", String("{\"clid\":\"") + clid + "\",\"ep\":[\"controll/tinaco/" + clid + "/req\"]}", "tinaco/2022", doInLoop, onConnected, onParsed);
  
}






