# chip-sht31 — Emulador SHT31-D para Wokwi

Custom chip que emula el sensor Sensirion **SHT31-D** (I2C, dirección `0x44`)
para poder simular en Wokwi el mismo firmware (`Adafruit_SHT31`) que usarás
en el hardware real (Heltec ESP32-S3 + SHT31-D).

## ⚠️ Antes de usarlo

Este código está escrito siguiendo el patrón oficial de la **Wokwi Chips API**
(ver `docs.wokwi.com/chips-api`), basado en ejemplos reales como
[wokwi/inverter-chip](https://github.com/wokwi/inverter-chip) y
[SergioGasquez/wokwi-shtc3](https://github.com/SergioGasquez/wokwi-shtc3)
(sensor hermano del SHT31, mismo tipo de protocolo I2C + CRC Sensirion).

La API evoluciona, así que **antes de compilar revisa**:
- Los nombres exactos de campos de `i2c_config_t` en `docs.wokwi.com/chips-api/i2c`
- Si tu build falla por firmas de función distintas, ajusta `src/main.c` según
  el ejemplo oficial más reciente (`wokwi/inverter-chip` es el más simple para comparar).

## Cómo compilarlo

### Opción A — GitHub Actions (recomendado)
1. Crea un repo en GitHub y sube esta carpeta completa.
2. Crea un tag de versión:
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```
3. La Action en `.github/workflows/build.yml` compilará `chip.wasm` y subirá
   un `chip.zip` a la Release del tag.

### Opción B — Compilar localmente
Necesitas `clang` con soporte para `wasm32-unknown-unknown`:
```bash
clang --target=wasm32-unknown-unknown \
  -nostdlib -Wl,--no-entry -Wl,--export-all \
  -O2 -o chip.wasm src/main.c
```

## Cómo usarlo en tu proyecto Wokwi

En tu `diagram.json`:

```json
{
  "parts": [
    { "type": "chip-sht31", "id": "sht31", "top": -150, "left": -190, "attrs": {} }
  ],
  "dependencies": {
    "chip-sht31": "github:TU_USUARIO/wokwi-chip-sht31@1.0.0"
  }
}
```

Conecta `VCC`→3V3, `GND`→GND, `SDA`→GPIO41, `SCL`→GPIO42 (igual que harías
con cualquier otro sensor I2C).

## Ajustar temperatura/humedad durante la simulación

El chip expone dos atributos editables (`temperature`, `humidity`) que puedes
modificar desde el panel de Wokwi haciendo clic sobre el componente — así
puedes bajar la temperatura simulada a 1.5 °C, por ejemplo, y verificar que
tu lógica de histéresis (`UMBRAL_HELADA_ON = 2.0`, `UMBRAL_RECUPERA_OFF = 2.5`)
dispare y libere el relé correctamente.
