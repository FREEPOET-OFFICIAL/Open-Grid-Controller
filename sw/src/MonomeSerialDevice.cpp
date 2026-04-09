#include "MonomeSerialDevice.h"

#include <Adafruit_TinyUSB.h>

MonomeSerialDevice::MonomeSerialDevice() {}

size_t MonomeSerialDevice::getCommandLength(uint8_t command) {
    switch (command) {
        case 0x00:
        case 0x01:
        case 0x03:
        case 0x05:
        case 0x07:
        case 0x12:
        case 0x13:
        case 0x80:
        case 0x81:
            return 1;
        case 0x17:
        case 0x19:
        case 0x51:
        case 0x52:
            return 2;
        case 0x06:
        case 0x08:
        case 0x10:
        case 0x11:
        case 0x15:
        case 0x16:
        case 0x20:
        case 0x21:
        case 0x50:
        case 0x91:
            return 3;
        case 0x04:
        case 0x18:
        case 0x90:
            return 4;
        case 0x93:
            return 5;
        case 0x1B:
        case 0x1C:
            return 7;
        case 0x0F:
            return 9;
        case 0x14:
            return 11;
        case 0x02:
            return 33;
        case 0x92:
            return 34;
        case 0x1A:
            return 35;
        default:
            return 1;
    }
}

void MonomeSerialDevice::initialize() {
    active = false;
    isMonome = false;
    isGrid = true;
    rows = 0;
    columns = 0;
    encoders = 0;
    clearAllLeds();
    arcDirty = false;
    gridDirty = false;
}

void MonomeSerialDevice::setupAsGrid(uint8_t gridRows, uint8_t gridColumns) {
    initialize();
    active = true;
    isMonome = true;
    isGrid = true;
    rows = gridRows;
    columns = gridColumns;
    gridDirty = true;
}

void MonomeSerialDevice::setupAsArc(uint8_t encoderCount) {
    initialize();
    active = true;
    isMonome = true;
    isGrid = false;
    encoders = encoderCount;
    arcDirty = true;
}

void MonomeSerialDevice::poll() {
    while (tryProcessSerial()) {
    }
}

bool MonomeSerialDevice::tryProcessSerial() {
    const int availableBytes = Serial.available();
    if (availableBytes <= 0) {
        return false;
    }

    const int peekedCommand = Serial.peek();
    if (peekedCommand < 0) {
        return false;
    }

    const size_t commandLength = getCommandLength(static_cast<uint8_t>(peekedCommand));
    if (availableBytes < static_cast<int>(commandLength)) {
        return false;
    }

    processSerial();
    return true;
}

void MonomeSerialDevice::setAllLEDs(int value) {
    for (int i = 0; i < MAX_LED_COUNT; ++i) {
        leds[i] = value;
    }
}

void MonomeSerialDevice::setGridLed(uint8_t x, uint8_t y, uint8_t level) {
    if (x < columns && y < rows) {
        const uint32_t index = y * columns + x;
        leds[index] = level;
    }
}

void MonomeSerialDevice::clearGridLed(uint8_t x, uint8_t y) {
    setGridLed(x, y, 0);
}

void MonomeSerialDevice::setArcLed(uint8_t enc, uint8_t led, uint8_t level) {
    const int index = led + (enc << 6);
    if (index < MAX_LED_COUNT) {
        leds[index] = level;
    }
}

void MonomeSerialDevice::clearArcLed(uint8_t enc, uint8_t led) {
    setArcLed(enc, led, 0);
}

void MonomeSerialDevice::clearAllLeds() {
    for (int i = 0; i < MAX_LED_COUNT; ++i) {
        leds[i] = 0;
    }
}

void MonomeSerialDevice::clearArcRing(uint8_t ring) {
    for (int index = ring << 6, upper = index + 64; index < upper && index < MAX_LED_COUNT; ++index) {
        leds[index] = 0;
    }
}

void MonomeSerialDevice::refreshGrid() {
    gridDirty = true;
}

void MonomeSerialDevice::refreshArc() {
    arcDirty = true;
}

