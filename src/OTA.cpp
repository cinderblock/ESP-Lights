
#include <ArduinoOTA.h>

#include "Config.h"
#include "Networking.h"
#include "OTA.h"


void Blinken::OTA::setup() {
  // Send OTA events to the browser
  ArduinoOTA.onStart(
      []() { Blinken::Networking::events.send("Update Start", "ota"); });

  ArduinoOTA.onEnd(
      []() { Blinken::Networking::events.send("Update End", "ota"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "Progress: %u%%\n", (progress / (total / 100)));
    Blinken::Networking::events.send(p, "ota");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR)
      Blinken::Networking::events.send("Auth Failed", "ota");
    else if (error == OTA_BEGIN_ERROR)
      Blinken::Networking::events.send("Begin Failed", "ota");
    else if (error == OTA_CONNECT_ERROR)
      Blinken::Networking::events.send("Connect Failed", "ota");
    else if (error == OTA_RECEIVE_ERROR)
      Blinken::Networking::events.send("Recieve Failed", "ota");
    else if (error == OTA_END_ERROR)
      Blinken::Networking::events.send("End Failed", "ota");
  });

  ArduinoOTA.setHostname(Config::hostName);
  ArduinoOTA.begin();
}

void Blinken::OTA::loop() { ArduinoOTA.handle(); }