/*
 * Custom Chip: Emulador mínimo de Sensirion SHT31-D para Wokwi
 * --------------------------------------------------------------
 * Responde al protocolo I2C usado por la librería Adafruit_SHT31:
 *   1) El maestro escribe 2 bytes de comando (p.ej. 0x2400 = single shot, high repeatability)
 *   2) El chip "mide" (instantáneo en la simulación) y prepara 6 bytes de respuesta:
 *        [tempMSB, tempLSB, tempCRC, humMSB, humLSB, humCRC]
 *   3) El maestro lee esos 6 bytes.
 *
 * Fórmulas del datasheet Sensirion SHT3x:
 *   T[°C] = -45 + 175 * (rawT / 65535)
 *   RH[%] =        100 * (rawH / 65535)
 *
 * CRC-8 Sensirion: polinomio 0x31, valor inicial 0xFF, sin reflejar, sin XOR final.
 *
 * Los valores de temperatura y humedad se controlan con dos atributos
 * editables desde el panel de Wokwi (clic en el chip): "temperature" y "humidity".
 *
 * NOTA: la firma exacta de i2c_config_t / i2c_init puede variar ligeramente entre
 * versiones de la Wokwi Chips API. Si el build falla, revisa
 * https://docs.wokwi.com/chips-api/i2c y ajusta los nombres de campos.
 */

#include "wokwi-api.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
  i2c_dev_t i2c;

  attr_t attr_temp;
  attr_t attr_hum;

  uint8_t tx_buffer[6];
  uint8_t tx_pos;
  uint8_t tx_len;

  uint8_t rx_buffer[2];
  uint8_t rx_pos;
} chip_state_t;

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
static void chip_prepare_measurement(chip_state_t *chip) {
  double t = attr_read_float(chip->attr_temp);
  double h = attr_read_float(chip->attr_hum);

  if (t < -40) t = -40;
  if (t > 125) t = 125;
  if (h < 0)   h = 0;
  if (h > 100) h = 100;

  uint16_t raw_t = (uint16_t)(((t + 45.0) / 175.0) * 65535.0);
  uint16_t raw_h = (uint16_t)((h / 100.0) * 65535.0);

  chip->tx_buffer[0] = (raw_t >> 8) & 0xFF;
  chip->tx_buffer[1] = raw_t & 0xFF;
  chip->tx_buffer[2] = sensirion_crc8(chip->tx_buffer, 2);

  chip->tx_buffer[3] = (raw_h >> 8) & 0xFF;
  chip->tx_buffer[4] = raw_h & 0xFF;
  chip->tx_buffer[5] = sensirion_crc8(chip->tx_buffer + 3, 2);

  chip->tx_pos = 0;
  chip->tx_len = 6;
}

/* ---------- Callbacks I2C ---------- */
static bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (read) {
    /* El maestro quiere leer los 6 bytes de medición ya preparados */
    if (chip->tx_len == 0) {
      chip_prepare_measurement(chip);
    }
  } else {
    /* El maestro va a escribir el comando de 2 bytes */
    chip->rx_pos = 0;
  }
  return true; /* aceptar la conexión */
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (chip->tx_pos < chip->tx_len) {
    return chip->tx_buffer[chip->tx_pos++];
  }
  return 0xFF;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (chip->rx_pos < 2) {
    chip->rx_buffer[chip->rx_pos++] = data;
  }
  if (chip->rx_pos == 2) {
    /* Comando completo recibido (p.ej. 0x2400). No distinguimos el modo de
       repetibilidad: siempre devolvemos la medición actual configurada. */
    chip_prepare_measurement(chip);
  }
  return true; /* ACK */
}

static void on_i2c_disconnect(void *user_data, uint32_t address) {
  (void)user_data;
  (void)address;
  /* nada que limpiar */
}

/* ---------- Inicialización del chip ---------- */
void chip_init(void) {
  chip_state_t *chip = (chip_state_t *)malloc(sizeof(chip_state_t));
  memset(chip, 0, sizeof(chip_state_t));

  chip->attr_temp = attr_init_float("temperature", 2.5f); /* °C, editable en el panel */
  chip->attr_hum  = attr_init_float("humidity", 60.0f);   /* %,  editable en el panel */

  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = 0x44,
    .scl = pin_init("SCL", INPUT),
    .sda = pin_init("SDA", INPUT),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
    .disconnect = on_i2c_disconnect,
  };
  chip->i2c = i2c_init(&i2c_config);

  printf("SHT31 custom chip iniciado en 0x44\n");
}
