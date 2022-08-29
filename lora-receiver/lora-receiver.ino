#include "heltec.h"            // Board specific library for our ESP32 Device
// I think we can get rid of this
// as I think it was part of the implementation I was 
// trying to do for wireguard
//#include <ESPmDNS.h>           // To resolve hostnames via DNS - check if used
#include <OneWire.h>           // For temp sensor
#include <DallasTemperature.h> // For temp sensor
#include <WebServer.h>         // Implements a simple HTTP Server
#include "EspMQTTClient.h"     // For sending and receiving messages over MQTT
#include <WiFi.h>              // For connecting to the local WIFI network
#include <WiFiClient.h>        // For connecting to the local WIFI network
#include "esp_log.h"           // Check if still used
#include "secrets.h"           // All passwords etc. should be stored here
#include "splash.h"            // For displaying a logo on start - edit in GIMP if needed
#include "messages.h"          // For logging recent messages to display on screen
#include <ESP32-Farm-Project-Library.h> // For constants and device definitions

#define THIS_DEVICE = DEVICE_MASTER;



/*
 Interrupt handlers setup
 See https://www.visualmicro.com/page/Timer-Interrupts-Explained.aspx 
 for logic of interrupt handlers
*/
volatile int global_current_interrupts;
int global_total_interrupts;
int global_last_interrupts;
hw_timer_t * global_timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

