# ESP-Lights

A simple Platform.IO based program to run on an ESP32 (and possibly ESP8266) to control addressable LED string lights over WiFi.

## Hacky

I hacked this together in like a day.

I started with an example Arduino compatible OTA enabled program from Platform.IO and added WS281x control and a websocket and just strung the pieces together.
