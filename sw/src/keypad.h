#ifndef KEYPAD_H
#define KEYPAD_H

#include <Arduino.h>

class Keypad {
public:
    static constexpr uint8_t kRows = 8;
    static constexpr uint8_t kCols = 16;
    static constexpr uint8_t kKeyCount = kRows * kCols;
    static constexpr uint8_t kBoardCount = 2;
    static constexpr uint8_t kBoardCols = 8;
    static constexpr uint8_t kKeysPerBoard = kRows * kBoardCols;

    bool begin();
    void scanKeypad();

    bool wasKeyPressed(uint8_t keyIndex) const;
    bool wasKeyReleased(uint8_t keyIndex) const;
    uint8_t getTotalKeys() const;

    void setPixelColor(uint8_t keyIndex, uint8_t level);
    void clearPixels();
    void update();

    bool isConnected() const;
    bool needsLedSync() const;

private:
    struct BoardConfig {
        uint8_t address;
        uint8_t xOffset;
    };

    static constexpr uint8_t kI2cAddrMin = 0x20;
    static constexpr uint8_t kI2cAddrMax = 0x2F;
    static constexpr uint8_t kI2cSdaPin = 4;
    static constexpr uint8_t kI2cSclPin = 5;
    static constexpr uint32_t kI2cSpeedHz = 100000;
    static constexpr uint8_t kReadBlockMax = 32;
    static constexpr uint8_t kPackedLevelBlockMax = 58;
    static constexpr uint8_t kDebounceSamples = 3;
    static constexpr uint8_t kStartupIgnoreFrames = 3;
    static constexpr uint32_t kStartupIgnoreMs = 200;
    static constexpr uint32_t kScanIntervalMs = 2000;
    static constexpr uint32_t kScanIntervalDisconnectedMs = 500;
    static constexpr uint8_t kProtocolVersion = 0x02;
    static constexpr uint8_t kMinimumBoardKeyCount = kKeysPerBoard;

    static constexpr uint8_t kRegProtocolVersion = 0x00;
    static constexpr uint8_t kRegDeviceAddress = 0x01;
    static constexpr uint8_t kRegKeyCountL = 0x02;
    static constexpr uint8_t kRegKeyCountH = 0x03;
    static constexpr uint8_t kRegLedClear = 0x12;
    static constexpr uint8_t kRegLedShow = 0x13;
    static constexpr uint8_t kRegLedSetLevelBlockPacked = 0x17;
    static constexpr uint8_t kRegKeyReadBlock = 0x21;
    static constexpr uint8_t kRetryCount = 3;

    static const BoardConfig kBoardConfigs[kBoardCount];

    uint32_t lastScanMs_ = 0;
    bool boardConnected_[kBoardCount] = {false};
    uint8_t warmupFrames_[kBoardCount] = {0};
    uint32_t warmupUntilMs_[kBoardCount] = {0};
    bool fullLedSync_[kBoardCount] = {true, true};

    bool stablePressed_[kKeyCount] = {false};
    bool candidatePressed_[kKeyCount] = {false};
    uint8_t candidateCount_[kKeyCount] = {0};
    bool keyPressedEdge_[kKeyCount] = {false};
    bool keyReleasedEdge_[kKeyCount] = {false};
    uint8_t ledLevels_[kKeyCount] = {0};
    uint8_t lastSentLevels_[kKeyCount] = {0};

    void refreshConnection(bool force);
    bool probeDevice(uint8_t address);
    bool readDeviceInfo(uint8_t address);
    bool readRegister(uint8_t address, uint8_t reg, uint8_t &value);
    bool readRegisters(uint8_t address, const uint8_t *request, size_t requestLength, uint8_t *response, size_t responseLength);
    bool writeCommand(uint8_t address, const uint8_t *data, size_t length);
    bool readKeyBlock(uint8_t address, uint8_t startKey, uint8_t count, uint8_t *outStates);
    bool clearAllLeds(uint8_t address);
    bool showLeds(uint8_t address);
    bool writePackedLevelBlock(uint8_t address, uint8_t startKey, uint8_t count, const uint8_t *levels);
    uint8_t localToGlobalKey(uint8_t boardIndex, uint8_t localKeyIndex) const;
    uint8_t globalToBoardIndex(uint8_t globalKeyIndex) const;
    uint8_t globalToLocalKey(uint8_t globalKeyIndex) const;
    void resetBoardKeyState(uint8_t boardIndex);
    void markBoardDisconnected(uint8_t boardIndex);
};

#endif