void MonomeSerialDevice::processSerial() {
    uint8_t identifierSent;
    uint8_t index;
    uint8_t readX;
    uint8_t readY;
    uint8_t readN;
    uint8_t readA;
    uint8_t dummy;
    uint8_t gridNum;
    uint8_t deviceAddress;
    uint8_t n;
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t intensity = 15;
    uint8_t gridKeyX;
    uint8_t gridKeyY;
    int8_t delta;
    const uint8_t gridX = columns;
    const uint8_t gridY = rows;
    const uint8_t numQuads = columns / rows;

    identifierSent = Serial.read();

    switch (identifierSent) {
        case 0x00:
            Serial.write(static_cast<uint8_t>(0x00));
            Serial.write(static_cast<uint8_t>(0x01));
            Serial.write(numQuads);

            Serial.write(static_cast<uint8_t>(0x00));
            Serial.write(static_cast<uint8_t>(0x02));
            Serial.write(numQuads);
            break;

        case 0x01:
            Serial.write(static_cast<uint8_t>(0x01));
            for (uint8_t i = 0; i < 32; ++i) {
                if (i < deviceID.length()) {
                    Serial.write(deviceID[i]);
                } else {
                    Serial.write(static_cast<uint8_t>(0x00));
                }
            }
            break;

        case 0x02:
            {
                char incomingId[33] = {0};
                for (uint8_t i = 0; i < 32; ++i) {
                    incomingId[i] = static_cast<char>(Serial.read());
                }
                deviceID = String(incomingId);
            }
            break;

        case 0x03:
            Serial.write(static_cast<uint8_t>(0x02));
            Serial.write(static_cast<uint8_t>(0x01));
            Serial.write(static_cast<uint8_t>(0));
            Serial.write(static_cast<uint8_t>(0));
            break;

        case 0x04:
            gridNum = Serial.read();
            readX = Serial.read();
            readY = Serial.read();
            (void)gridNum;
            (void)readX;
            (void)readY;
            break;

        case 0x05:
            Serial.write(static_cast<uint8_t>(0x03));
            Serial.write(gridX);
            Serial.write(gridY);
            break;

        case 0x06:
            readX = Serial.read();
            readY = Serial.read();
            (void)readX;
            (void)readY;
            break;

        case 0x07:
            break;

        case 0x08:
            deviceAddress = Serial.read();
            dummy = Serial.read();
            (void)deviceAddress;
            (void)dummy;
            break;

        case 0x0F:
            for (uint8_t i = 0; i < 8; ++i) {
                Serial.read();
            }
            break;

        case 0x10:
            readX = Serial.read();
            readY = Serial.read();
            setGridLed(readX, readY, 0);
            break;

        case 0x11:
            readX = Serial.read();
            readY = Serial.read();
            setGridLed(readX, readY, 15);
            break;

        case 0x12:
            clearAllLeds();
            break;

        case 0x13:
            setAllLEDs(15);
            break;

        case 0x14:
            readX = Serial.read();
            while (readX > 16) {
                readX += 16;
            }
            readX &= 0xF8;

            readY = Serial.read();
            while (readY > 16) {
                readY += 16;
            }
            readY &= 0xF8;

            for (y = 0; y < 8; ++y) {
                intensity = Serial.read();
                for (x = 0; x < 8; ++x) {
                    setGridLed(readX + x, readY + y, ((intensity >> x) & 0x01) ? 15 : 0);
                }
            }
            break;

        case 0x15:
            readX = Serial.read();
            while (readX > 16) {
                readX += 16;
            }
            readX &= 0xF8;

            readY = Serial.read();
            intensity = Serial.read();
            for (x = 0; x < 8; ++x) {
                setGridLed(readX + x, readY, ((intensity >> x) & 0x01) ? 15 : 0);
            }
            break;

        case 0x16:
            readX = Serial.read();
            readY = Serial.read();
            while (readY > 16) {
                readY += 16;
            }
            readY &= 0xF8;

            intensity = Serial.read();
            for (y = 0; y < 8; ++y) {
                setGridLed(readX, readY + y, ((intensity >> y) & 0x01) ? 15 : 0);
            }
            break;

        case 0x17:
            intensity = Serial.read();
            setAllLEDs(intensity);
            break;

        case 0x18:
            readX = Serial.read();
            readY = Serial.read();
            intensity = Serial.read();
            setGridLed(readX, readY, intensity);
            break;

        case 0x19:
            intensity = Serial.read();
            setAllLEDs(intensity);
            break;

        case 0x1A:
            readX = Serial.read();
            while (readX > 16) {
                readX += 16;
            }
            readX &= 0xF8;

            readY = Serial.read();
            while (readY > 16) {
                readY += 16;
            }
            readY &= 0xF8;

            z = 0;
            for (y = 0; y < 8; ++y) {
                for (x = 0; x < 8; ++x) {
                    if ((z % 2) == 0) {
                        intensity = Serial.read();
                        setGridLed(readX + x, readY + y,
                                   (((intensity >> 4) & 0x0F) > variMonoThresh) ? ((intensity >> 4) & 0x0F) : 0);
                    } else {
                        setGridLed(readX + x, readY + y,
                                   ((intensity & 0x0F) > variMonoThresh) ? (intensity & 0x0F) : 0);
                    }
                    ++z;
                }
            }
            break;

        case 0x1B:
            readX = Serial.read();
            while (readX > 16) {
                readX += 16;
            }
            readX &= 0xF8;

            readY = Serial.read();
            while (readY > 16) {
                readY += 16;
            }
            readY &= 0xF8;

            for (x = 0; x < 8; ++x) {
                if ((x % 2) == 0) {
                    intensity = Serial.read();
                    setGridLed(readX + x, readY,
                               (((intensity >> 4) & 0x0F) > variMonoThresh) ? ((intensity >> 4) & 0x0F) : 0);
                } else {
                    setGridLed(readX + x, readY,
                               ((intensity & 0x0F) > variMonoThresh) ? (intensity & 0x0F) : 0);
                }
            }
            break;

        case 0x1C:
            readX = Serial.read();
            while (readX > 16) {
                readX += 16;
            }
            readX &= 0xF8;

            readY = Serial.read();
            while (readY > 16) {
                readY += 16;
            }
            readY &= 0xF8;

            for (y = 0; y < 8; ++y) {
                if ((y % 2) == 0) {
                    intensity = Serial.read();
                    setGridLed(readX, readY + y,
                               (((intensity >> 4) & 0x0F) > variMonoThresh) ? ((intensity >> 4) & 0x0F) : 0);
                } else {
                    setGridLed(readX, readY + y,
                               ((intensity & 0x0F) > variMonoThresh) ? (intensity & 0x0F) : 0);
                }
            }
            break;

        case 0x20:
            gridKeyX = Serial.read();
            gridKeyY = Serial.read();
            addGridEvent(gridKeyX, gridKeyY, 0);
            break;

        case 0x21:
            gridKeyX = Serial.read();
            gridKeyY = Serial.read();
            addGridEvent(gridKeyX, gridKeyY, 1);
            break;

        case 0x50:
            index = Serial.read();
            delta = static_cast<int8_t>(Serial.read());
            addArcEvent(index, delta);
            break;

        case 0x51:
        case 0x52:
            n = Serial.read();
            (void)n;
            break;

        case 0x80:
        case 0x81:
            break;

        case 0x90:
            readN = Serial.read();
            readX = Serial.read();
            readA = Serial.read();
            setArcLed(readN, readX, readA);
            break;

        case 0x91:
            readN = Serial.read();
            readA = Serial.read();
            for (uint8_t q = 0; q < 64; ++q) {
                setArcLed(readN, q, readA);
            }
            break;

        case 0x92:
            readN = Serial.read();
            for (y = 0; y < 64; ++y) {
                if ((y % 2) == 0) {
                    intensity = Serial.read();
                    setArcLed(readN, y, ((intensity >> 4) & 0x0F) > 0 ? ((intensity >> 4) & 0x0F) : 0);
                } else {
                    setArcLed(readN, y, (intensity & 0x0F) > 0 ? (intensity & 0x0F) : 0);
                }
            }
            break;

        case 0x93:
            readN = Serial.read();
            readX = Serial.read();
            readY = Serial.read();
            readA = Serial.read();

            if (readX < readY) {
                for (y = readX; y < readY; ++y) {
                    setArcLed(readN, y, readA);
                }
            } else {
                for (y = readX; y < 64; ++y) {
                    setArcLed(readN, y, readA);
                }
                for (x = 0; x < readY; ++x) {
                    setArcLed(readN, x, readA);
                }
            }
            break;

        default:
            break;
    }
}

