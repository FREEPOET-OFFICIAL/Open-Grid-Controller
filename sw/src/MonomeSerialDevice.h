#ifndef MONOME_SERIAL_DEVICE_H
#define MONOME_SERIAL_DEVICE_H

#include <Arduino.h>

class MonomeGridEvent {
public:
    uint8_t x;
    uint8_t y;
    uint8_t pressed;
};

class MonomeArcEvent {
public:
    uint8_t index;
    int8_t delta;
};

class MonomeEventQueue {
public:
    bool gridEventAvailable();
    MonomeGridEvent readGridEvent();

    bool arcEventAvailable();
    MonomeArcEvent readArcEvent();

    void addGridEvent(uint8_t x, uint8_t y, uint8_t pressed);
    void sendGridKey(uint8_t x, uint8_t y, uint8_t pressed);
    void queueGridKey(uint8_t x, uint8_t y, uint8_t pressed);
    void flushGridKeyQueue();
    void addArcEvent(uint8_t index, int8_t delta);
    void sendArcDelta(uint8_t index, int8_t delta);
    void sendArcKey(uint8_t index, uint8_t pressed);

private:
    static const int MAX_EVENT_COUNT = 50;
    static const int MAX_OUTGOING_COUNT = 64;

    MonomeGridEvent outgoingGridEvents[MAX_OUTGOING_COUNT] = {};
    int outgoingGridEventCount = 0;
    int outgoingGridFirstEvent = 0;

    MonomeGridEvent emptyGridEvent = {};
    MonomeGridEvent gridEvents[MAX_EVENT_COUNT] = {};
    int gridEventCount = 0;
    int gridFirstEvent = 0;

    MonomeArcEvent emptyArcEvent = {};
    MonomeArcEvent arcEvents[MAX_EVENT_COUNT] = {};
    int arcEventCount = 0;
    int arcFirstEvent = 0;
};

class MonomeSerialDevice : public MonomeEventQueue {
public:
    MonomeSerialDevice();

    void initialize();
    void setupAsGrid(uint8_t rows, uint8_t columns);
    void setupAsArc(uint8_t encoders);
    void poll();

    void setGridLed(uint8_t x, uint8_t y, uint8_t level);
    void clearGridLed(uint8_t x, uint8_t y);
    void setArcLed(uint8_t enc, uint8_t led, uint8_t level);
    void setAllLEDs(int value);
    void clearArcLed(uint8_t enc, uint8_t led);
    void clearAllLeds();
    void clearArcRing(uint8_t ring);
    void refreshGrid();
    void refreshArc();

    bool active = false;
    bool isMonome = false;
    bool isGrid = true;
    uint8_t rows = 0;
    uint8_t columns = 0;
    uint8_t encoders = 0;

    static const int variMonoThresh = 0;
    static const int MAX_LED_COUNT = 256;
    uint8_t leds[MAX_LED_COUNT] = {0};
    String deviceID;

private:
    bool arcDirty = false;
    bool gridDirty = false;

    static size_t getCommandLength(uint8_t command);
    bool tryProcessSerial();
    void processSerial();
};

#endif