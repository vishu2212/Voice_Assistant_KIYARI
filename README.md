# KIYARI Project Repository

This repository hosts the code, firmware, and assets for **KIYARI**, a fully offline voice assistant project utilizing an ESP32-S3 and local server backends.

## 📂 Repository Structure

```text
KIYARI/
│
├── firmware/
│   ├── esphome/        # ESPHome YAML configurations and startup sounds
│   │   ├── kiyari.yaml
│   │   └── sounds/     # local startup/alert wav files
│   └── kiyari-esp32/   # ESP-IDF firmware modular C codebase
│       ├── main/       # Application bootloader and state manager
│       │   ├── app_main.c
│       │   ├── app_main.h
│       │   ├── config.h
│       │   └── app_state.h
│       ├── components/ # Custom modular hardware components
│       │   ├── microphone/ # INMP441 standard I2S recorder driver
│       │   ├── speaker/    # MAX98357A standard I2S playback driver
│       │   ├── display/    # SSD1306 standard I2C OLED driver
│       │   ├── network/    # Station Wi-Fi manager controller
│       │   ├── api/        # PCM buffer and WAV formatting compiler
│       │   ├── assistant/  # HTTP request and JSON parsing manager
│       │   ├── storage/    # NVS flash flash initialization component
│       │   ├── button/     # GPIO push-button listener component
│       │   └── led/        # Status LED indicator driver component
│       ├── assets/     # Local static assets
│       ├── managed_components/ # External registered library packages
│       └── sdkconfig.defaults # Default build configs
│
│
├── backend/            # FastAPI Offline Speech/LLM/TTS Pipeline
│   ├── server.py
│   ├── config.py
│   ├── requirements.txt
│   ├── routes/
│   ├── services/
│   ├── utils/
│   ├── temp/
│   └── venv/
│
├── models/             # Directory to host local model weights
│   ├── whisper/        # faster-whisper models
│   └── piper/          # piper voice models
│
├── audio/              # Reference audio files / captures
│   ├── startup/        # system startup chime/alert
│   ├── listening/      # trigger/listening chime
│   ├── thinking/       # processing query sound
│   ├── success/        # positive transaction sound
│   └── error/          # alert/connection error sound
│
├── docs/               # Technical designs and documentations
└── README.md           # Main documentation file
```

---

## 🛠 Prerequisites & Installation

### 1. Python Environment
This project requires Python 3.11+.

Change directory to the `backend` folder, set up a virtual environment, and install dependencies:
```bash
cd backend
python -m venv venv
.\venv\Scripts\activate     # Windows
source venv/bin/activate   # Linux/macOS
pip install -r requirements.txt
```

