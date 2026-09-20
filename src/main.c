/*
 * Custom Chip: Emulador mínimo de Sensirion SHT31-D para Wokwi
 * --------------------------------------------------------------
 * Responde al protocolo I2C usado por la librería Adafruit_SHT31:
 *   1) El maestro escribe 2 bytes de comando (p.ej. 0x2400)
 *   2) El chip prepara 6 bytes de respuesta:
 *        [tempMSB, tempLSB, tempCRC, humMSB, humLSB, humCRC]
 *   3) El maestro lee esos 6 bytes.
 */

#include "wokwi-api.h"

typedef struct {
  i2c_dev_t i2c;

  uint32_t attr_temp;
  uint32_t attr_hum;

  uint8_t tx_buffer[6];
  uint8_t tx_pos;
  uint8_t tx_len;

  uint8_t rx_buffer[2];
  uint8_t rx_pos;
} chip_state_t;

static chip_state_t chip;

/* ---------- CRC-8 Sensirion (poly 0x31, init 0xFF) ---------- */
static uint8_t sensirion_crc8(const uint8_t *data, int len) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

/* ---------- Arma el paquete de 6 bytes con T y RH actuales ---------- */
static void chip_prepare_measurement(chip_state_t *c) {
  float t = attr_read_float(c->attr_temp);
  float h = attr_read_float(c->attr_hum);

  if (t < -40.0f) t = -40.0f;
  if (t > 125.0f) t = 125.0f;
  if (h < 0.0f)   h = 0.0f;
  if (h > 100.0f) h = 100.0f;

  uint16_t raw_t = (uint16_t)(((t + 45.0f) / 175.0f) * 65535.0f);
  uint16_t raw_h = (uint16_t)((h / 100.0f) * 65535.0f);

  c->tx_buffer[0] = (raw_t >> 8) & 0xFF;
  c->tx_buffer[1] = raw_t & 0xFF;
  c->tx_buffer[2] = sensirion_crc8(c->tx_buffer, 2);

  c->tx_buffer[3] = (raw_h >> 8) & 0xFF;
  c->tx_buffer[4] = raw_h & 0xFF;
  c->tx_buffer[5] = sensirion_crc8(c->tx_buffer + 3, 2);

  c->tx_pos = 0;
  c->tx_len = 6;
}

/* ---------- Callbacks I2C ---------- */
static bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  chip_state_t *c = (chip_state_t *)user_data;
  if (read) {
    if (c->tx_len == 0) {
      chip_prepare_measurement(c);
    }
  } else {
    c->rx_pos = 0;
  }
  return true;
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *c = (chip_state_t *)user_data;
  if (c->tx_pos < c->tx_len) {
    return c->tx_buffer[c->tx_pos++];
  }
  return 0xFF;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *c = (chip_state_t *)user_data;
  if (c->rx_pos < 2) {
    c->rx_buffer[c->rx_pos++] = data;
  }
  if (c->rx_pos == 2) {
    chip_prepare_measurement(c);
  }
  return true;
}

static void on_i2c_disconnect(void *user_data) {
  (void)user_data;
}

/* ---------- Inicialización del chip ---------- */
void chip_init(void) {
  chip.attr_temp = attr_init_float("temperature", 2.5f);
  chip.attr_hum  = attr_init_float("humidity", 60.0f);
  chip.tx_pos = 0;
  chip.tx_len = 0;
  chip.rx_pos = 0;

  const i2c_config_t i2c_config = {
    .user_data = &chip,
    .address = 0x44,
    .scl = pin_init("SCL", INPUT),
    .sda = pin_init("SDA", INPUT),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
    .disconnect = on_i2c_disconnect,
  };
  chip.i2c = i2c_init(&i2c_config);
}
