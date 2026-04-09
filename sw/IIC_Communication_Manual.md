# 8x8-Pad IIC Communication Manual

## 1. Scope

This document defines the IIC (I2C) slave protocol for the 8x8-Pad device.
It is intended for host-side firmware developers who need to control LEDs and read key states from one or more 8x8-Pad devices on the same IIC bus.

Protocol version `0x02` adds level-based LED bulk transfer, internal level-to-RGB mapping, and buffered LED flush semantics so high LED traffic does not directly stall key scanning.

## 2. Hardware and Bus Basics

- Device role: IIC slave
- Address range: `0x20` to `0x2F`
- Matrix size: `16 x 8`
- Total keys/LEDs: `128`
- Key index range: `0..127`
- IIC pins:
  - `SDA = GPIO4`
  - `SCL = GPIO5`

### 2.1 Address Select Pins

The device reads 4 address-select pins at boot:

- `GPIO8` -> bit0
- `GPIO9` -> bit1
- `GPIO10` -> bit2
- `GPIO11` -> bit3

Pin mode is `INPUT_PULLUP`.

- Floating pin -> logic `1`
- GND connected -> logic `0`

Address calculation:

`IIC_ADDR = 0x20 + ((b3 << 3) | (b2 << 2) | (b1 << 1) | b0)`

Where:

- `b0 = GPIO8`
- `b1 = GPIO9`
- `b2 = GPIO10`
- `b3 = GPIO11`

Examples:

- `GPIO11..8 = 1111` -> `0x2F`
- `GPIO11..8 = 0000` -> `0x20`
- `GPIO11..8 = 0101` -> `0x25`

## 3. Protocol Model

The protocol is register-based.

- The first byte written by master is always register address (`reg`).
- Optional payload bytes follow based on register definition.
- For read-type registers, the master writes `reg` (and optional params), then reads response bytes.

### 3.1 LED Update Model

Starting from protocol `0x02`, LED writes update a target buffer first.

- `set` registers only modify the target LED buffer.
- `show` requests the firmware to flush the target buffer on the next LED refresh timeslice.
- The firmware does not call the LED hardware driver for every I2C write.
- Key scan and LED refresh run on separate schedules inside the device firmware.

This is the main change that removes the previous contention between large LED writes and key scanning.

## 4. Register Map

### 4.1 Device Info Registers (Read)

- `0x00` `REG_PROTOCOL_VERSION`
  - Read length: 1
  - Value: protocol version, currently `0x02`

- `0x01` `REG_DEVICE_ADDRESS`
  - Read length: 1
  - Value: current slave address (`0x20..0x2F`)

- `0x02` `REG_KEY_COUNT_L`
  - Read length: 1
  - Value: low byte of key count (`128 -> 0x80`)

- `0x03` `REG_KEY_COUNT_H`
  - Read length: 1
  - Value: high byte of key count (`128 -> 0x00`)

### 4.2 Legacy LED Compatibility Registers (Write)

These commands are retained for backward compatibility only.

- `0x10` `REG_LED_SET_PIXEL`
  - Write format: `[0x10, keyIndex, R, G, B]`
  - Function: compatibility path, internally quantized to one local brightness level
  - Note: color is not preserved; the device uses the brightest RGB channel and maps it to local level `0..15`

- `0x11` `REG_LED_SET_PIXEL_SHOW`
  - Write format: `[0x11, keyIndex, R, G, B]`
  - Function: compatibility path, quantize to local brightness level and request a buffered flush

- `0x12` `REG_LED_CLEAR`
  - Write format: `[0x12]`
  - Function: clear target LED buffer, no immediate hardware refresh

- `0x13` `REG_LED_SHOW`
  - Write format: `[0x13]`
  - Function: request LED flush on next LED refresh timeslice

### 4.3 Preferred LED Level Registers (Write)

All preferred LED writes use brightness `level` instead of raw RGB.

- Level range: `0..15`
- Payload size: `1 byte per LED` for unpacked mode, `2 LEDs per byte` for packed mode
- Level-to-RGB mapping is implemented inside firmware

- `0x14` `REG_LED_SET_LEVEL`
  - Write format: `[0x14, keyIndex, level]`
  - Function: set one LED target level, no immediate flush

- `0x15` `REG_LED_SET_LEVEL_SHOW`
  - Write format: `[0x15, keyIndex, level]`
  - Function: set one LED target level and request a buffered flush

- `0x16` `REG_LED_SET_LEVEL_BLOCK`
  - Write format: `[0x16, startKey, count, level0, level1, ...]`
  - Function: write a continuous LED level range into target buffer
  - `count` maximum for broad master compatibility: `29`
  - Host should follow with `REG_LED_SHOW` after one or more writes

