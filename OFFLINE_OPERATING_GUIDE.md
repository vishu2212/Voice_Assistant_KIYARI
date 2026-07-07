# KIYARI Voice Assistant - Offline Operation Guide

Welcome to your offline voice assistant! This guide details how to set up, launch, and operate the KIYARI Voice Companion completely offline tomorrow.

---

## 1. Quick Start Steps (What to do Tomorrow)

1. **Start the local LLM Server (LM Studio):**
   * Open LM Studio on your PC.
   * Make sure the model **`qwen2.5-7b-instruct`** is loaded and the local server is started on port `1234`.
2. **Connect to Wi-Fi:**
   * Connect your PC to the Wi-Fi network named **`Lucifer`**.
3. **Open a terminal (PowerShell) on your PC and run the backend:**
   ```powershell
   cd c:\Users\VISHAV\OneDrive\Desktop\KIYARI\backend
   .\venv\Scripts\python.exe server.py
   ```
4. **Power on the KIYARI Hardware:**
   * Plug in the USB cable to power the ESP32-S3 board.
   * The board will boot up, connect to the **`Lucifer`** Wi-Fi, and establish a WebSocket connection to your PC.
5. **Ready State:**
   * The OLED screen on the ESP32 will display:
     ```text
     Connected
     Ready
     Press BOOT to speak
     ```

---

## 2. Operating the Voice Assistant

### Speaking to KIYARI (Hold-to-Talk)
1. **Press and hold the BOOT button** (GPIO 0, the button next to the USB port) on the ESP32 board.
2. **Speak your question** clearly into the microphone (you can speak in English, Hindi, or a bilingual Hinglish mix).
3. **Release the BOOT button** immediately after you finish speaking.
4. **Processing & Playback:**
   * The screen will change to **`Thinking...`** as the server transcribes and processes your query.
   * KIYARI will then play back the response dynamically in a natural Indian accent!

### Voice-Controlled Volume
You can change the speaker volume simply by speaking to the assistant! Examples:
* **To set an absolute level:** *"Set volume to 50%"* or *"Volume 80"* (Accepts any value from `10%` to `120%`).
* **To increase volume:** *"Volume up"* or *"Make it louder"* (Increases the volume by 20%).
* **To decrease volume:** *"Volume down"* or *"Quieter"* (Decreases the volume by 20%).

*Note: The backend adjusts the volume digitally using 16-bit PCM scaling and applies clipping protection to keep the output clear.*

---

## 3. Bilingual Speech & TTS Voice Models

* **Multilingual Whisper:** Whisper automatically detects whether you are speaking English or Hindi.
* **Bilingual LLM prompt:** The LLM is configured to reply in Hindi (using Devanagari script) for Hindi/Hinglish questions, and in English for English questions.
* **Dynamic TTS Switch:** 
  * **Hindi:** Uses the native `hi_IN-pratham-medium.onnx` voice model.
  * **English:** Uses the `en_US-lessac-medium.onnx` voice model.

---

## 4. Troubleshooting (Offline Checks)

* **Screen shows "Error occurred" or doesn't connect:**
  * Press the **RST** button on the ESP32 board to reboot it, or unplug/replug the USB cable. It will connect automatically.
* **Port 8001 is already in use:**
  * If the server fails to start because port 8001 is busy, close any other terminals running the server, or force-kill it in PowerShell:
    ```powershell
    Stop-Process -Id (Get-NetTCPConnection -LocalPort 8001).OwningProcess -Force
    ```
* **No Voice Response / Infinite "Thinking...":**
  * Check that LM Studio is open on your PC and the local server is running at `http://127.0.0.1:1234`.