/*
  Interrupt callback
  Don't do any sleep or serial printing in
  here because it needs to be highly performant.
*/
void IRAM_ATTR timer_interrupt_handler() {
  portENTER_CRITICAL_ISR(&timerMux);
  global_current_interrupts++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

/* 
Lora setup: The band you should use is region specific. In europe we need to
use 868E6, but if you are in a different region you will need to research
online what the appropriate band code is. 
*/
#define BAND    868E6  
String global_lora_rssi = "RSSI --";
String global_lora_packet_size = "--";
String global_lora_packet ;

/* 
Read the ESP32 schematics carefully - some GPIO pins are read only so you
cannot set them to output mode. This can vary from device to device, so read
the manufacturer specs carefully then use an appropriate pin. 

ESP32 pin GIOP21 connected to DS18B20 sensor's DQ pin for temp sensor.
*/
#define SENSOR_PIN  21 
#define LED_PIN 12
#define INTERNAL_LED_PIN 25

OneWire oneWire(SENSOR_PIN);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
float global_temp_c; // temperature in Celsius

/**
 * For MQTT Setup
 */
EspMQTTClient client(
  wifi_ssid,
  wifi_password,
  broker_ip, // MQTT Broker server ip
  broker_user, // User Can be omitted if not needed
  broker_password, // Pass Can be omitted if not needed
  broker_client_name, // Client name that uniquely identify your device
  broker_port // MQTT port to use
);

WebServer global_web_server(80);

void setup(void) {
  Serial.begin(115200);
  /* Interrupts setup - we will measure temp every minute **/
  // Configure Prescaler to 80, as our timer runs @ 80Mhz
  // Giving an output of 80,000,000 / 80 = 1,000,000 ticks / second
  global_timer = timerBegin(0, 80, true);                
  timerAttachInterrupt(global_timer, &timer_interrupt_handler, true);    
  // Fire Interrupt every 60m ticks, so 60s
  timerAlarmWrite(global_timer, 60000000, true);      
  timerAlarmEnable(global_timer);
  global_last_interrupts = 0;

  // LoRa setup
  Heltec.begin(
    true /*DisplayEnable Enable*/, 
    true /*Heltec.Heltec.Heltec.LoRa Disable*/, 
    true /*Serial Enable*/, 
    true /*PABOOST Enable*/, 
    BAND /*long BAND*/);
  LoRa.setTxPower(14,RF_PACONFIG_PASELECT_PABOOST);
  
  setupMessages();
  addMessage("Heltec.LoRa Initial success!");
  addMessage("Setting up device...");
  
  delay(1000);
  
  digitalWrite(INTERNAL_LED_PIN, 0);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  addMessage("Connecting to wifi...", true);
  
  // Wait for connection
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    addMessage(wifi_ssid);
    addMessage(wifi_password);
    addMessage(String(attempt), true);
    attempt += 1;
  }
  
  addMessage("Connected...");
  addMessage(wifi_ssid);
  addMessage(WiFi.localIP().toString().c_str(), true);
  

  // Set the time
  configTime(9 * 60 * 60, 0, "pt.pool.ntp.org", "time.google.com");

  // I think we can get rid of this
  // as I think it was part of the implementation I was 
  // trying to do for wireguard
  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  // Optional functionalities of EspMQTTClient
  // Enable debugging messages sent to serial output
  client.enableDebuggingMessages(); 
  /* 
    Enable the web updater. 
    User and password default to values of MQTTUsername and MQTTPassword. 
    These can be overridded with enableHTTPWebUpdater("user", "password").
  */
  client.enableHTTPWebUpdater(); 
  /*
     Enable OTA (Over The Air) updates. 
     Password defaults to MQTTPassword. 
     Port is the default OTA port. 
     Can be overridden with enableOTA("password", port).
  */
  client.enableOTA(); 
  client.enableLastWillMessage(
    "esp32/lastwill", "Hub going offline..."); 

  setupHttpServer();

  delay (1000);
  /* Setup for temperature probe */
  pinMode(SENSOR_PIN, INPUT);
  // initialize the temperature sensor  
  sensors.begin();    
  /* Setup for leds
   Read the ESP32 schematics carefully - some GPIO pins are read only so you
   cannot set them to output mode. */
  pinMode(INTERNAL_LED_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  blink();
}

/* This is a handler for an on-device temperature sensor on the controller. */
void readTemperature() {
      addMessage("Reading temperature on hub.");
      sensors.requestTemperatures();       // send the command to get temperatures
      global_temp_c = sensors.getTempCByIndex(0);  // read temperature i

      addMessage("Sending temp message via LoRa");
      
      LoRa.beginPacket();
      LoRa.print("INSIDETEMP=");
      LoRa.print(global_temp_c);
      LoRa.endPacket();
      
      addMessage("Temperature ");
      addMessage( String(global_temp_c) + "°C", true);  

}

void handleRoot() {
  
  global_web_server.send(
    200, "application/json" , jsonMessages());
  
  blink();
}

void handleNotFound() {
  digitalWrite(INTERNAL_LED_PIN, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += global_web_server.uri();
  message += "\nMethod: ";
  message += (global_web_server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += global_web_server.args();
  message += "\n";
  for (uint8_t i = 0; i < global_web_server.args(); i++) {
    message += " " + global_web_server.argName(i) + ": " + global_web_server.arg(i) + "\n";
  }
  global_web_server.send(404, "text/plain", message);
}


void setupHttpServer() {
  
  // HTTP Server end points
  global_web_server.on("/", handleRoot);

  global_web_server.on("/inline", []() {
    global_web_server.send(200, "text/plain", "this works as well");
  });

  global_web_server.onNotFound(handleNotFound);

  global_web_server.begin();
  addMessage("HTTP server started", true);

}

void loop(void) {

  if (global_current_interrupts > 0) {
    portENTER_CRITICAL(&timerMux);
    global_current_interrupts--;
    portEXIT_CRITICAL(&timerMux);
    global_total_interrupts++;
    // TODO: get rid of this, it is just for debugging
    if (!global_total_interrupts % 1000){
    	addMessage("global_total_interrupts");
      addMessage(String(global_total_interrupts), true);
    }
  }

  if (global_last_interrupts < global_total_interrupts) {
    readTemperature();
    global_total_interrupts = global_current_interrupts;
  }


  
  // For web server
  global_web_server.handleClient();
  // For lora receiver not needed if using callback 
  int packet_size = Heltec.LoRa.parsePacket();
  if (packet_size) { loraCallback(packet_size);  }
  delay(10);
  // For MQTT client
  client.loop();
  /*
  int touchValue = touchRead(2);
  if (touchValue > 104)
  {
    client.publish("esp32/touchValue", String(touchValue));
    blink();
  }
  */
}

void blink()
{
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(INTERNAL_LED_PIN, HIGH);
  delay(10);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(INTERNAL_LED_PIN, LOW);
}

void loraDataReceived(){
  addMessage("Received "+ global_lora_packet_size + " bytes");
  addMessage(global_lora_packet);
  addMessage(global_lora_rssi);    
  if (global_lora_packet.indexOf("TEMP=") > 0) {
    global_lora_packet.replace("TEMP=", "");
    addMessage(global_lora_packet);  
    addMessage("Temp", true);  

    // Also publish the received message on MQTT
    // You can activate the retain flag by setting the 
    // third parameter to true      
    client.publish("esp32/temperature", global_lora_packet); 
    delay(1000);
  }
  else if (global_lora_packet.indexOf("STATUS=") > 0) {
    global_lora_packet.replace("STATUS=", "");
    addMessage(global_lora_packet);  
    addMessage("Status", true);      
    // Also publish the received message on MQTT
    // You can activate the retain flag by setting the 
    // third parameter to true  
    client.publish("esp32/status", global_lora_packet); 
    delay(1000);
  }
  else {
    // Also publish the received message on MQTT
    // You can activate the retain flag by setting the 
    // third parameter to true  
    addMessage(global_lora_packet);  
    addMessage("Other", true);      
    client.publish("esp32", global_lora_packet); 
    delay(1000);
  }

}

void loraCallback(int packet_size) {
  global_lora_packet ="";
  global_lora_packet_size = String(packet_size,DEC);
  for (int i = 0; i < packet_size; i++) { 
	global_lora_packet += (char) Heltec.LoRa.read(); 
  }
  global_lora_rssi = "RSSI " + String(Heltec.LoRa.packetRssi(), DEC) ;
  loraDataReceived();
}

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{
  addMessage("MQTT...connection established", true);

  /* Subscribe to "esp32/getStatus" on MQTT. If we get this message, we will
     send a LoRA packet to all esp32 devices asking them to return a message
     with their status. */
  client.subscribe("esp32/getValveStatus",[](const String & topic, const String & payload) {
      
      addMessage("IN:MQTT:esp32/getStatus");
      addMessage("OUT:LoRA:GETSTATUS", true);
      // Uncomment for debugging 
      // client.publish("esp32", "Processing get status request.");
      
      // Send out a broadcast asking the remote 
      // LoRA devices for their status.
      LoRa.beginPacket();
      LoRa.print("GETSTATUS");
      LoRa.endPacket();
      // In the main loop we will listen for any response
      // message with STATUS= in it and pass that over to MQTT
      blink();      
  });
  
  /* Subscribe to "esp32/getTemperature" on MQTT If we get this message, we
     will send a LoRA packet to all esp32 devices asking them to return a
     message with their temperature if they are able to do that. */
  client.subscribe("esp32/getTemperature",[](const String & topic, const String & payload) {
      
      addMessage("IN:MQTT:esp32/getTemperature");
      addMessage("OUT:LoRA:GETTEMP", true);
            
      /* First we send out a broadcast asking the remote lora device for its
         status put the radio in idle mode first. */
      LoRa.beginPacket();
      LoRa.print("GETTEMP");
      LoRa.endPacket();
      
      // In the main loop we will listen for a 
      // message with TEMP= in it and pass that over to MQTT
      // Two blinks means getTemp
      blink();
      blink();
   
  });  


  client.subscribe("esp32/getIP",[](const String & topic, const String & payload) {
      addMessage("IN:MQTT:esp32/getIP");
      addMessage("OUT:LoRA:" + WiFi.localIP().toString());
      client.publish("esp32/ip", WiFi.localIP().toString());
  });  
  

  // TODO: Decide if we really want wildcard handling here.
  // Subscribe to "mytopic/#" and display received message to Serial
  client.subscribe("esp32/wildcard/#", [](const String & topic, const String & payload) {
      addMessage("IN:MQTT:wildcard");
      addMessage(topic);
      addMessage(payload, true);
      // Three blinks means wildcard message
      blink();
      blink();
      blink();
  });

  // You can activate the retain flag by setting the third parameter to true
  client.publish("esp32", "ESP Hub Device Online");

  // Execute delayed instructions
  /*
  client.executeDelayed(5 * 1000, []() {
    client.publish("esp32/wildcardtest/test123", "This is a message sent 5 seconds later");
  });
  */
}
