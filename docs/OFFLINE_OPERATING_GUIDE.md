# ZYRA Voice Assistant - Offline Operation Guide

Welcome to your offline voice assistant! This guide details how to set up, launch, and operate the ZYRA Voice Companion completely offline.

---

## 1. Prerequisites & System Setup

Ensure you have the following installed on your local development PC:
1.  **LM Studio:** Download and run [LM Studio](https://lmstudio.ai/). Load an OpenAI-compatible small model (e.g. `qwen2.5-7b-instruct`). Make sure the local server is started and running on port `1234`.
2.  **Piper TTS:** Download the Piper executable and voices. Set the paths in `backend/config.py` (default: `C:\piper\piper.exe` and `C:\piper\voices\en_US-lessac-medium.onnx`).
3.  **Python 3.13:** Installed with path configurations added to environment variables.
4.  **ESP-IDF v5.5.4:** The development environment required to compile and flash the ESP32-S3 firmware.

---

## 2. Launching the Backend Server

1.  Open terminal or PowerShell.
2.  Navigate to the ZYRA backend directory:
    ```bash
    cd c:\Users\VISHAV\OneDrive\Desktop\ZYRA\backend
    ```
3.  Activate the python virtual environment:
    ```powershell
    .\venv\Scripts\activate
    ```
4.  Launch the FastAPI server:
    ```bash
    python server.py
    ```
    The server console logs will report:
    `[INFO] [server] Initializing ZYRA Backend Application Lifespan...`

---

## 3. Operating the ZYRA Device

1.  **Power on the ZYRA Hardware:** Connect the ESP32-S3 DevKitC-1 development board via USB-C to a power source (your PC or a 5V charger).
2.  **Visual Feedback (OLED):**
    *   **Booting:** The screen displays `ZYRA` on line 1 and `Starting ZYRA...` on line 3.
    *   **WiFi Connection:** The screen transitions to `Connecting WiFi...`.
    *   **Ready:** Once connected, the screen displays `Ready. Say: 'Hey Zyra' or press BOOT`. The speaker plays the voice prompt: *"Hello, I am Zyra."*
3.  **Wake Word:**
    *   Say `"Hey Zyra"` to trigger the wake-word engine.
    *   The assistant chime sounds and the display transitions to the siri-style dynamic wave representation: `Listening...`
4.  **Hold-to-Talk (Manual Trigger):**
    *   Press and hold the physical **BOOT button (GPIO 0)** on the ESP32-S3 board.
    *   Speak your question clearly into the microphone.
    *   Release the BOOT button when you are finished speaking.
5.  **Processing (Thinking):**
    *   The OLED screen shows a circular loader dot animation and says `Thinking...`.
    *   The backend transcribes, queries the local LLM, and synthesizes response speech.
6.  **Response (Speaking):**
    *   The speaker will play back the assistant response voice dynamically.
    *   The text of the assistant response scrolls on the screen alongside a bouncing equalizer bar animation in the top-right corner.
    *   Once playback finishes, the screen transitions back to the `Ready` state.
