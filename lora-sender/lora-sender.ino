#include <dummy.h>
#include "heltec.h" 

#define BAND  868E6  //you can set band here directly,e.g. 868E6,915E6

const String GETSTATUS = "GETSTATUS";
const String SETSTATUS = "SETSTATUS=";
int counter = 0;
String packet ;

void setup() {
  //WIFI Kit series V1 not support Vext control
  Heltec.begin(
    true /*DisplayEnable Enable - off to save power*/, 
    true /*Heltec.LoRa Disable*/, 
    true /*Serial Enable*/, 
    true /*PABOOST Enable*/, 
    BAND /*long BAND*/);
  
  Heltec.display->init();
  Heltec.display->flipScreenVertically();  
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Setting up device");
  Heltec.display->display(); 
  delay(1500);
      
  Serial.begin(115200);
  Serial.println("Setting up device");
  // For lora receiver not needed if using callback 
  while (!LoRa.begin(868E6, true)) {
    Serial.print(".");
    delay(500);
  } 
  //LoRa.setTxPower(14,RF_PACONFIG_PASELECT_PABOOST);
  
}

void loop() {

  /** For lora receiver
  We are going to listen for GETSTATUS
  requests from the controller MQTT device
  and when we recieve that we will send 
  the current status (currently just counter for testing).
  **/
  
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "LoRa Message Check");
  Heltec.display->display();
 
  int packetSize = Heltec.LoRa.parsePacket();
  if (packetSize) { loraCallback(packetSize);  }
  if (!packetSize && counter % 30 == 0)
  {
    Heltec.display->drawString(0, 30, "No message");
    /* For debugging on mqtt:
    LoRa.beginPacket();
    LoRa.print("Lora message WC: ");
    LoRa.print("Waiting for request ");
    LoRa.print(counter);
    LoRa.endPacket();
    */
    Serial.print("No messages: ");
    Serial.println(counter);
  }
  counter++;
  //
  // If you make this delay number smaller
  // thank 1 second (1000ms),
  // getstatus messages will not reliable
  // be received or returned...
  //
  delay(1000); 
}

void loraCallback(int packetSize) {
  packet ="";
  for (int i = 0; i < packetSize; i++) { packet += (char) Heltec.LoRa.read(); }
  // Dont make your message too long - there is a short limit to messages
  // in the LoRa protocol....
  LoRa.beginPacket();
  LoRa.print("Got WC message: ");
  LoRa.print(packet);
  LoRa.print(" ");
  LoRa.print(counter);
  LoRa.endPacket();
  Serial.println("--------------------");
  Serial.print("Received a message on counter: ");
  Serial.print(counter);
  Serial.print(" : ");
  Serial.println(packet);
  
  if (packetSize == GETSTATUS.length()) {
    Serial.println("Get Status Request Size Confirmed:");
    Heltec.display->drawString(0, 10, "GetStatus Request Size Received:");
    Heltec.display->drawString(0, 20, packet );
    if(packet.equals(GETSTATUS)){
      Serial.println("Get Status Request Message Confirmed:");
      Heltec.display->drawString(0, 30, "GETSTATUS Found");
      Serial.println("Sending status message via LoRa");
      LoRa.beginPacket();
      LoRa.print(GETSTATUS);
      LoRa.print("=");
      LoRa.print(counter);
      LoRa.endPacket();
    }
    else
    {
      Heltec.display->drawString(0, 30, "GETSTATUS NOT Found");
      LoRa.beginPacket();
      LoRa.print("Not a GETSTATUS Request.");
      LoRa.endPacket();
    }
  }
  Heltec.display->display(); 

  /**
  digitalWrite(25, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(25, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);                       // wait for a second
  **/
}
