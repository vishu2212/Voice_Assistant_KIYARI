# KIYARI ESP-IDF Firmware

This is the official C firmware implementation for the **KIYARI** offline voice assistant device based on an ESP32-S3.

## 📂 Project Structure

```text
kiyari-esp32/
│
├── main/
│   ├── app_main.c          # Application bootloader and state loop
│   ├── app_main.h          # Global function definitions
│   ├── config.h            # System pins and credentials configurations
│   ├── app_state.h         # State machine enum definitions
│   └── CMakeLists.txt
│
├── components/
│   ├── microphone/         # INMP441 I2S record component
│   ├── speaker/            # MAX98357A I2S playback component
│   ├── display/            # SSD1306 OLED I2C display component
│   ├── network/            # Station Wi-Fi manager component
│   ├── api/                # PCM compiler and WAV formatting buffer
│   ├── assistant/          # POST upload and download HTTP manager
│   ├── storage/            # NVS flash initialization component
│   ├── button/             # GPIO trigger push-button component
│   └── led/                # Status indicator LED controller component
│
├── assets/                 # Storage for visual assets and tones
├── managed_components/     # External registered components
├── sdkconfig.defaults      # Default compiler configurations
└── CMakeLists.txt          # Root boilerplate compile targets definitions
```

## ⚙️ Pin Mappings

Default pin configurations defined in `main/config.h`:
*   **I2C OLED:** SDA=GPIO_1, SCL=GPIO_2
*   **I2S Mic/Speaker (Shared Clocks):** BCLK=GPIO_15, WS=GPIO_16
*   **I2S Speaker Data Out:** DOUT=GPIO_7
*   **I2S Microphone Data In:** DIN=GPIO_6
*   **Trigger Button:** BOOT=GPIO_0 (devkit Boot button)
*   **Status LED:** LED=GPIO_48