- `0x17` `REG_LED_SET_LEVEL_BLOCK_PACKED`
  - Write format: `[0x17, startKey, count, packed0, packed1, ...]`
  - Function: write a continuous LED level range using packed 4-bit levels
  - Packed order: high nibble first, low nibble second
  - Example: one payload byte `0xAB` means first LED level `A`, second LED level `B`
  - `count` maximum for broad master compatibility: `58`
  - Recommended default bulk write command

- `0x18` `REG_LED_SET_8X8_BLOCK`
  - Write format: `[0x18, blockIndex, packed32bytes...]`
  - Function: write exactly one `8 x 8` block using 32 packed bytes
  - `blockIndex = 0` -> keys `0..63` -> columns `0..7`
  - `blockIndex = 1` -> keys `64..127` -> columns `8..15`
  - Host should follow with `REG_LED_SHOW`
  - Note: total I2C write length is `34 bytes`, so the master stack must support writes larger than 32 bytes

- `0x19` `REG_LED_SET_FULL_FRAME`
  - Write format: `[0x19, packed64bytes...]`
  - Function: write the full `16 x 8` frame using 64 packed bytes
  - Host should follow with `REG_LED_SHOW`
  - Note: total I2C write length is `65 bytes`, so the master stack must support writes larger than 65 bytes

### 4.4 Key Read Registers

- `0x20` `REG_KEY_READ_ONE`
  - Write format: `[0x20, keyIndex]`
  - Read length: 1
  - Return: `0x00` not pressed, `0x01` pressed

- `0x21` `REG_KEY_READ_BLOCK`
  - Write format: `[0x21, startKey, count]`
  - Read length: `count`
  - Return: `count` bytes, each byte is `0x00/0x01`
  - `count` maximum is `32`
  - If key index exceeds `127`, returned value is `0x00`

## 5. Key and LED Index Mapping

Use this mapping for `(x, y)` to key index:

`keyIndex = x * 8 + y`

- `x` range: `0..15` (column)
- `y` range: `0..7` (row)

Examples:

- `(0, 0)` -> `0`
- `(0, 7)` -> `7`
- `(1, 0)` -> `8`
- `(15, 7)` -> `127`

This index order is also the LED buffer order.

- Continuous bulk writes always follow increasing `keyIndex`
- `8 x 8` block `0` is the first 8 columns in the same order
- `8 x 8` block `1` is the second 8 columns in the same order

## 6. LED Level Mapping

The host now sends only `level`, not RGB.

The device converts level to RGB locally using the following grayscale lookup table:

| Level | RGB |
| --- | --- |
| 0 | `(0, 0, 0)` |
| 1 | `(4, 4, 4)` |
| 2 | `(12, 12, 12)` |
| 3 | `(24, 24, 24)` |
| 4 | `(36, 36, 36)` |
| 5 | `(48, 48, 48)` |
| 6 | `(60, 60, 60)` |
| 7 | `(72, 72, 72)` |
| 8 | `(84, 84, 84)` |
| 9 | `(118, 118, 118)` |
| 10 | `(140, 140, 140)` |
| 11 | `(160, 160, 160)` |
| 12 | `(182, 182, 182)` |
| 13 | `(206, 206, 206)` |
| 14 | `(230, 230, 230)` |
| 15 | `(255, 255, 255)` |

## 7. Typical Transactions

### 7.1 Read Protocol Version

1. Master write: `[0x00]`
2. Master read 1 byte -> expected `0x02`

### 7.2 Read Device Address

1. Master write: `[0x01]`
2. Master read 1 byte -> expected `0x20..0x2F`

### 7.3 Set One LED to Full Brightness and Show

1. Master write: `[0x15, 0x00, 0x0F]`
2. Optional additional writes
3. Master write: `[0x13]`

### 7.4 Write a Continuous LED Range

Write levels for keys `16..23`:

1. Master write: `[0x16, 0x10, 0x08, l0, l1, l2, l3, l4, l5, l6, l7]`
2. Master write: `[0x13]`

### 7.5 Write a Packed LED Range

Write four LEDs from key `0` using levels `1, 2, 3, 4`:

1. Master write: `[0x17, 0x00, 0x04, 0x12, 0x34]`
2. Master write: `[0x13]`

### 7.6 Write One 8x8 Block

1. Master write: `[0x18, 0x00, packed32bytes...]`
2. Master write: `[0x13]`

### 7.7 Write Full Frame

1. Master write: `[0x19, packed64bytes...]`
2. Master write: `[0x13]`

### 7.8 Read key state at `(x, y)`

Assume `(x, y) = (3, 2)`:

- `keyIndex = 3 * 8 + 2 = 26 (0x1A)`

Transaction:

1. Master write: `[0x20, 0x1A]`
2. Master read 1 byte:
   - `0x01`: pressed
   - `0x00`: released

### 7.9 Read continuous key states

Read keys `16..23` (8 keys):

1. Master write: `[0x21, 0x10, 0x08]`
2. Master read 8 bytes

## 8. New vs Legacy LED Protocol

