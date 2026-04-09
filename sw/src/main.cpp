#include <Adafruit_TinyUSB.h>

#include "MonomeSerialDevice.h"
#include "keypad.h"

namespace {

constexpr uint32_t kKeyScanIntervalMs = 4;
constexpr uint32_t kLedRefreshIntervalMs = 8;

char manufacturerString[] = "monome";
char productString[] = "monome";
char serialString[] = "m6000000";

MonomeSerialDevice monomeDevice;
Keypad grid;
uint8_t previousLedBuffer[Keypad::kKeyCount] = {0};
uint32_t lastKeyScanMs = 0;
uint32_t lastLedRefreshMs = 0;

void processKeyInput() {
  for (uint8_t keyIndex = 0; keyIndex < grid.getTotalKeys(); ++keyIndex) {
    const uint8_t x = keyIndex / Keypad::kRows;
    const uint8_t y = keyIndex % Keypad::kRows;

    if (grid.wasKeyPressed(keyIndex)) {
      monomeDevice.queueGridKey(x, y, 1);
    } else if (grid.wasKeyReleased(keyIndex)) {
      monomeDevice.queueGridKey(x, y, 0);
    }
  }
}

void sendLeds() {
  bool dirty = false;

  for (uint8_t index = 0; index < Keypad::kKeyCount; ++index) {
    const uint8_t value = monomeDevice.leds[index];
    if (value == previousLedBuffer[index]) {
      continue;
    }

    const uint8_t x = index % Keypad::kCols;
    const uint8_t y = index / Keypad::kCols;
    const uint8_t hardwareIndex = y + x * Keypad::kRows;
    grid.setPixelColor(hardwareIndex, value);
    previousLedBuffer[index] = value;
    dirty = true;
  }

  if (dirty || grid.needsLedSync()) {
    grid.update();
  }
}

}  // namespace

void setup() {
  TinyUSBDevice.setManufacturerDescriptor(manufacturerString);
  TinyUSBDevice.setProductDescriptor(productString);
  TinyUSBDevice.setSerialDescriptor(serialString);

  Serial.begin(115200);

  const uint32_t mountDeadline = millis() + 3000;
  while (!TinyUSBDevice.mounted() && millis() < mountDeadline) {
    delay(1);
  }

  monomeDevice.deviceID = "OpenGrid";
  monomeDevice.isMonome = true;
  monomeDevice.setupAsGrid(Keypad::kRows, Keypad::kCols);

  grid.begin();

  for (uint8_t index = 0; index < 8; ++index) {
    monomeDevice.poll();
    delay(20);
  }

  Serial.println("[USB] monome grid bridge ready");
}

void loop() {
  monomeDevice.poll();

  const uint32_t now = millis();
  if (now - lastKeyScanMs >= kKeyScanIntervalMs) {
    grid.scanKeypad();
    processKeyInput();
    lastKeyScanMs = now;
  }

  if (now - lastLedRefreshMs >= kLedRefreshIntervalMs) {
    sendLeds();
    lastLedRefreshMs = now;
  }

  monomeDevice.poll();

  monomeDevice.flushGridKeyQueue();
}