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

constexpr unsigned int magnitude(int x) { return x < 0 ? -x : x; }

void MicLoop() {

  static Filter dc[] = {.0001, .0001, .0001};

  static unsigned int count = 0;
  int raw[3];
  int mic[3];

  auto const now = micros();
  raw[0] = analogRead(32);
  raw[1] = analogRead(33);
  raw[2] = analogRead(34);

  mic[0] = raw[0] - dc[0].feed(raw[0]);
  mic[1] = raw[1] - dc[1].feed(raw[1]);
  mic[2] = raw[2] - dc[2].feed(raw[2]);

  count++;

  static typeof(micros()) firstPulse[3];
  static bool captured[3] = {false, false, false};

  static bool started = false;
  static typeof(micros()) startTime;

  static bool blanking = false;
  static typeof(micros()) blankingStart;
  constexpr typeof(micros()) blankingTime = 100e3;

  constexpr unsigned int threshold = 300;

  if (blanking) {
    if (micros() - blankingStart < blankingTime)
      return;

    blanking = false;
    captured[0] = captured[1] = captured[2] = false;
    started = false;
  }

  if (!captured[0] && magnitude(mic[0]) > threshold) {
    if (!started) {
      started = true;
      startTime = now;
      firstPulse[0] = 0;
    } else {
      firstPulse[0] = now - startTime;
    }
    captured[0] = true;
  }

  if (!captured[1] && magnitude(mic[1]) > threshold) {
    if (!started) {
      started = true;
      startTime = now;
      firstPulse[1] = 0;
    } else {
      firstPulse[1] = now - startTime;
    }
    captured[1] = true;
  }

  if (!captured[2] && magnitude(mic[2]) > threshold) {
    if (!started) {
      started = true;
      startTime = now;
      firstPulse[2] = 0;
    } else {
      firstPulse[2] = now - startTime;
    }
    captured[2] = true;
  }

  if (captured[0] && captured[1] && captured[2]) {
    blanking = true;
    blankingStart = micros();
    if (lastClient)
      lastClient->binary((uint8_t *)firstPulse, sizeof(firstPulse));
  }

  // static auto lastADC = micros();
  // runIntervalMicro(
  //     [&mic, &raw]() {
  //     },
  //     lastADC, 10);

  // static auto lastDebug = millis();
  // runInterval(
  //     [&mic]() {
  //       Serial.print(mic[0]);
  //       Serial.print(",\t");
  //       Serial.print(mic[1]);
  //       Serial.print(",\t");
  //       Serial.print(mic[2]);
  //       Serial.print(",\t");
  //       Serial.println(count);
  //       count = 0;
  //     },
  //     lastDebug, 100);
}

void loop() {
  OTA::loop();

  static auto lastStrip = millis();
  runInterval([]() { strip.Show(); }, lastStrip, 100);

  MicLoop();
}