#include <Arduino.h>
#include <WiFi.h>

#include "Config.h"
#include "Hardware.h"
#include "Networking.h"
#include "OTA.h"

using namespace Blinken;

// SKETCH BEGIN

void setup() {
  using namespace Config;
  Serial.begin(baud);
  while (!Serial)
    continue;

  Serial.setDebugOutput(true);

  Serial.println("Hello world!");

  while (1) {
    WiFi.begin(ssid, password);
    WiFi.setHostname(hostName);
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
      break;
    Serial.println("Connect failed! Trying alternate...");
    WiFi.disconnect(false);
    delay(1000);
    WiFi.begin(ssidAlt, passwordAlt);
    WiFi.setHostname(hostName);
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
      break;
    Serial.println("Connect failed! Will retry...");
    WiFi.disconnect(false);
    delay(1000);
  }

  Networking::setup();
  OTA::setup();

  Serial.println("Running!");

  strip.Begin();

  for (int i = 0; i < 300; i++) {
    HslColor c(i * 360. / 300, 1, .5);
    strip.SetPixelColor(i, c);
    strip.Show();
    strip.SetPixelColor(i, {0});
  }
  strip.Show();
}

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> Blinken::strip(300, 13);

void runInterval(std::function<void()> func, typeof(millis()) &last,
                 const unsigned int period) {
  const auto now = millis();
  if (now - last < period)
    return;
  last = now;
  func();
}

void runIntervalMicro(std::function<void()> func, typeof(micros()) &last,
                      const unsigned int period) {
  const auto now = micros();
  if (now - last < period)
    return;
  last = now;
  func();
}

class Filter {
  unsigned int running;
  bool initialized = false;
  const double lambda;

public:
  Filter(double const lambda) : lambda(lambda) {}
  int feed(unsigned int next) {
    if (!initialized) {
      initialized = true;
      return running = next;
    }
    return running = next * lambda + running * (1 - lambda);
  }
  inline typeof(running) getValue() { return running; }
};

AsyncWebSocketClient *lastClient = nullptr;

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if (type == WS_EVT_ERROR) {
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(),
                  *((uint16_t *)arg), (char *)data);
  } else if (type == WS_EVT_PONG) {
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len,
                  (len) ? (char *)data : "");
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->opcode != WS_BINARY)
      return;

    static RgbColor c;
    for (unsigned int i = 0; i < len; i++) {
      auto const j = info->index + i;
      switch (j % 3) {
      case 0:
        c.R = data[i];
        break;
      case 1:
        c.G = data[i];
        break;
      case 2:
        c.B = data[i];
        if (j < 900) {
          Blinken::strip.SetPixelColor(j / 3, c);
        }
      }
    }
    if (info->final) {
      Blinken::strip.Show();
    }

    lastClient = client;
  }
}

void loop() {
  OTA::loop();

  static auto lastStrip = millis();
  runInterval([]() { strip.Show(); }, lastStrip, 100);
}