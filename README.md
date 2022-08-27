# ESP32-Farm-Project

IoT project using ESP32, MQTT for managing my farm water and other systems.

Goal for round #1

![image](https://user-images.githubusercontent.com/178003/183320068-6088a241-fabf-45f2-b0a9-09ec879ad76e.png)

## Libraries used:

* [EspMQTTClient](https://github.com/plapointe6/EspMQTTClient) - used for pushing status updates to MQTT and Node Red. Also used to send instructions to the ESP32 devices via the gateway receiver device which is both on WIFI and the LoRa network.
* [PubSubClient](https://pubsubclient.knolleary.net/) - a dependency of the EspMQTTClient library.
* [Heltec_ESP32](https://github.com/HelTecAutomation/Heltec_ESP32) - used to control the ESP32 boards I use (see hardware below).
* [OneWire](https://www.pjrc.com/teensy/td_libs_OneWire.html) - For temperature sensor
* [Dallas](https://github.com/milesburton/Arduino-Temperature-Control-Library) - For temperature sensor

## Hardware used:

* [Heltec WiFi LoRa 32](https://heltec-automation-docs.readthedocs.io/en/latest/esp32/wifi_lora_32/index.html) - this is a small all in one board with WiFi, LoRa radio, an integrated LCD panel and typical pin outs for an ESP32 device. 
* [24V Power Supply]() - Notes to come
* [1 1/2" Electronic Valve]() - Notes to come. Purchased at my local Leroy Merlin (big hardware chain in Portugal).
* [Single switch relay]() - Notes to come.

## Build notes

You need two (or more) ESP32 devices. One is to act as the Gateway receiver, the other to act as a sender from out on the farm that will be installed next to the valve.

Open each sketch in the Arduino IDE and flash the code to each.


* Setup NodeRed (using docker) https://nodered.org/docs/getting-started/docker
* Install NodeRed dashboard node-red-dashboard
* Install [NodeRed flow](nodered/flows.json) - make sure to configure passwords etc for your MQTT broker.

# Home Server Setup

To go with the project I set up a small, lower power NUC device with Ubuntu Linux Server minimal install and installed the following:

## Adguard Home 

``sudo snap install adguard-home`` and [how to resolve bind in use](https://github.com/AdguardTeam/AdGuardHome/wiki/FAQ#bindinuse). After installing, the setup page is at http://host:3000

## Tailscale 

I created a VPN between my machines so I can look at my home node-red dashboards without publishing them online. Just go to the tailscale website and follow their very clear setup processes.

## Node-Red

> Do not use the snap package, fonts are broken for the graph-image node.

sudo apt install npm
sudo npm cache clean -f
sudo npm install -g n
sudo n stable

(Above steps from https://askubuntu.com/a/480642)

To start run:

> node-red

Then install these nodes:

node-red-dashboard
@studiobox/node-red-contrib-ui-widget-thermometer
node-red-contrib-chart-image
node-red-contrib-ui-media
node-red-contrib-telegrambot


For charts to render I had to install these:

> sudo apt-get install build-essential libcairo2-dev libpango1.0-dev libjpeg-dev libgif-dev librsvg2-dev
> npm install canvas

See https://discourse.nodered.org/t/solved-using-node-red-contrib-chart-image/35300/7

## Mosquitto MQTT server

```
sudo apt install mosquitto mosquitto-clients
```

Summary of process:

* Setup MQTT authentigation as per [this tutorial](https://www.vultr.com/pt/docs/install-mosquitto-mqtt-broker-on-ubuntu-20-04-server/)
* Disable MQTT on local only mode as per [this tutorial](https://techoverflow.net/2021/11/25/how-to-fix-mosquitto-mqtt-local-only-mode-and-listen-on-all-ip-addresses/)

These steps do all from above tutorials:

/etc/mosquitto/mosquitto.conf :

```
#Place your local configuration in /etc/mosquitto/conf.d/
#
# A full description of the configuration file is at
# /usr/share/doc/mosquitto/examples/mosquitto.conf.example

# Added by Tim to listen on public port too
bind_address 0.0.0.0

pid_file /run/mosquitto/mosquitto.pid

persistence true
persistence_location /var/lib/mosquitto/

log_dest file /var/log/mosquitto/mosquitto.log

include_dir /etc/mosquitto/conf.d

```

/etc/mosquitto/conf.d/default.conf


```
allow_anonymous false
password_file /etc/mosquitto/passwd
```


```
sudo vim /etc/mosquitto/passwd
```

Create a user:password entry with a strong password. Then do

```
sudo mosquitto_passwd -U /etc/mosquitto/passw
```

Restart mosquitto

```
sudo systemctl mosquitto restart
```

Test client in Terminal 1:

```
mosquitto_sub -u tim -P "XXXXXXXXXXXXXXXXXXXX" -h localhost -t 'esp32/example' -v
```

Test server in Terminal 2:

```
mosquitto_pub -u tim -P "XXXXXXXXXXXXXXXXXXXX" -h localhost -t 'esp32/example' -v -m "hello"
```



## PostgreSQL


sudo apt install postgresql-14-postgis-3






-----

Tim Sutton
tim@kartoza.com
August 2022