### 2. LM Studio Setup
1. Download and install [LM Studio](https://lmstudio.ai/).
2. Download a compatible model, recommended: `qwen2.5-7b-instruct`.
3. Go to the **Local Server** tab (developer icon on the left panel).
4. Set port to `1234` and start the server.
5. Verify the server is running at `http://127.0.0.1:1234/v1`.

### 3. Piper TTS Setup
1. Download the precompiled Piper standalone binary for your OS from the [Piper GitHub Releases](https://github.com/rhasspy/piper/releases).
2. Extract the archive (e.g., to `C:\piper\`).
3. Download an ONNX voice model and its `.json` config file (e.g., `en_US-lessac-medium.onnx` and `en_US-lessac-medium.onnx.json`) from the [Piper Voice list](https://github.com/rhasspy/piper/releases/download/v0.0.2/voice-en_US-lessac-medium.tar.gz).
4. Update the paths in `backend/config.py` to point to your `piper.exe` and voice `.onnx` model files:
   ```python
   PIPER_EXECUTABLE = "C:\\piper\\piper.exe"
   PIPER_VOICE_MODEL = "C:\\piper\\voices\\en_US-lessac-medium.onnx"
   ```

---

## 🚀 Running the Server

Make sure you are in the `backend` directory, activate the virtual environment, and start the FastAPI application:
```bash
cd backend
python server.py
```
Or run via Uvicorn:
```bash
uvicorn server:app --host 0.0.0.0 --port 8000
```

---

## 📡 API Reference & Examples

### 1. Health Check
*   **Endpoint:** `GET /health`
*   **Description:** Verifies that the server is online.
*   **Example curl:**
    ```bash
    curl http://localhost:8000/health
    ```
*   **Response:**
    ```json
    {"status": "ok"}
    ```

### 2. Available Models
*   **Endpoint:** `GET /models`
*   **Description:** Queries and returns the models currently running in LM Studio.
*   **Example curl:**
    ```bash
    curl http://localhost:8000/models
    ```
*   **Response:**
    ```json
    {"models": ["qwen2.5-7b-instruct"]}
    ```

### 3. Chat Pipeline (Voice-to-Voice)
*   **Endpoint:** `POST /chat`
*   **Description:** Uploads a captured audio file, transcribes it, runs the LLM, synthesizes the response, and streams back the generated `.wav` audio.
*   **Input parameters (multipart/form-data):**
    *   `audio`: Audio binary file (WAV or raw PCM).
    *   `session_id` (optional): Unique string identifier to maintain individual chat history state.
*   **Example curl:**
    ```bash
    curl -F "audio=@test_input.wav" -F "session_id=user_1" --output response.wav http://localhost:8000/chat
    ```
*   **Response:** Binary audio stream (content-type: `audio/wav`).

### 4. Transcribe Only (Speech-to-Text)
*   **Endpoint:** `POST /transcribe`
*   **Description:** Transcribes audio and returns the text payload only.
*   **Example curl:**
    ```bash
    curl -F "audio=@test_input.wav" http://localhost:8000/transcribe
    ```
*   **Response:**
    ```json
    {"text": "Hello, how are you today?"}
    ```

### 5. Speak (Text-to-Speech)
*   **Endpoint:** `POST /speak`
*   **Description:** Synthesizes the provided text into a downloadable WAV file.
*   **Example curl:**
    ```bash
    curl -X POST -H "Content-Type: application/json" -d "{\"text\":\"Hello, I am KIYARI\"}" --output speech.wav http://localhost:8000/speak
    ```
*   **Response:** Binary audio stream (content-type: `audio/wav`).

---

## 🔌 ESPHome / ESP32 Integration

To hook your ESP32-S3 microphone and speaker up to the backend, utilize ESPHome's voice assistant or HTTP request component actions.

### Example ESPHome YAML logic:

```yaml
# HTTP request to send captured audio and play output
http_request:
  timeout: 30s

# Triggered when button is pressed and audio is recorded
on_button_press:
  then:
    - http_request.post:
        url: !lambda |-
          return "http://YOUR_SERVER_IP:8000/chat";
        headers:
          Content-Type: "multipart/form-data"
        files:
          - name: "audio"
            filename: "capture.wav"
            # Captures buffer from mic
            buffer: mic_buffer
        # Target action when WAV is returned
        on_response:
          then:
            - media_player.play_media:
                id: kiyari_player
                media_url: !lambda |-
                  return response.body_url;
```

---

## 🛡 System Design & Error Handling

*   **Memory Management:** The server automatically launches an hourly background sweeper that deletes temporary WAV files older than 1 hour. In addition, every API route deletes temporary files immediately after serving using FastAPI `BackgroundTasks`.
*   **Device Fallback:** `faster-whisper` automatically searches for a local CUDA setup. If no CUDA driver or GPU is available, it gracefully falls back to CPU execution using optimized `int8` quantization.
*   **PCM Wrapper:** The `AudioService` intercepts raw uploads. If an upload lacks a `RIFF` WAV header, it automatically assumes it is a raw `16-bit 16kHz Mono PCM` stream and wraps it into a correct WAV header before feeding it to the transcription engine.
