#include "keypad.h"

#include <Wire.h>

const Keypad::BoardConfig Keypad::kBoardConfigs[Keypad::kBoardCount] = {
    {0x2E, 0},
    {0x2D, 8},
};

bool Keypad::begin() {
    Wire.setSDA(kI2cSdaPin);
    Wire.setSCL(kI2cSclPin);
    Wire.begin();
    Wire.setClock(kI2cSpeedHz);

    for (uint8_t index = 0; index < kKeyCount; ++index) {
        lastSentLevels_[index] = 0xFF;
    }

    for (uint8_t boardIndex = 0; boardIndex < kBoardCount; ++boardIndex) {
        fullLedSync_[boardIndex] = true;
    }

    refreshConnection(true);
    return true;
}

void Keypad::scanKeypad() {
    refreshConnection(false);

    for (uint8_t keyIndex = 0; keyIndex < kKeyCount; ++keyIndex) {
        keyPressedEdge_[keyIndex] = false;
        keyReleasedEdge_[keyIndex] = false;
    }

    const uint32_t now = millis();
    uint8_t block[kReadBlockMax] = {0};

    for (uint8_t boardIndex = 0; boardIndex < kBoardCount; ++boardIndex) {
        if (!boardConnected_[boardIndex]) {
            continue;
        }

        const BoardConfig &board = kBoardConfigs[boardIndex];
        bool boardReadFailed = false;
        bool rawStates[kKeysPerBoard] = {false};

        for (uint8_t startKey = 0; startKey < kKeysPerBoard; startKey += kReadBlockMax) {
            if (!readKeyBlock(board.address, startKey, kReadBlockMax, block)) {
                boardReadFailed = true;
                fullLedSync_[boardIndex] = true;
                break;
            }

            for (uint8_t offset = 0; offset < kReadBlockMax; ++offset) {
                rawStates[startKey + offset] = (block[offset] != 0);
            }
        }

        if (boardReadFailed || !boardConnected_[boardIndex]) {
            continue;
        }

        const bool inWarmup = (warmupFrames_[boardIndex] > 0) || (now < warmupUntilMs_[boardIndex]);
        if (warmupFrames_[boardIndex] > 0) {
            --warmupFrames_[boardIndex];
        }
        if (inWarmup) {
            continue;
        }

        for (uint8_t localKey = 0; localKey < kKeysPerBoard; ++localKey) {
            const uint8_t globalKey = localToGlobalKey(boardIndex, localKey);
            const bool rawPressed = rawStates[localKey];

            if (rawPressed == stablePressed_[globalKey]) {
                candidateCount_[globalKey] = 0;
                continue;
            }

            if (rawPressed == candidatePressed_[globalKey]) {
                if (candidateCount_[globalKey] < 0xFF) {
                    ++candidateCount_[globalKey];
                }
            } else {
                candidatePressed_[globalKey] = rawPressed;
                candidateCount_[globalKey] = 1;
            }

            if (candidateCount_[globalKey] >= kDebounceSamples) {
                stablePressed_[globalKey] = rawPressed;
                candidateCount_[globalKey] = 0;
                keyPressedEdge_[globalKey] = rawPressed;
                keyReleasedEdge_[globalKey] = !rawPressed;
            }
        }
    }
}

bool Keypad::wasKeyPressed(uint8_t keyIndex) const {
    return keyIndex < kKeyCount && keyPressedEdge_[keyIndex];
}

bool Keypad::wasKeyReleased(uint8_t keyIndex) const {
    return keyIndex < kKeyCount && keyReleasedEdge_[keyIndex];
}

uint8_t Keypad::getTotalKeys() const {
    return kKeyCount;
}

void Keypad::setPixelColor(uint8_t keyIndex, uint8_t level) {
    if (keyIndex >= kKeyCount) {
        return;
    }

    ledLevels_[keyIndex] = (level > 15) ? 15 : level;
}

void Keypad::clearPixels() {
    for (uint8_t keyIndex = 0; keyIndex < kKeyCount; ++keyIndex) {
        ledLevels_[keyIndex] = 0;
    }
    for (uint8_t boardIndex = 0; boardIndex < kBoardCount; ++boardIndex) {
        fullLedSync_[boardIndex] = true;
    }
}

