# ZYRA Hardware Configuration & Interface Guide

This document describes the hardware specifications, component descriptions, and pin connections for the **ZYRA Smart Voice Companion**.

---

## 🛠 Hardware Components List

The satellite hardware client consists of the following components:
1.  **Microcontroller:** ESP32-S3 DevKitC-1 (configured with 16MB Flash and PSRAM).
2.  **Microphone:** INMP441 Omnidirectional Microphone Module (I2S interface).
3.  **Audio Amplifier & Speaker:** MAX98357A I2S Mono DAC Module connected to a 4Ω 3W speaker.
4.  **Display:** 128x64 SH1106 or SSD1306 OLED Display (I2C interface).
5.  **Status Indicator:** Built-in WS2812B Addressable RGB LED (GPIO 48).
6.  **Push Button:** Physical BOOT button (GPIO 0).

---

## 🔌 Wiring Schematic & Pins Connection

The connections between the ESP32-S3 DevKitC-1 and the modules are detailed below:

### 1. INMP441 Microphone (I2S Port 0)
The INMP441 is a high-performance, low power, digital-output omnidirectional microphone with a bottom port. It outputs standard 24-bit I2S data.
- **SCK (Serial Clock):** Connect to **GPIO 5** on ESP32-S3.
- **WS (Word Select / Left-Right Clock):** Connect to **GPIO 4** on ESP32-S3.
- **SD (Serial Data Out):** Connect to **GPIO 6** on ESP32-S3.
- **L/R (Left/Right Select):** Connect to **GND** (for left channel mono).
- **VDD:** Connect to **3.3V**.
- **GND:** Connect to **GND**.

### 2. MAX98357A I2S Amplifier (I2S Port 1)
The MAX98357A is an easy-to-use digital audio transmitter that works with standard microcontrollers and single-board computers. It decodes the digital signal directly and drives the speaker.
- **LRC (Left-Right Clock):** Connect to **GPIO 16** on ESP32-S3.
- **BCLK (Bit Clock):** Connect to **GPIO 15** on ESP32-S3.
- **DIN (Data In):** Connect to **GPIO 7** on ESP32-S3.
- **GAIN:** Leave floating (default gain) or pull to GND for +9dB gain.
- **SD (Shutdown / Mode):** Leave floating or connect to VDD via resistor.
- **Vin:** Connect to **5V** (or 3.3V, but 5V provides higher loudness).
- **GND:** Connect to **GND**.

### 3. SH1106 / SSD1306 OLED Display (I2C)
A standard monochrome 128x64 display is used to show states and transcribed texts.
- **SDA (Data line):** Connect to **GPIO 8** on ESP32-S3.
- **SCL (Clock line):** Connect to **GPIO 9** on ESP32-S3.
- **VDD:** Connect to **3.3V**.
- **GND:** Connect to **GND**.
- **I2C Address:** Default is `0x3C`.

---

## 🔋 Power Setup
For stationary use, simply power the ESP32-S3 via the micro-USB/USB-C connection.
For future portable battery configuration:
1. Connect a 3.7V LiPo battery to a **TP4056** USB-C battery management charging board.
2. Feed the TP4056 output to a **5V step-up booster converter**.
3. Connect the booster output to the **5V pin** of the ESP32-S3 board.
