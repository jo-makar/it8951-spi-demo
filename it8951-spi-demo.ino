// Waveshare RP2350B Plus W development board connected to a E-Paper Display IT8951 controller via SPI.
//
// When using the IT8951 HAT (Hardware Attached on Top), it must be powered via VBUS (5V from the USB port)
// from the Waveshare board otherwise the HAT will not have sufficient current to update the display.
//
// The IT8951 supports a faster I80 interface but the need is diminished with a high SPI clock as used here.

#include "image.h"
#include "IT8951.h"

#include <SPI.h>

const int PIN_SPI_MISO = 16;
const int PIN_SPI_CS = 17; // Active low
const int PIN_SPI_SCK = 18;
const int PIN_SPI_MOSI = 19;

const int PIN_RST = 20; // Output: Active low
const int PIN_HRDY = 21; // Input: Active low

SPISettings spi_settings(24000000, MSBFIRST, SPI_MODE0);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { delay(10); }

  Serial.println(F("Waveshare RP2350-Plus-W"));

  Serial.print(F("Processor architecture: "));
  #if defined(__riscv)
  Serial.println(F("Hazard3 RISC-V"));
  #else
  Serial.println(F("ARM Cortex-M33"));
  #endif

  Serial.print(F("CPU frequency: "));
  Serial.print(rp2040.f_cpu() / 1000000);
  Serial.println(F(" MHz"));

  pico_unique_board_id_t board_id;
  pico_get_unique_board_id(&board_id);
  Serial.print(F("Unique chip / Flash ID: "));
  for (int i = 0; i < 8; i++) {
    if (board_id.id[i] < 0x10) { Serial.print('0'); }
    Serial.print(board_id.id[i], HEX);
    if (i < 7) { Serial.print(':'); }
  }
  Serial.println();

  Serial.print(F("Flash size: "));
  Serial.print(PICO_FLASH_SIZE_BYTES / 1048576);
  Serial.println(F(" MB"));

  // TODO Ideally could be calculated (if not provided by a macro)
  Serial.println(F("SRAM size: 520 KB"));

  // Returns zero if PSRAM is not installed
  // Not automatically used, must be explicitly assigned via PSRAM attribute or via pmalloc()
  Serial.print(F("PSRAM size: "));
  Serial.print(rp2040.getPSRAMSize() / 1048576);
  Serial.println(F(" MB"));

  Serial.print(F("Endianness: "));
  uint16_t val = 0x0001;
  uint8_t *ptr = (uint8_t *)&val;
  if (*ptr == 1) {
    Serial.println(F("Little-Endian"));
  } else {
    Serial.println(F("Big-Endian"));
  }

  pinMode(PIN_HRDY, INPUT_PULLUP);
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH);

  // The chip select line is manually controlled (rather than handled by beginTransaction)
  // because the IT8951 expects the chip select line to be held low across transfers.
  pinMode(PIN_SPI_CS, OUTPUT);
  digitalWrite(PIN_SPI_CS, HIGH);

  SPI.setRX(PIN_SPI_MISO);
  SPI.setSCK(PIN_SPI_SCK);
  SPI.setTX(PIN_SPI_MOSI);
  SPI.begin();

  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  IT8951 it8951(&SPI, spi_settings, PIN_SPI_CS, PIN_HRDY, PIN_RST);

  it8951.reset();
  
  IT8951::DeviceInfo info = it8951.get_device_system_info();

  Serial.print(F("Panel resolution: "));
  Serial.print(info.panel_width);
  Serial.print(F("x"));
  Serial.print(info.panel_height);
  Serial.println();

  Serial.print(F("Image buffer address: 0x"));
  Serial.print(info.buffer_address, HEX);
  Serial.println();

  Serial.print(F("IT8951 version: "));
  Serial.print(info.it8951_version);
  Serial.println();

  // Enable packed mode via the I80CPCR register
  it8951.write_register(0x0004, 1);

  Serial.println(F("Starting image load"));
  uint32_t start = millis();
  it8951.load_image(image_data, info, 2);
  uint32_t delta = millis() - start;
  Serial.print(F("Image load completed: "));
  Serial.print(delta);
  Serial.println(F(" ms"));

  Serial.println(F("Starting display update"));
  start = millis();
  it8951.display_area(info, 2);
  delta = millis() - start;
  Serial.print(F("Display update completed: "));
  Serial.print(delta);
  Serial.println(F(" ms"));

  while (1) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }
}
