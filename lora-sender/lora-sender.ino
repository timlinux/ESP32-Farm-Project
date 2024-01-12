#include <dummy.h>
#include "heltec.h" 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32-Farm-Project-Library.h> // For constants and device definitions

#define BAND  868E6  //you can set band here directly,e.g. 868E6,915E6

/* Read the ESP32 schematics carefully - some GPIO pins are read only so you cannot set them to output mode. */
#define SENSOR_PIN  21 // ESP32 pin GIOP21 connected to DS18B20 sensor's DQ pin for temp sensor
#define INTERNAL_LED_PIN 25 // onboard LED

OneWire oneWire(SENSOR_PIN);
//DallasTemperature DS18B20(&oneWire);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
float tempC; // temperature in Celsius

String global_lora_packet ;

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

  
  /* Setup for temperature probe */
  pinMode(SENSOR_PIN, INPUT);
  sensors.begin();    // initialize the temperature sensor

  pinMode(INTERNAL_LED_PIN, OUTPUT);
  blink();
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
  //
  // If you make this delay number smaller
  // than 1 second (1000ms),
  // getstatus messages will not reliably
  // be received or returned...
  //
  //delay(1000); 
}

void loraCallback(int packetSize) {
  global_lora_packet ="";
  for (int i = 0; i < packetSize; i++) { global_lora_packet += (char) Heltec.LoRa.read(); }
  // Dont make your message too long - there is a short limit to messages
  // in the LoRa protocol....

  
  Serial.println("--------------------");
  Serial.println(global_lora_packet);
  Heltec.display->drawString(0, 10, "LoRA Request Received:");
  Heltec.display->drawString(0, 20, global_lora_packet );

  
  if (String(global_lora_packet.charAt(0)) == MODE_INSTRUCTION)
  {
    if (String(global_lora_packet.charAt(1)) == SENSOR_STATUS) 
    {
      
      // Wait a second before replying -Why?
      //delay(1000);      
      Serial.println("Sending status message via LoRa");
      String message = MODE_RESPONSE + SENSOR_STATUS + DEVICE_PUMPHOUSE + ':' + STATUS_OK;
      Serial.println(message);  
      
      LoRa.beginPacket();
      LoRa.print(message);
      LoRa.endPacket();
      
      blink();
      //delay(1000); 
    
    }
    else if (String(global_lora_packet.charAt(1)) == SENSOR_TEMPERATURE) 
    {
      // Wait a second before replying
      //delay(1000);      
      String message = MODE_RESPONSE + SENSOR_TEMPERATURE + DEVICE_PUMPHOUSE;
      
      Serial.println("Reading temperature");

      sensors.requestTemperatures();       // send the command to get temperatures
      tempC = sensors.getTempCByIndex(0);  // read temperature i

      Serial.print("Temperature: ");
      Serial.print(tempC);    // print the temperature in °C
      Serial.println("°C");  

      Serial.println("Sending temp message via LoRa");
      message += ':' + String(tempC);
      Serial.println(message);
      LoRa.beginPacket();
      LoRa.print(message);
      LoRa.endPacket();
      
      Heltec.display->drawString(0, 30 , "Temperature ");
      Heltec.display->drawString(0, 40, String(tempC) + "°C");  
      Heltec.display->display();
      blink();
      delay(1000); 
      blink();   
    }  
    else
    {
      // Wait a second before replying
      //delay(1000);
      Heltec.display->drawString(0, 30, "Request NOT Found");
      LoRa.beginPacket();
      LoRa.print("Not a KNOWN Request.");
      LoRa.endPacket();
      blink();
      delay(1000); 
      blink();
      delay(1000); 
      blink();
    }
    Heltec.display->display(); 
  }
  /**
  digitalWrite(25, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(25, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);                       // wait for a second
  **/
}

void blink()
{
  digitalWrite(INTERNAL_LED_PIN, HIGH);
  delay(100);
  digitalWrite(INTERNAL_LED_PIN, LOW);
}