void MonomeEventQueue::addGridEvent(uint8_t x, uint8_t y, uint8_t pressed) {
    if (gridEventCount >= MAX_EVENT_COUNT) {
        return;
    }

    const uint8_t index = (gridFirstEvent + gridEventCount) % MAX_EVENT_COUNT;
    gridEvents[index].x = x;
    gridEvents[index].y = y;
    gridEvents[index].pressed = pressed;
    ++gridEventCount;
}

bool MonomeEventQueue::gridEventAvailable() {
    return gridEventCount > 0;
}

MonomeGridEvent MonomeEventQueue::readGridEvent() {
    if (gridEventCount == 0) {
        return emptyGridEvent;
    }

    --gridEventCount;
    const uint8_t index = gridFirstEvent;
    gridFirstEvent = (gridFirstEvent + 1) % MAX_EVENT_COUNT;
    return gridEvents[index];
}

void MonomeEventQueue::addArcEvent(uint8_t index, int8_t delta) {
    if (arcEventCount >= MAX_EVENT_COUNT) {
        return;
    }

    const uint8_t eventIndex = (arcFirstEvent + arcEventCount) % MAX_EVENT_COUNT;
    arcEvents[eventIndex].index = index;
    arcEvents[eventIndex].delta = delta;
    ++arcEventCount;
}

