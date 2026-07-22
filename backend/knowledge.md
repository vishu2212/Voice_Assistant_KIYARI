# JARVIS AI Voice Assistant

JARVIS is a modular, general-purpose AI-powered Voice Assistant designed for IoT, robotics, embedded AI, and smart home applications.

## Technical Specifications
- **Hardware Platform:** ESP32-S3 DevKitC-1 with INMP441 I2S Microphone, MAX98357A I2S Amplifier, and SSD1306 OLED Display.
- **Communication Protocol:** WebSockets transmitting 16kHz Mono 16-bit PCM audio and JSON control events.
- **Backend Stack:** Python, FastAPI, Faster-Whisper (Speech-to-Text), local LLM (OpenAI-compatible / LM Studio), and Piper (Text-to-Speech).

## User Operations
- **Wake Word:** Say "Hey Jarvis" to wake the assistant.
- **Hold-to-Talk:** Press and hold the BOOT button (GPIO 0) on the ESP32-S3 board to speak.
- **States:**
- **Ready:** "Ready. Say: 'Hey Jarvis'" (or press the button).
  - **Listening:** Waves appear on the display, showing audio capture.
  - **Thinking:** Bouncing dot animation while processing.
  - **Speaking:** Graphic equalizer and response text scrolling.
