# KIYARI Smart Voice Companion - Project & Technical Guide

Welcome to the technical guide for your **KIYARI Smart Voice Companion**! This document provides a complete breakdown of the project architecture, frameworks, and drivers used to build it, along with a blueprint of future enhancements to make it fully portable without a laptop or Wi-Fi.

---

## 1. Project Overview & Architectural Design

The KIYARI Voice Assistant is a completely offline, low-latency voice system designed to act as the rule referee, card helper, and interactive guide for the custom strategy board game **KIYARI**. 

### System Topology
*   **Hardware Client (Satellite):** ESP32-S3 DevKitC-1 microcontroller equipped with an INMP441 I2S Microphone, a MAX98357A I2S Audio Amplifier & Speaker, and a 128x64 SSD1306 OLED Screen.
*   **AI Backend (Brain):** A local FastAPI Python application hosting local Speech-to-Text (Whisper), a local Large Language Model (Qwen 2.5), and local Speech Synthesis (Piper TTS).
*   **Communication Pipeline:** Real-time WebSockets over a local Wi-Fi connection, transmitting JSON control frames and binary 16kHz PCM16 audio packets.

---

## 2. Custom Firmware Framework & Drivers

The firmware was compiled using **ESP-IDF v5.5.4** (Espressif IoT Development Framework) in C. It relies on the **FreeRTOS** real-time kernel to manage multitasking and priority queue operations.

### Key Drivers & Protocols Implemented:
1.  **I2S (Inter-IC Sound) Protocol:**
    *   **Audio Capture (RX):** Handles standard digital audio capture from the **INMP441 microphone** at a 16kHz sample rate, 16-bit depth, mono channel configuration.
    *   **Audio Playback (TX):** Sends incoming PCM audio bytes received from the WebSocket directly to the **MAX98357A I2S DAC** to drive the analog speaker.
2.  **I2C (Inter-Integrated Circuit):**
    *   Interfaces with the **SSD1306 OLED Display** to render text-based state notifications ('Connected', 'Ready', 'Listening...', 'Thinking...', 'Speaking...').
3.  **GPIO & Button Handlers:**
    *   Monitors the physical **BOOT button (GPIO 0)** using active-low polling to detect when the user presses and holds the button to start/stop audio recording.
4.  **Persistent WebSocket Client:**
    *   Uses Espressif's standard WebSocket client library (`esp_websocket_client`) to establish a persistent TCP pipe. It handles full-duplex communication: streaming raw voice bytes up to the server and downloading speech audio chunks down.

---

## 3. Offline AI Backend Pipeline

The backend is built in **Python 3.13** using **FastAPI** and runs entirely offline on your local PC:

1.  **FastAPI WebSockets:** Manages connection states and handles simultaneous string events (`"start"`, `"stop"`, `"listening"`, `"thinking"`, `"done"`) and binary data streams.
2.  **Faster-Whisper (Speech-to-Text):** Runs locally on CPU/GPU to transcribe captured crop query files. It automatically detects the language (Hindi, English, or Hinglish).
3.  **Qwen 2.5 7B Instruct (Large Language Model):** Run locally through LM Studio. The model is injected with a system instruction prompt containing rules extracted from your *Kiyari manual.pdf* and *1.pdf* files.
4.  **Piper TTS (Text-to-Speech):** A high-performance offline voice generator. It checks the language of the LLM's response and dynamically switches between:
    *   **Hindi Output:** Native Pratham voice (`hi_IN-pratham-medium.onnx`).
    *   **English Output:** Lessac voice (`en_US-lessac-medium.onnx`).
5.  **Digital Volume Scaling:** Intercepts volume requests (e.g. *"set volume to 50%"*). It scales the raw 16-bit PCM signal amplitudes (multiplier `0.1` to `1.2`) and applies digital clipping protection (`np.clip`) before sending audio to the board.

---

## 4. Future Scope: Complete Portability (No Laptop or Wi-Fi)

To make the KIYARI assistant completely portable—a pocket-sized standalone device that requires no laptop, external router, or Wi-Fi—the following upgrades can be implemented:

### A. On-Chip Inference (Voice Command Engine)
*   Instead of sending audio to a server for speech-to-text, you can use the **ESP-Skainet** framework (Espressif's Intelligent Voice Assistant) or **TensorFlow Lite Micro** directly on the ESP32-S3.
*   This allows the chip to recognize local commands (like *"Next player"*, *"Reset rules"*, *"Volume up"*) directly on-chip with zero latency and zero connection requirements.

### B. Hardware Neural Accelerator (Stand-alone AI)
*   For running full conversational AI (LLM and TTS) in a pocket toy, you can attach a co-processor module like a **Raspberry Pi Compute Module 4 (CM4)** or a **Kendryte K210/K510** AI chip inside the enclosure.
*   This enables running highly quantized small-scale LLMs (such as a 1B parameter model) and local TTS directly inside the physical game unit.

### C. Access Point (AP) Hotspot Mode
*   Configure the ESP32-S3 firmware to act as a **Wi-Fi Access Point**. The board will broadcast its own Wi-Fi SSID (e.g. *"Kiyari-Companion"*).
*   Your smartphone can connect to this network and host the backend server using an Android/iOS Python runner app (like Termux or Pyto). This eliminates the need for a laptop or external Wi-Fi router entirely.

### D. SPI MicroSD Card Integration
*   Attach a MicroSD card reader to the ESP32-S3 via SPI.
*   Instead of generating TTS speech dynamically, you can pre-render thousands of common assistant voice replies as `.wav` files and store them on the SD card. The ESP32 can play them back directly from the card based on quick logic triggers, bypassing TTS generation entirely.

### E. Battery Power Management
*   Add a **3.7V Lithium-Polymer (LiPo)** battery inside the enclosure.
*   Connect a **TP4056** USB-C charging board and a **5V step-up boost converter** to supply clean power directly to the ESP32-S3 development board, making it completely cordless and portable.