bool MonomeEventQueue::arcEventAvailable() {
    return arcEventCount > 0;
}

MonomeArcEvent MonomeEventQueue::readArcEvent() {
    if (arcEventCount == 0) {
        return emptyArcEvent;
    }

    --arcEventCount;
    const uint8_t index = arcFirstEvent;
    arcFirstEvent = (arcFirstEvent + 1) % MAX_EVENT_COUNT;
    return arcEvents[index];
}

void MonomeEventQueue::sendArcDelta(uint8_t index, int8_t delta) {
    Serial.write(static_cast<uint8_t>(0x50));
    Serial.write(index);
    Serial.write(delta);
}

void MonomeEventQueue::sendArcKey(uint8_t index, uint8_t pressed) {
    uint8_t buffer[2] = {static_cast<uint8_t>(pressed == 1 ? 0x52 : 0x51), index};
    Serial.write(buffer, sizeof(buffer));
}

void MonomeEventQueue::sendGridKey(uint8_t x, uint8_t y, uint8_t pressed) {
    const uint8_t buffer[3] = {
        static_cast<uint8_t>(pressed == 1 ? 0x21 : 0x20),
        x,
        y,
    };

    tud_cdc_write(buffer, sizeof(buffer));
    tud_cdc_write_flush();
}

void MonomeEventQueue::queueGridKey(uint8_t x, uint8_t y, uint8_t pressed) {
    if (outgoingGridEventCount >= MAX_OUTGOING_COUNT) {
        return;
    }

    const int insertIndex = (outgoingGridFirstEvent + outgoingGridEventCount) % MAX_OUTGOING_COUNT;
    outgoingGridEvents[insertIndex].x = x;
    outgoingGridEvents[insertIndex].y = y;
    outgoingGridEvents[insertIndex].pressed = pressed;
    ++outgoingGridEventCount;
}

void MonomeEventQueue::flushGridKeyQueue() {
    while (outgoingGridEventCount > 0) {
        MonomeGridEvent event = outgoingGridEvents[outgoingGridFirstEvent];
        outgoingGridFirstEvent = (outgoingGridFirstEvent + 1) % MAX_OUTGOING_COUNT;
        --outgoingGridEventCount;
        sendGridKey(event.x, event.y, event.pressed);
    }

    tud_cdc_write_flush();
}