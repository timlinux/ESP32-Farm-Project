#include <dummy.h>             // Check if still used
#include "heltec.h"            // Board specific library for our ESP32 Device
// I think we can get rid of this
// as I think it was part of the implementation I was 
// trying to do for wireguard
#include <ESPmDNS.h>           // To resolve hostnames via DNS - check if used
#include <OneWire.h>           // For temp sensor
#include <DallasTemperature.h> // For temp sensor
#include <WebServer.h>         // Implements a simple HTTP Server
#include "EspMQTTClient.h"     // For sending and receiving messages over MQTT
#include <WiFi.h>              // For connecting to the local WIFI network
#include <WiFiClient.h>        // For connecting to the local WIFI network
#include "esp_log.h"           // Check if still used
#include "splash.xbm"          // For displaying a logo on start - edit in GIMP if needed
#include "secrets.h"           // All passwords etc. should be stored here

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

void logo(){
  Heltec.display->clear();
  Heltec.display->drawXbm(0,5,splash_width,splash_height,splash_bits);
  Heltec.display->display();
}

void setup(void) {

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
  Heltec.display->init();
  //Heltec.display->flipScreenVertically();  
  Heltec.display->setFont(ArialMT_Plain_10);
  logo();
  delay(1500);
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Heltec.LoRa Initial success!");
  Heltec.display->drawString(0, 10, "Setting up device...");
  Heltec.display->display();
  delay(1000);
  
  digitalWrite(INTERNAL_LED_PIN, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  Heltec.display->drawString(0, 20, "Connecting to wifi...");
  Heltec.display->display();
  // Wait for connection
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Heltec.display->clear();
    Heltec.display->drawString(0, 10, wifi_ssid);
    Heltec.display->drawString(0, 20, wifi_password);
    Heltec.display->drawString(0, 40, String(attempt));
    Heltec.display->display();
    attempt += 1;
  }
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Connected...");
  Heltec.display->drawString(0, 10, wifi_ssid);
  Heltec.display->drawString(0, 20, WiFi.localIP().toString().c_str());
  Heltec.display->display();

  // Set the time
  configTime(9 * 60 * 60, 0, "pt.pool.ntp.org", "time.google.com");

  Heltec.display->clear();
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
      Heltec.display->clear();
      Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
      Heltec.display->setFont(ArialMT_Plain_10);
      Heltec.display->drawString(0, 30, "Reading temperature on hub.");
      Serial.println("Reading temperature");
      sensors.requestTemperatures();       // send the command to get temperatures
      global_temp_c = sensors.getTempCByIndex(0);  // read temperature i

      Serial.print("Temperature: ");
      Serial.print(global_temp_c);    // print the temperature in °C
      Serial.println("°C");  
      Serial.println("Sending temp message via LoRa");
      
      LoRa.beginPacket();
      LoRa.print("INSIDETEMP=");
      LoRa.print(global_temp_c);
      LoRa.endPacket();
      
      Heltec.display->drawString(0, 30 , "Temperature ");
      Heltec.display->drawString(0, 40, String(global_temp_c) + "°C");  
      Heltec.display->display();
  
}

void handleRoot() {
  global_web_server.send(200, "text/plain", "Last packet received: " + global_lora_packet);
  
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
  Serial.println("HTTP server started");

}

void loop(void) {

  if (global_current_interrupts > 0) {
    portENTER_CRITICAL(&timerMux);
    global_current_interrupts--;
    portEXIT_CRITICAL(&timerMux);
    global_total_interrupts++;
    // TODO: get rid of this, it is just for debugging
    if (!global_total_interrupts % 1000){
    	Serial.print("global_total_interrupts");
    	Serial.println(global_total_interrupts);
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
  Heltec.display->clear();
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->drawString(0 , 10 , "Received "+ global_lora_packet_size + " bytes");
  Heltec.display->drawStringMaxWidth(0 , 20 , 128, global_lora_packet);
  Heltec.display->drawString(0, 30, global_lora_rssi);    
  if (global_lora_packet.indexOf("TEMP=") > 0) {
    global_lora_packet.replace("TEMP=", "");
    Heltec.display->drawString(0, 40, global_lora_packet);  
    Heltec.display->drawString(0, 50, "Tempii");  

    // Also publish the received message on MQTT
    // You can activate the retain flag by setting the 
    // third parameter to true      
    client.publish("esp32/temperature", global_lora_packet); 
    delay(1000);
  }
  else if (global_lora_packet.indexOf("STATUS=") > 0) {
    global_lora_packet.replace("STATUS=", "");
    Heltec.display->drawString(0, 40, global_lora_packet);  
    Heltec.display->drawString(0, 50, "Status");      
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
    Heltec.display->drawString(0, 40, global_lora_packet);  
    Heltec.display->drawString(0, 50, "Other");      
    client.publish("esp32", global_lora_packet); 
    delay(1000);
  }

  Heltec.display->display();
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
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "MQTT...connection established");
  Heltec.display->display();

  /* Subscribe to "esp32/getStatus" on MQTT. If we get this message, we will
     send a LoRA packet to all esp32 devices asking them to return a message
     with their status. */
  client.subscribe("esp32/getValveStatus",[](const String & topic, const String & payload) {
      Heltec.display->clear();
      Heltec.display->drawString(0, 0, "IN:MQTT:esp32/getStatus");
      Heltec.display->drawString(0, 10, "OUT:LoRA:GETSTATUS");
      Heltec.display->display();   
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
      Heltec.display->clear();
      Heltec.display->drawString(0, 0, "IN:MQTT:esp32/getTemperature");
      Heltec.display->drawString(0, 10, "OUT:LoRA:GETTEMP");
      Heltec.display->display();    
      
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

  // TODO: Decide if we really want wildcard handling here.
  // Subscribe to "mytopic/#" and display received message to Serial
  client.subscribe("esp32/wildcard/#", [](const String & topic, const String & payload) {
      Heltec.display->clear();
      Heltec.display->drawString(0, 0, "IN:MQTT:wildcard");
      Heltec.display->drawString(0, 10, topic);
      Heltec.display->drawString(0, 20, payload);
      Heltec.display->display();
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
