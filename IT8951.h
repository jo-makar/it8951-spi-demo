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

    uint16_t spi_transfer(uint16_t v) {
      uint8_t h = spi->transfer(v >> 8);
      uint8_t l = spi->transfer(v & 0xff);
      return (h << 8) | l;
    }

    void command(uint16_t c) {
      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x6000); // Command
      spi_transfer(c);

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100); // FIXME Necessary?
    }

    uint16_t read() {
      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x1000); // Read
      spi_transfer(0); // Dummy word read
      uint16_t rv = spi_transfer(0);

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100); // FIXME Necessary?

      return rv;
    }

    void read_buf(uint16_t *buf, size_t n) {
      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x1000); // Read
      spi_transfer(0); // Dummy word read

      for (size_t i = 0; i < n; i++) {
        buf[i] = spi_transfer(0);
      }

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100); // FIXME Necessary?
    }

    void write(uint16_t v) {
      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x0000); // Write
      spi_transfer(v);

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100);
    }

    void write_buf(uint16_t *buf, size_t n) {
      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x0000); // Write

      for (size_t i = 0; i < n; i++) {
        spi_transfer(buf[i]);
      }

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100); // FIXME Necessary?
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
      uint16_t data[20];

      spi->beginTransaction(spi_settings);
      command(0x0302); // Get Device System Info
      read_buf(data, 20);
      spi->endTransaction();
      
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

      return info;
    }

    uint16_t read_register(uint16_t addr) {
      spi->beginTransaction(spi_settings);
      command(0x0010); // Read Register
      write(addr);
      uint16_t reg = read();
      spi->endTransaction();

      return reg;
    }

    void write_register(uint16_t addr, uint16_t val) {
      spi->beginTransaction(spi_settings);
      command(0x0011); // Write Register
      write(addr);
      write(val);
      spi->endTransaction();
    }

    // Pixel mode: 0, 1, 2 => 2, 3, 4 bits per pixel
    void load_image(const uint8_t image[], DeviceInfo info, int mode) {
      // Set the LISAR register to the image buffer address
      write_register(0x020a, (uint16_t)(info.buffer_address >> 16));
      write_register(0x0208, (uint16_t)(info.buffer_address & 0xffff));

      spi->beginTransaction(spi_settings);
      command(0x0021); // Load Image Area
      write(
        ((endian == Endian::Big ? 1 : 0) << 8)
          | (mode << 4)
          | 0 // Rotation
      );
      write(0); // X coordinate
      write(0); // Y coordinate
      write(info.panel_width);
      write(info.panel_height);

      // Transfer one row of pixels per SPI transfer
      for (uint16_t i = 0; i < info.panel_height; i++) {
        ready();
        digitalWrite(pin_spi_cs, LOW);

        spi_transfer(0x0000); // Write

        for (uint16_t j = 0; j < info.panel_width / 2; j += 2) {
          spi->transfer(image[i * (info.panel_width / 2) + j + 1]);
          spi->transfer(image[i * (info.panel_width / 2) + j]);
        }

        digitalWrite(pin_spi_cs, HIGH);
        delayMicroseconds(100); // FIXME Necessary?
      }

      command(0x0022); // Load Image End

      spi->endTransaction();
    }

    // Display mode:
    // 0 => Initialize
    // 1 => Direct update, monochrome only
    // 2 => Grayscale with clear/refresh
    // 3 => Grayscale w/o full clear/refresh
    void display_area(DeviceInfo info, uint8_t mode) {
      spi->beginTransaction(spi_settings);
      command(0x0034); // Display Area
      write(0); // X coordinate
      write(0); // Y coordinate
      write(info.panel_width);
      write(info.panel_height);
      write(mode);
      spi->endTransaction();

      // Wait for the LUT engine to be ready (via the LUTAFSR register) 
      while (read_register(0x1224) != 0) {
        delay(10);
      }
    }
};

#endif