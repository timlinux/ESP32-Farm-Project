#include <dummy.h>
#include "heltec.h" 
#include <OneWire.h>
#include <DallasTemperature.h>

#define BAND  868E6  //you can set band here directly,e.g. 868E6,915E6

/* Read the ESP32 schematics carefully - some GPIO pins are read only so you cannot set them to output mode. */
#define SENSOR_PIN  21 // ESP32 pin GIOP21 connected to DS18B20 sensor's DQ pin for temp sensor
#define INTERNAL_LED_PIN 25 // onboard LED

OneWire oneWire(SENSOR_PIN);
//DallasTemperature DS18B20(&oneWire);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
float tempC; // temperature in Celsius

const String GETSTATUS = "GETSTATUS";
const String SENDER_STATUS = "STATUS=";
const String GETTEMP = "GETTEMP";
const String TEMP = "TEMP=";
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
    Serial.println("Possible get Status Request SIZE Confirmed:");
    Heltec.display->drawString(0, 10, "GetStatus Request Size Received:");
    Heltec.display->drawString(0, 20, packet );
    if(packet.equals(GETSTATUS)){
      // Wait a second before replying
      delay(1000);      
      Serial.println("Get Status Request Message CONFIRMED:");
      Heltec.display->drawString(0, 30, "GETSTATUS Found");
      Serial.println("Sending status message via LoRa");
      LoRa.beginPacket();
      LoRa.print(SENDER_STATUS);
      LoRa.print("=");
      LoRa.print(counter);
      LoRa.endPacket();
      blink();
      delay(1000); 
    }
  }
  else if (packetSize == GETTEMP.length()) {
    Serial.println("Possible get Temp Request SIZE Confirmed:");
    Heltec.display->drawString(0, 10, "GetTemp Request Size Received:");
    Heltec.display->drawString(0, 20, packet );
    if(packet.equals(GETTEMP)){
      // Wait a second before replying
      delay(1000);
      Serial.println("Get Status Request Message CONFIRMED:");
      
      Heltec.display->clear();
      Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
      Heltec.display->setFont(ArialMT_Plain_10);
      Heltec.display->drawString(0, 30, "GETTEMP Found");

      Serial.println("Reading temperature");

      sensors.requestTemperatures();       // send the command to get temperatures
      tempC = sensors.getTempCByIndex(0);  // read temperature i

      Serial.print("Temperature: ");
      Serial.print(tempC);    // print the temperature in °C
      Serial.println("°C");  

      Serial.println("Sending temp message via LoRa");
      
      LoRa.beginPacket();
      LoRa.print(TEMP);
      LoRa.print("=");
      LoRa.print(tempC);
      LoRa.endPacket();
      
      Heltec.display->drawString(0, 30 , "Temperature ");
      Heltec.display->drawString(0, 40, String(tempC) + "°C");  
      Heltec.display->display();
      blink();
      delay(1000); 
      blink();   
    }  
  }  
  else
  {
      // Wait a second before replying
      delay(1000);
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
