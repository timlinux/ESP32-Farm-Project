#include <ESP32-Farm-Project-Library.h> // For constants and device definitions

/* For storing last messages sent to the screen.
 *  
 */

String global_message_log[6] = {};
void logo(){
  Heltec.display->clear();
  Heltec.display->drawXbm(0,5,splash_width,splash_height,splash_bits);
  Heltec.display->display();
}

void clearMessages() {
  Heltec.display->clear();
  for (int i=0; i <= 5; i++)
  {
    global_message_log[i] = String(' ');
  }
}

void showMessages() {
  Serial.println("Listing messages:");
  Serial.println("-----------------");
  Heltec.display->clear();
  for (int i=0; i <= 5; i++)
  {
    Heltec.display->drawString(0, i*10, global_message_log[i]);
    Serial.print(i);
    Serial.print(" : ");    
    Serial.println(global_message_log[i]);
  }
  Heltec.display->display();
}

String jsonMessages() {
  String json = String("{\n");
  for (int i=0; i <= 5; i++)
  {
    json += '\"';
    json += i;
    json += '\":\"';
    json += global_message_log[i];
    json += '\",\n';
  }
  json += "\n}";
  Serial.println(json);
  return json;
}

void addMessage(String message, bool show=false) {
  for (int i=5; i >= 1; i--)
  {
    global_message_log[i] = global_message_log[i-1];
  }
  global_message_log[0] = message;
  if ( show ) showMessages(); 
}

void setupMessages() {
  Heltec.display->init();
  //Heltec.display->flipScreenVertically(); 
  Heltec.display->clear();
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->setFont(ArialMT_Plain_10);
  logo();
  delay(2500);
  clearMessages();
  addMessage(DEVICE_TYPE_HELTEC, true);
}