| Item | Legacy RGB | Preferred Level Bulk |
| --- | --- | --- |
| Host payload per LED | 3 bytes RGB | 4-bit or 8-bit level field |
| Best use case | Old host compatibility | Normal runtime updates |
| I2C transaction count | High | Low |
| Flush behavior | Often triggered too frequently in old hosts | Explicit buffered flush |
| Device mapping | Host decides RGB | Device maps level to RGB locally |
| Recommended now | Deprecated | Yes |

Recommended host strategy:

1. Fill one or more LED ranges with `0x17` packed writes.
2. Send one `0x13` show command after the batch.
3. Use `0x18` or `0x19` only if the master stack supports larger write buffers.

## 9. Multi-Device Recommendations

### 9.1 Boot Scan and Identity Check

At startup, host should scan addresses `0x20..0x2F`:

1. Probe ACK.
2. For each online address, read `0x00`, `0x01`, `0x02`, `0x03`.
3. Build device table from responses.

This confirms each device identity and avoids wrong-target writes.

### 9.2 Avoid Address Conflict

Ensure each board has unique `GPIO8..11` strap combination.
If two devices share the same address, communication is undefined.

### 9.3 Error Handling

For every write/read transaction:

- Check transmit result (`ACK/NACK`).
- Check returned byte count equals expected.
- Use retry (2-3 times) on failure.
- Use timeout to prevent blocking.

Default recommendation remains `100 kHz` until protocol batching is already in place and verified.

## 10. Optional I2C Frequency Upgrade

The firmware changes in protocol `0x02` are intended to solve throughput problems before changing bus speed.

Recommended order:

1. First switch the host to level-based packed block writes.
2. Then verify LED flush cadence and key scan stability.
3. Only after that evaluate higher I2C speed.

Optional upgrade target:

- `400 kHz` Fast-mode can be considered if wiring is short and signal quality is clean.

Check these conditions before raising speed:

- Short cable length
- Proper pull-up resistor value
- No repeated NACK or corrupted frames under burst traffic
- The master implementation can sustain the chosen speed reliably

Do not assume Fast-mode Plus support. If the bus is long or marginal, higher speed can increase retry count and make real throughput worse.

## 11. Host Example (Arduino Wire)

```cpp
#include <Wire.h>

static const uint8_t DEV = 0x20;

bool writePacket(const uint8_t *data, uint8_t len, bool sendStop = true) {
  Wire.beginTransmission(DEV);
  for (uint8_t i = 0; i < len; i++) {
    Wire.write(data[i]);
  }
  return Wire.endTransmission(sendStop) == 0;
}

bool readReg1(uint8_t reg, uint8_t &value) {
  if (!writePacket(&reg, 1, false)) {
    return false;
  }
  uint8_t n = Wire.requestFrom((int)DEV, 1);
  if (n != 1) {
    return false;
  }
  value = Wire.read();
  return true;
}

bool showFrame() {
  const uint8_t cmd = 0x13;
  return writePacket(&cmd, 1, true);
}

bool writePackedRange(uint8_t startKey, uint8_t count, const uint8_t *packed) {
  const uint8_t packedBytes = (uint8_t)((count + 1) / 2);
  uint8_t cmd[32] = {0};

  if (count == 0 || count > 58) {
    return false;
  }

  cmd[0] = 0x17;
  cmd[1] = startKey;
  cmd[2] = count;
  for (uint8_t i = 0; i < packedBytes; i++) {
    cmd[3 + i] = packed[i];
  }
  return writePacket(cmd, (uint8_t)(3 + packedBytes), true);
}

bool readKeyXY(uint8_t x, uint8_t y, uint8_t &pressed) {
  if (x > 15 || y > 7) {
    return false;
  }

  uint8_t keyIndex = (uint8_t)(x * 8 + y);
  uint8_t cmd[2] = {0x20, keyIndex};

  if (!writePacket(cmd, 2, false)) {
    return false;
  }

  uint8_t n = Wire.requestFrom((int)DEV, 1);
  if (n != 1) {
    return false;
  }

  pressed = Wire.read() ? 1 : 0;
  return true;
}
```

## 12. Validation Checklist

1. Single LED:
   - Write `[0x14, key, level]`, then `[0x13]`
   - Confirm only that LED changes and the key read path still works

2. Continuous block write:
   - Write one or more `0x17` packed blocks
   - Confirm all keys in the range update after one `0x13`

3. Full-screen update:
   - Repeatedly write `0x18` or `0x19`
   - Confirm no LED remains stuck at an old level after the next flush

4. Key stability under LED load:
   - While repeatedly sending large LED updates, read `0x20` or `0x21`
   - Confirm the pressed state remains stable and debounce behavior does not visibly collapse

## 13. Versioning

- Current protocol version: `0x02`
- `0x01`: RGB-per-pixel legacy protocol
- `0x02`: level-based buffered bulk LED protocol with decoupled LED flush scheduling