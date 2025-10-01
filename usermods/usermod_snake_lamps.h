#pragma once
#include "wled.h"

/*
  usermod_snake_lamps.h
  Single snake across 3 lamps (6 segments total).
  Master/slave sync via UDP. Only 1 snake at a time.
  Background = dim white, snake = random color.
*/

#define SNAKE_LEN 86
#define SNAKE_BRI_PCT 80
#define BACKGROUND_WHITE_PCT 35
#define FRAME_MS 40       // animation speed
#define MASTER true       // set true on 1 ESP, false on slaves
#define UDP_PORT 21324    // UDP sync port

class SnakeLampsUsermod : public Usermod {
private:
  uint32_t lastMs = 0;
  uint32_t currentColor = 0;
  int head = 0;
  bool initialized = false;
  std::vector<int> snakePath;

  WiFiUDP udp;

  inline uint32_t packRGB(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }

  void setPixelRGB(int idx, uint8_t r, uint8_t g, uint8_t b) {
    if (idx < 0 || idx >= strip.getLengthTotal()) return;
    strip.setPixelColor(idx, ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
  }

  void unpackAndSet(int idx, uint32_t color, uint8_t briPct) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint16_t scale = (uint16_t)briPct * 255 / 100;
    r = (uint8_t)((uint16_t)r * scale / 255);
    g = (uint8_t)((uint16_t)g * scale / 255);
    b = (uint8_t)((uint16_t)b * scale / 255);
    setPixelRGB(idx, r, g, b);
  }

  void chooseNewColor() {
    uint8_t h = random(0, 255);
    CHSV hsv(h, 255, 255);
    CRGB rgb;
    hsv2rgb_rainbow(hsv, rgb);
    currentColor = packRGB(rgb.r, rgb.g, rgb.b);
  }

  void buildPath() {
    snakePath.clear();
    // Segment order: Lamp1 seg0, seg1; Lamp2 seg0, seg1; Lamp3 seg0, seg1
    int segOrder[6] = {0,1,2,3,4,5};
    for (int i = 0; i < 6; i++) {
      WS2812FX::Segment seg = strip.getSegment(segOrder[i]);
      if (seg.len <= 0) continue;
      if (i % 2 == 0) { // 86-LED segment = forward
        for (int p = 0; p < seg.len; p++) snakePath.push_back(seg.start + p);
      } else {          // 95-LED segment = reversed
        for (int p = seg.len - 1; p >= 0; p--) snakePath.push_back(seg.start + p);
      }
    }
  }

  void sendUDP() {
    if (!MASTER) return;
    uint8_t buf[5];
    buf[0] = (head >> 24) & 0xFF;
    buf[1] = (head >> 16) & 0xFF;
    buf[2] = (head >> 8) & 0xFF;
    buf[3] = head & 0xFF;
    buf[4] = (currentColor >> 16) & 0xFF; // send just the color high byte for simplicity
    udp.beginPacketMulticast(IPAddress(239,255,255,250), UDP_PORT, WiFi.localIP());
    udp.write(buf, sizeof(buf));
    udp.endPacket();
  }

  void receiveUDP() {
    if (MASTER) return;
    int packetSize = udp.parsePacket();
    if (packetSize >= 5) {
      uint8_t buf[5];
      udp.read(buf, 5);
      head = ((uint32_t)buf[0]<<24)|((uint32_t)buf[1]<<16)|((uint32_t)buf[2]<<8)|buf[3];
      uint8_t colorHigh = buf[4];
      currentColor = packRGB(colorHigh, colorHigh, colorHigh); // simplified for slave
    }
  }

public:
  void setup() {
    buildPath();
    chooseNewColor();
    head = 0;
    lastMs = millis();
    initialized = true;
    udp.begin(UDP_PORT);
  }

  void loop() {
    if (!initialized) return;
    if (millis() - lastMs < FRAME_MS) return;
    lastMs = millis();

    if (MASTER) {
      head++;
      if (head >= snakePath.size()) {
        head = 0;
        chooseNewColor();
      }
      sendUDP();
    } else {
      receiveUDP();
    }

    uint8_t wb = (uint16_t)BACKGROUND_WHITE_PCT * 255 / 100;
    for (int i = 0; i < strip.getLengthTotal(); i++) setPixelRGB(i, wb, wb, wb);

    int len = SNAKE_LEN;
    if (len > snakePath.size()) len = snakePath.size();
    for (int s = 0; s < len; s++) {
      int pos = head - s;
      if (pos < 0) pos += snakePath.size();
      int ledIndex = snakePath[pos];
      unpackAndSet(ledIndex, currentColor, SNAKE_BRI_PCT);
    }

    ledsUpdated();
  }

  uint16_t getId() { return USERMOD_ID_BASE + 212; }
};

SnakeLampsUsermod* snakeLampsUsermod = new SnakeLampsUsermod();
usermods.add(snakeLampsUsermod);

