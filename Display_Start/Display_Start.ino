#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// 1.3" SH1106 OLED (128x64)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void setup() {
  Serial.begin(115200);

  // SDA = GPIO21, SCL = GPIO22
  Wire.begin(21, 22);
  Wire.setClock(100000);

  u8g2.begin();

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_logisoso18_tf);

  int16_t x = (128 - u8g2.getStrWidth("KIYARI")) / 2;
  int16_t y = 40;

  u8g2.drawStr(x, y, "KIYARI");

  u8g2.sendBuffer();

  Serial.println("OLED OK");
}

void loop() {
}