void Keypad::update() {
    for (uint8_t boardIndex = 0; boardIndex < kBoardCount; ++boardIndex) {
        if (!boardConnected_[boardIndex]) {
            continue;
        }

        const BoardConfig &board = kBoardConfigs[boardIndex];
        bool needsSync = fullLedSync_[boardIndex];

        if (!needsSync) {
            for (uint8_t localKey = 0; localKey < kKeysPerBoard; ++localKey) {
                const uint8_t globalKey = localToGlobalKey(boardIndex, localKey);
                if (ledLevels_[globalKey] != lastSentLevels_[globalKey]) {
                    needsSync = true;
                    break;
                }
            }
        }

        if (!needsSync) {
            continue;
        }

        uint8_t levels[kPackedLevelBlockMax] = {0};
        bool transferFailed = false;

        for (uint8_t startKey = 0; startKey < kKeysPerBoard; startKey += kPackedLevelBlockMax) {
            const uint8_t count = static_cast<uint8_t>(min<uint8_t>(kPackedLevelBlockMax, kKeysPerBoard - startKey));
            for (uint8_t offset = 0; offset < count; ++offset) {
                const uint8_t globalKey = localToGlobalKey(boardIndex, static_cast<uint8_t>(startKey + offset));
                levels[offset] = ledLevels_[globalKey] & 0x0F;
            }

            if (!writePackedLevelBlock(board.address, startKey, count, levels)) {
                transferFailed = true;
                break;
            }
        }

        if (transferFailed) {
            fullLedSync_[boardIndex] = true;
            continue;
        }

        if (!showLeds(board.address)) {
            fullLedSync_[boardIndex] = true;
            continue;
        }

        for (uint8_t localKey = 0; localKey < kKeysPerBoard; ++localKey) {
            const uint8_t globalKey = localToGlobalKey(boardIndex, localKey);
            lastSentLevels_[globalKey] = ledLevels_[globalKey];
        }

        fullLedSync_[boardIndex] = false;
    }
}

bool Keypad::isConnected() const {
    for (uint8_t boardIndex = 0; boardIndex < kBoardCount; ++boardIndex) {
        if (boardConnected_[boardIndex]) {
            return true;
        }
    }
    return false;
}

bool Keypad::needsLedSync() const {
    for (uint8_t boardIndex = 0; boardIndex < kBoardCount; ++boardIndex) {
        if (boardConnected_[boardIndex] && fullLedSync_[boardIndex]) {
            return true;
        }
    }
    return false;
}

void Keypad::refreshConnection(bool force) {
    const uint32_t now = millis();
    if (!force) {
        bool anyBoardConnected = false;
        for (uint8_t boardIndex = 0; boardIndex < kBoardCount; ++boardIndex) {
            if (boardConnected_[boardIndex]) {
                anyBoardConnected = true;
                break;
            }
        }

        const uint32_t scanInterval = anyBoardConnected ? kScanIntervalMs : kScanIntervalDisconnectedMs;
        if (now - lastScanMs_ < scanInterval) {
            return;
        }
    }

    lastScanMs_ = now;

    for (uint8_t boardIndex = 0; boardIndex < kBoardCount; ++boardIndex) {
        const BoardConfig &board = kBoardConfigs[boardIndex];
        const bool wasPresent = boardConnected_[boardIndex];
        bool isPresent = false;

        if (wasPresent) {
            isPresent = probeDevice(board.address);
        } else {
            isPresent = probeDevice(board.address) && readDeviceInfo(board.address);
        }

        if (isPresent && !wasPresent) {
            boardConnected_[boardIndex] = true;
            warmupFrames_[boardIndex] = kStartupIgnoreFrames;
            warmupUntilMs_[boardIndex] = now + kStartupIgnoreMs;
            resetBoardKeyState(boardIndex);
            fullLedSync_[boardIndex] = true;

            for (uint8_t localKey = 0; localKey < kKeysPerBoard; ++localKey) {
                lastSentLevels_[localToGlobalKey(boardIndex, localKey)] = 0xFF;
            }

            Serial.printf("[I2C] board %u online at 0x%02X (x=%u..%u)\n",
                          boardIndex,
                          board.address,
                          board.xOffset,
                          static_cast<uint8_t>(board.xOffset + kBoardCols - 1));
        } else if (!isPresent && wasPresent) {
            Serial.printf("[I2C] board %u offline at 0x%02X\n", boardIndex, board.address);
            markBoardDisconnected(boardIndex);
        }
    }
}

bool Keypad::probeDevice(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission(true) == 0;
}

bool Keypad::readDeviceInfo(uint8_t address) {
    uint8_t protocolVersion = 0;
    uint8_t deviceAddress = 0;
    uint8_t keyCountLow = 0;
    uint8_t keyCountHigh = 0;

    if (!readRegister(address, kRegProtocolVersion, protocolVersion) ||
        !readRegister(address, kRegDeviceAddress, deviceAddress) ||
        !readRegister(address, kRegKeyCountL, keyCountLow) ||
        !readRegister(address, kRegKeyCountH, keyCountHigh)) {
        return false;
    }

    const uint16_t keyCount = static_cast<uint16_t>(keyCountLow) | (static_cast<uint16_t>(keyCountHigh) << 8);
    return protocolVersion == kProtocolVersion && deviceAddress == address && keyCount >= kMinimumBoardKeyCount;
}

