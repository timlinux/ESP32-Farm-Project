# ESP32-Farm-Project

IoT project using ESP32, MQTT for managing my farm water and other systems.

Goal for round #1

![image](https://user-images.githubusercontent.com/178003/183320068-6088a241-fabf-45f2-b0a9-09ec879ad76e.png)

## Libraries used:

* [EspMQTTClient](https://github.com/plapointe6/EspMQTTClient) - used for pushing status updates to MQTT and Node Red. Also used to send instructions to the ESP32 devices via the gateway receiver device which is both on WIFI and the LoRa network.
* [PubSubClient](https://pubsubclient.knolleary.net/) - a dependency of the EspMQTTClient library.
* [Heltec_ESP32](https://github.com/HelTecAutomation/Heltec_ESP32) - used to control the ESP32 boards I use (see hardware below).

## Hardware used:

* [Heltec WiFi LoRa 32](https://heltec-automation-docs.readthedocs.io/en/latest/esp32/wifi_lora_32/index.html) - this is a small all in one board with WiFi, LoRa radio, an integrated LCD panel and typical pin outs for an ESP32 device. 
* [24V Power Supply]() - Notes to come
* [1 1/2" Electronic Valve]() - Notes to come. Purchased at my local Leroy Merlin (big hardware chain in Portugal).
* [Single switch relay]() - Notes to come.

## Build notes

You need two (or more) ESP32 devices. One is to act as the Gateway receiver, the other to act as a sender from out on the farm that will be installed next to the valve.

Open each sketch in the Arduino IDE and flash the code to each.


* Setup MQTT https://www.vultr.com/pt/docs/install-mosquitto-mqtt-broker-on-ubuntu-20-04-server/
* Disable MQTT on local only mode: https://techoverflow.net/2021/11/25/how-to-fix-mosquitto-mqtt-local-only-mode-and-listen-on-all-ip-addresses/
* Setup NodeRed (using docker) https://nodered.org/docs/getting-started/docker
* Install NodeRed dashboard node-red-dashboard
* Install [NodeRed flow](nodered/flows.json) - make sure to configure passwords etc for your MQTT broker.



-----

Tim Sutton
tim@kartoza.com
August 2022
