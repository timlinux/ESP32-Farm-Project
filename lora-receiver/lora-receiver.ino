#include <dummy.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "heltec.h" 
#include "images.h"
#include "EspMQTTClient.h"
//#include <WireGuard.hpp>
//#include <WireGuard-ESP32.h>
#include "esp_log.h"
//https://demo-dijiudu.readthedocs.io/en/stable/api-reference/system/log.html
//esp_log_level_set("*", ESP_LOG_DEBUG);

#include "secrets.h"

#define BAND    868E6  //you can set band here directly,e.g. 868E6,915E6
String rssi = "RSSI --";
String packSize = "--";
String packet ;

/* Read the ESP32 schematics carefully - some GPIO pins are read only so you cannot set them to output mode. */
#define LED_PIN 12
#define INTERNAL_LED_PIN 25

/**
 * For MQTT Setup
 */
EspMQTTClient client(
  wifi_ssid,
  wifi_password,
  broker_ip, // MQTT Broker server ip
  broker_user,   // User Can be omitted if not needed
  broker_password,   // Pass Can be omitted if not needed
  broker_client_name,      // Client name that uniquely identify your device
  broker_port
);

WebServer server(80);

void logo(){
  Heltec.display->clear();
  Heltec.display->drawXbm(0,5,logo_width,logo_height,logo_bits);
  Heltec.display->display();
}