bool Keypad::readRegister(uint8_t address, uint8_t reg, uint8_t &value) {
    const uint8_t request[] = {reg};
    uint8_t response[1] = {0};
    if (!readRegisters(address, request, sizeof(request), response, sizeof(response))) {
        return false;
    }

    value = response[0];
    return true;
}

bool Keypad::readRegisters(uint8_t address, const uint8_t *request, size_t requestLength, uint8_t *response, size_t responseLength) {
    for (uint8_t attempt = 0; attempt < kRetryCount; ++attempt) {
        Wire.beginTransmission(address);
        Wire.write(request, requestLength);
        if (Wire.endTransmission(false) != 0) {
            continue;
        }

        const uint8_t bytesRead = Wire.requestFrom(static_cast<int>(address), static_cast<int>(responseLength));
        if (bytesRead != responseLength) {
            continue;
        }

        for (size_t index = 0; index < responseLength; ++index) {
            response[index] = static_cast<uint8_t>(Wire.read());
        }
        return true;
    }

    return false;
}

bool Keypad::writeCommand(uint8_t address, const uint8_t *data, size_t length) {
    for (uint8_t attempt = 0; attempt < kRetryCount; ++attempt) {
        Wire.beginTransmission(address);
        Wire.write(data, length);
        if (Wire.endTransmission(true) == 0) {
            return true;
        }
    }

    return false;
}

bool Keypad::readKeyBlock(uint8_t address, uint8_t startKey, uint8_t count, uint8_t *outStates) {
    const uint8_t request[] = {kRegKeyReadBlock, startKey, count};
    return readRegisters(address, request, sizeof(request), outStates, count);
}

bool Keypad::clearAllLeds(uint8_t address) {
    const uint8_t command[] = {kRegLedClear};
    return writeCommand(address, command, sizeof(command)) && showLeds(address);
}

bool Keypad::showLeds(uint8_t address) {
    const uint8_t command[] = {kRegLedShow};
    return writeCommand(address, command, sizeof(command));
}

bool Keypad::writePackedLevelBlock(uint8_t address, uint8_t startKey, uint8_t count, const uint8_t *levels) {
    if (count == 0 || count > kPackedLevelBlockMax) {
        return false;
    }

    const uint8_t packedByteCount = static_cast<uint8_t>((count + 1) / 2);
    uint8_t command[3 + ((kPackedLevelBlockMax + 1) / 2)] = {0};
    command[0] = kRegLedSetLevelBlockPacked;
    command[1] = startKey;
    command[2] = count;

    for (uint8_t offset = 0; offset < count; offset += 2) {
        const uint8_t firstLevel = levels[offset] & 0x0F;
        const uint8_t secondLevel = (offset + 1 < count) ? (levels[offset + 1] & 0x0F) : 0;
        command[3 + (offset / 2)] = static_cast<uint8_t>((firstLevel << 4) | secondLevel);
    }

    return writeCommand(address, command, static_cast<size_t>(3 + packedByteCount));
}

uint8_t Keypad::localToGlobalKey(uint8_t boardIndex, uint8_t localKeyIndex) const {
    const uint8_t localX = localKeyIndex / kRows;
    const uint8_t localY = localKeyIndex % kRows;
    const uint8_t globalX = static_cast<uint8_t>(kBoardConfigs[boardIndex].xOffset + localX);
    return static_cast<uint8_t>(globalX * kRows + localY);
}

uint8_t Keypad::globalToBoardIndex(uint8_t globalKeyIndex) const {
    const uint8_t globalX = globalKeyIndex / kRows;
    return static_cast<uint8_t>(globalX / kBoardCols);
}

uint8_t Keypad::globalToLocalKey(uint8_t globalKeyIndex) const {
    const uint8_t globalX = globalKeyIndex / kRows;
    const uint8_t globalY = globalKeyIndex % kRows;
    const uint8_t localX = static_cast<uint8_t>(globalX % kBoardCols);
    return static_cast<uint8_t>(localX * kRows + globalY);
}

void Keypad::resetBoardKeyState(uint8_t boardIndex) {
    for (uint8_t localKey = 0; localKey < kKeysPerBoard; ++localKey) {
        const uint8_t globalKey = localToGlobalKey(boardIndex, localKey);
        stablePressed_[globalKey] = false;
        candidatePressed_[globalKey] = false;
        candidateCount_[globalKey] = 0;
        keyPressedEdge_[globalKey] = false;
        keyReleasedEdge_[globalKey] = false;
    }
}

void Keypad::markBoardDisconnected(uint8_t boardIndex) {
    boardConnected_[boardIndex] = false;
    warmupFrames_[boardIndex] = 0;
    warmupUntilMs_[boardIndex] = 0;
    fullLedSync_[boardIndex] = true;
    resetBoardKeyState(boardIndex);

    for (uint8_t localKey = 0; localKey < kKeysPerBoard; ++localKey) {
        lastSentLevels_[localToGlobalKey(boardIndex, localKey)] = 0xFF;
    }
}