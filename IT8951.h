#ifndef IT8951_H
#define IT8951_H

#include <SPI.h>

class IT8951 {
  private:
    SPIClass * const spi;
    const SPISettings &spi_settings;
    const int pin_spi_cs;
    const int pin_hrdy;
    const int pin_rst;

    enum class Endian { Little, Big };
    const Endian endian;

    static Endian determine_endian() {
      uint16_t val = 0x0001;
      uint8_t *ptr = (uint8_t *)&val;
      if (*ptr == 1) {
        return Endian::Little;
      } else {
        return Endian::Big;
      }
    }

    uint16_t be_to_host(uint16_t x) {
      if (endian == Endian::Little) {
        return (x << 8) | (x >> 8);
      } else {
        return x;
      }
    }

  public:
    IT8951(SPIClass *spi, const SPISettings &spi_settings, int pin_spi_cs, int pin_hrdy, int pin_rst)
      : spi(spi), spi_settings(spi_settings), pin_spi_cs(pin_spi_cs), pin_hrdy(pin_hrdy), pin_rst(pin_rst), endian(determine_endian()) {
    }

    void ready() {
      while (digitalRead(pin_hrdy) == LOW) {
        delayMicroseconds(50);
      }
    }

    void reset() {
      digitalWrite(pin_rst, LOW);
      delayMicroseconds(100);
      digitalWrite(pin_rst, HIGH);
      delay(150);

      ready();
    }

    struct DeviceInfo {
      uint16_t panel_width;
      uint16_t panel_height;
      uint32_t buffer_address;
      char it8951_version[16];
      char lut_version[16];
    };

    DeviceInfo get_device_system_info() {
      DeviceInfo info;

      ready();
      spi->beginTransaction(spi_settings);
      digitalWrite(pin_spi_cs, LOW);

      uint16_t preamble = 0x6000; // Command
      spi->transfer(preamble >> 8);
      spi->transfer(preamble & 0xff);

      uint16_t command = 0x0302; // Get Device System Info
      spi->transfer(command >> 8);
      spi->transfer(command & 0xff);

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100);

      ready();
      digitalWrite(pin_spi_cs, LOW);

      preamble = 0x1000; // Read
      spi->transfer(preamble >> 8);
      spi->transfer(preamble & 0xff);

      // Dummy word read
      spi->transfer(0);
      spi->transfer(0);

      // Device System Info read
      uint16_t data[20];
      for (int i = 0; i < 20; i++) {
        uint8_t h = spi->transfer(0);
        uint8_t l = spi->transfer(0);
        data[i] = (h << 8) | l;
      }

      info.panel_width = data[0];
      info.panel_height = data[1];
      info.buffer_address = (data[3] << 16) | data[2];

      for (int i = 4, j = 0; i < 12; i++, j += 2) {
        info.it8951_version[j] = data[i] >> 8;
        info.it8951_version[j+1] = data[i];
      }
      info.it8951_version[15] = 0;

      for (int i = 12, j = 0; i < 20; i++, j += 2) {
        info.lut_version[j] = data[i] >> 8;
        info.lut_version[j+1] = data[i];
      }
      info.lut_version[15] = 0;

      digitalWrite(pin_spi_cs, HIGH);
      spi->endTransaction();

      return info;
    }

    uint16_t read_register(uint16_t address) {
      ready();
      spi->beginTransaction(spi_settings);
      digitalWrite(pin_spi_cs, LOW);

      uint16_t preamble = 0x6000; // Command
      spi->transfer(preamble >> 8);
      spi->transfer(preamble & 0xff);

      uint16_t command = 0x0010; // Read Register
      spi->transfer(command >> 8);
      spi->transfer(command & 0xff);

      spi->transfer(address >> 8);
      spi->transfer(address & 0xff);

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100);

      ready();
      digitalWrite(pin_spi_cs, LOW);

      preamble = 0x1000; // Read
      spi->transfer(preamble >> 8);
      spi->transfer(preamble & 0xff);

      // Dummy word read
      spi->transfer(0);
      spi->transfer(0);

      // Register read
      uint8_t h = spi->transfer(0);
      uint8_t l = spi->transfer(0);

      digitalWrite(pin_spi_cs, HIGH);
      spi->endTransaction();

      return (h << 8) | l;
    }

    void write_register(uint16_t address, uint16_t value) {
      ready();
      spi->beginTransaction(spi_settings);
      digitalWrite(pin_spi_cs, LOW);

      uint16_t preamble = 0x6000; // Command
      spi->transfer(preamble >> 8);
      spi->transfer(preamble & 0xff);

      uint16_t command = 0x0011; // Write Register
      spi->transfer(command >> 8);
      spi->transfer(command & 0xff);

      spi->transfer(address >> 8);
      spi->transfer(address & 0xff);

      spi->transfer(value >> 8);
      spi->transfer(value & 0xff);

      digitalWrite(pin_spi_cs, HIGH);
      spi->endTransaction();

      delayMicroseconds(50);
    }

    void load_image_4bpp(const uint8_t *image_data, size_t image_data_len, uint32_t buffer_addr) {
      // FIXME STOPPED
    }
};

#endif