void setup(void) {
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

  // Start wireguard
  // Local keys generated doing:
  // wg genkey > esp32-privatekey
  // wg pubkey < esp32-privatekey > esp32-publickey
  // Public key to copy to the WG server is:
  // JPXLIEipLp2afY5KQsWseIb477ug7+rmHVwk5nX0QT8=
  Heltec.display->clear();
 /*  
  Heltec.display->drawString(0, 0, "Setting up Wireguard");
  Heltec.display->drawString(0, 10, "Claiming 192.168.6.7...");
  Heltec.display->display();

  wg.begin(
      local_ip,
      private_key,
      remote_ip,
      public_key,
      endpoint_port);
 */
  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  // Optional functionalities of EspMQTTClient
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  client.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  client.enableLastWillMessage("TestClient/lastwill", "I am going offline");  // You can activate the retain flag by setting the third parameter to true

  setupHttpServer();

  delay (1000);
  /* Setup for leds */
  /* Read the ESP32 schematics carefully - some GPIO pins are read only so you cannot set them to output mode. */
  pinMode(INTERNAL_LED_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  blink();
}


void handleRoot() {
  server.send(200, "text/plain", "Last packet received: " + packet);
  
  blink();
}

void handleNotFound() {
  digitalWrite(INTERNAL_LED_PIN, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


void setupHttpServer() {
  
  // HTTP Server end points
  server.on("/", handleRoot);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

}

void loop(void) {
  // For web server
  server.handleClient();
  // For lora receiver not needed if using callback 
  int packetSize = Heltec.LoRa.parsePacket();
  if (packetSize) { loraCallback(packetSize);  }
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
  Heltec.display->drawString(0 , 10 , "Received "+ packSize + " bytes");
  Heltec.display->drawStringMaxWidth(0 , 20 , 128, packet);
  Heltec.display->drawString(0, 30, rssi);    
  if (packet.indexOf("TEMP=") > 0) {
    packet.replace("TEMP=", "");
    Heltec.display->drawString(0, 40, packet);  
    Heltec.display->drawString(0, 50, "Tempii");  

    // Also publish the received message on MQTT
    // You can activate the retain flag by setting the 
    // third parameter to true      
    client.publish("esp32/temperature", packet); 
    delay(1000);
  }
  else if (packet.indexOf("STATUS=") > 0) {
    packet.replace("STATUS=", "");
    Heltec.display->drawString(0, 40, packet);  
    Heltec.display->drawString(0, 50, "Status");      
    // Also publish the received message on MQTT
    // You can activate the retain flag by setting the 
    // third parameter to true  
    client.publish("esp32/status", packet); 
    delay(1000);
  }
  else {
    // Also publish the received message on MQTT
    // You can activate the retain flag by setting the 
    // third parameter to true  
    Heltec.display->drawString(0, 40, packet);  
    Heltec.display->drawString(0, 50, "Other");      
    client.publish("esp32", packet); 
    delay(1000);
  }

  Heltec.display->display();
  
 
}

void loraCallback(int packetSize) {
  packet ="";
  packSize = String(packetSize,DEC);
  for (int i = 0; i < packetSize; i++) { packet += (char) Heltec.LoRa.read(); }
  rssi = "RSSI " + String(Heltec.LoRa.packetRssi(), DEC) ;
  loraDataReceived();
}

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "MQTT...connection established");
  Heltec.display->display();
  /*
  // Subscribe to "mytopic/test" and display received message to Serial
  client.subscribe("esp32/test", [](const String & payload) {
      Heltec.display->clear();
      Heltec.display->drawString(0, 0, "MQTT...message received on esp32/test:");
      Heltec.display->drawString(0, 10, payload);
      Heltec.display->display();
  });
  */

  // Subscribe to "mytopic/geValveStats"
  // if we get this message, we will send a LoRA packet to the
  // esp32 device controlling the valve, asking for it's status.
  client.subscribe("esp32/getValveStatus",[](const String & topic, const String & payload) {
      Heltec.display->clear();
      Heltec.display->drawString(0, 0, "MQTT:" );
      Heltec.display->drawString(0, 10, "esp32/getValveStatus");
      Heltec.display->drawString(0, 20, "... received");
      Heltec.display->drawString(0, 30, "Sending GETSTATUS");
      Heltec.display->drawString(0, 40, "over LoRA");
      Heltec.display->display();   
      // Uncomment for debugging 
      // client.publish("esp32", "Processing get status request.");
      
      // First we send out a broadcast asking 
      // the remote lora device for its status
      // put the radio in idle mode first.
      LoRa.beginPacket();
      LoRa.print("GETSTATUS");
      LoRa.endPacket();
      // In the main loop we will listen for a 
      // message with STATUS= in it and pass that over to MQTT
      blink();      
  });
  
  client.subscribe("esp32/getTemperature",[](const String & topic, const String & payload) {
      Heltec.display->clear();
      Heltec.display->drawString(0, 0, "MQTT:" );
      Heltec.display->drawString(0, 10, "esp32/getTemperature");
      Heltec.display->drawString(0, 20, "... received");
      Heltec.display->display();    
      
      // First we send out a broadcast asking 
      // the remote lora device for its status
      // put the radio in idle mode first.
      LoRa.beginPacket();
      LoRa.print("GETTEMP");
      LoRa.endPacket();
      
      // In the main loop we will listen for a 
      // message with TEMP= in it and pass that over to MQTT
      blink();
      blink();
   
  });  

  // Subscribe to "mytopic/#" and display received message to Serial
  client.subscribe("esp32/wildcard/#", [](const String & topic, const String & payload) {
      Heltec.display->clear();
      Heltec.display->drawString(0, 0, "MQTT...message received on wildcard :");
      Heltec.display->drawString(0, 10, topic);
      Heltec.display->drawString(0, 20, payload);
      Heltec.display->display();
      blink();
  });

  // 
  // Normally you are going to move these to parts of the code that need to publish messages
  // 

  // Publish a message to "mytopic/test"
  //client.publish("esp32/test", "This is a message"); // You can activate the retain flag by setting the third parameter to true
  client.publish("esp32", "ESP Device MQTT setup done"); // You can activate the retain flag by setting the third parameter to true

  // Execute delayed instructions
  /*
  client.executeDelayed(5 * 1000, []() {
    client.publish("esp32/wildcardtest/test123", "This is a message sent 5 seconds later");
  });
  */
}
