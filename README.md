# Open Grid

Open Grid 是一款开源 8x16 Grid 控制器，用于音乐创作、现场演出、声音实验和交互装置。它以 Raspberry Pi Pico 为主控，通过 USB 模拟 monome grid 兼容设备，并将 128 个按键输入与 LED 亮度控制桥接到两块 8x8 I2C 按键/灯板。

项目包含可直接编译的 PlatformIO 固件、3D 打印外壳 STEP 文件，以及实物装配参考图。你可以把它作为一个完整的 DIY 控制器，也可以把它改造成自己的 sequencer、视觉控制器、媒体装置输入面板或 monome 生态兼容硬件。

![Open Grid 正面](pics/front.jpg)

## 特性

- 8x16 矩阵布局，共 128 个可独立读取的按键。
- 每个按键支持 0-15 级 LED 亮度控制。
- USB 串口协议兼容 monome grid 的基础输入与 LED 控制流程。
- 基于 Raspberry Pi Pico、Arduino Core 和 Adafruit TinyUSB。
- 两块 8x8 I2C 子板组合成完整 16 列 Grid。
- 支持按键消抖、热插拔检测、LED 全量同步和增量刷新。
- 提供 3D 打印外壳文件，方便复刻与二次设计。

## 产品外观

正面采用 16 列、8 行的经典 Grid 布局，适合步进音序、clip 触发、参数映射和模式切换。

![Open Grid 侧面](pics/side.jpg)

外壳为低矮桌面式结构，侧面预留 USB 接口，适合放在合成器、控制器或电脑旁长期使用。

![Open Grid 与 Shield Pro 联动](pics/OpenGridAndShieldPro.jpg)

Open Grid 可以与其他硬件控制器搭配使用。上图展示了它与 Shield Pro 同桌面联动时的状态，适合构建更完整的现场演出控制台。

## 硬件结构

![Open Grid 内部安装](pics/installation.jpg)

硬件由以下部分组成：

- Raspberry Pi Pico 主控板。
- 两块 8x8 I2C 按键/LED 子板。
- 3D 打印上盖与底壳。
- USB 接口与内部连接线。

固件默认使用 Pico 的 I2C0：

- SDA: GPIO 4
- SCL: GPIO 5
- I2C 速度: 100 kHz
- 左侧子板地址: `0x2E`
- 右侧子板地址: `0x2D`

## 仓库结构

```text
.
├── enclosure/              # 3D 外壳 STEP 文件
│   ├── OpenGrid-Bot.step
│   └── OpenGrid-Top.step
├── pics/                   # README 与装配参考图片
└── sw/                     # PlatformIO 固件工程
    ├── platformio.ini
    └── src/
```

## 固件构建与烧录

固件使用 PlatformIO 构建，目标板为 Raspberry Pi Pico。

1. 安装 VS Code 和 PlatformIO 插件。
2. 打开仓库中的 `sw` 目录。
3. 连接 Raspberry Pi Pico。
4. 构建并上传固件：

```bash
cd sw
pio run -t upload
```

串口监视器默认波特率为 `115200`：

```bash
pio device monitor -b 115200
```

启动成功后，串口会输出：

```text
[USB] monome grid bridge ready
```

## 软件兼容性

Open Grid 在 USB 侧使用 monome 风格的串口设备描述，并以 `OpenGrid` 作为设备 ID。固件实现了 Grid 尺寸查询、设备 ID、按键事件上报、LED 单点/区域/整屏控制等常用命令，适合接入支持 monome grid 的音乐软件、脚本或自定义工具链。

当前固件目标是提供稳定的 8x16 Grid 交互基础。如果你的软件依赖更完整的 monome 协议行为，建议根据目标应用补充测试并扩展 `sw/src/MonomeSerialDevice.cpp`。

## 外壳与装配

外壳 STEP 文件位于 `enclosure/`：

- `OpenGrid-Top.step`: 上盖。
- `OpenGrid-Bot.step`: 底壳。

装配建议：

- 打印前根据自己的按键帽、PCB 厚度和螺丝规格检查公差。
- 固定 PCB 前先确认 USB 接口与外壳开孔对齐。
- I2C 线建议尽量短，并保证 SDA、SCL、5V、GND 连接可靠。
- 首次上电时先打开串口监视器，确认两块子板都被识别。

## 适合做什么

- 音序器与节奏编排控制器。
- Max/MSP、norns、SuperCollider、Pure Data 等创作环境的 Grid 输入面板。
- 灯光、视觉、媒体装置控制台。
- monome 协议学习与开源硬件实验。
- 自定义 MIDI/OSC/USB 控制器的硬件基础。

## 开源说明

本仓库提供固件源码、外壳模型和装配图片，方便学习、复刻和二次开发。若你计划发布改版硬件或商业化使用，请先根据项目后续补充的许可证文件确认授权范围。

欢迎提交 issue、改进固件、补充装配文档，或分享你的 Open Grid 改装版本。
