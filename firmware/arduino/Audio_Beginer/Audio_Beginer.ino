#include <driver/i2s.h>
#include <math.h>

#define I2S_BCLK 5
#define I2S_LRC  6
#define I2S_DOUT 4

const int SAMPLE_RATE = 44100;

void playTone(float freq, int durationMs, int volume = 12000)
{
  int samples = SAMPLE_RATE * durationMs / 1000;

  for (int i = 0; i < samples; i++)
  {
    int16_t sample = volume * sin(2 * PI * freq * i / SAMPLE_RATE);

    size_t bytesWritten;
    i2s_write(
      I2S_NUM_0,
      &sample,
      sizeof(sample),
      &bytesWritten,
      portMAX_DELAY
    );
  }
}

void setup()
{
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false};

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCLK,
      .ws_io_num = I2S_LRC,
      .data_out_num = I2S_DOUT,
      .data_in_num = I2S_PIN_NO_CHANGE};

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  delay(500);

  // Startup Melody
  playTone(523, 150);   // C5
  playTone(659, 150);   // E5
  playTone(784, 250);   // G5
}

void loop()
{
}