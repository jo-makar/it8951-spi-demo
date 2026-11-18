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

      spi->beginTransaction(spi_settings);

      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x6000); // Command
      spi_transfer(0x0302); // Get Device System Info

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100);

      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x1000); // Read
      spi_transfer(0); // Dummy word read

      // Device System Info read
      uint16_t data[20];
      for (int i = 0; i < 20; i++) {
        data[i] = spi_transfer(0);
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

    uint16_t read_register(uint16_t addr) {
      spi->beginTransaction(spi_settings);

      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x6000); // Command
      spi_transfer(0x0010); // Read Register

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100);

      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x0000); // Write
      spi_transfer(addr);

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100);

      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x1000); // Read
      spi_transfer(0); // Dummy word read
      uint16_t reg = spi_transfer(0); // Register read

      digitalWrite(pin_spi_cs, HIGH);

      spi->endTransaction();

      return reg;
    }

    void write_register(uint16_t addr, uint16_t val) {
      spi->beginTransaction(spi_settings);

      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x6000); // Command
      spi_transfer(0x0011); // Write Register

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100);

      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x0000); // Write
      spi_transfer(addr);

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100);

      ready();
      digitalWrite(pin_spi_cs, LOW);

      spi_transfer(0x0000); // Write
      spi_transfer(val);

      digitalWrite(pin_spi_cs, HIGH);

      spi->endTransaction();
    }

/* FIXME STOPPED
void load_image_4bpp(const uint8_t *image_data, size_t image_data_len, uint16_t width, uint16_t height) {
      while (read_register(0x1224) != 0) {
        delay(5);
      }

      ready();
      spi->beginTransaction(spi_settings);
      digitalWrite(pin_spi_cs, LOW);

      // 1. Send Command Preamble (0x6000) + Load Image Area (0x0021)
      spi->transfer(0x60);
      spi->transfer(0x00);
      spi->transfer(0x00);
      spi->transfer(0x21);

      // 2. Send parameters (Pixel format 4bpp = 2)
      auto send_param = [this](uint16_t val) {
        spi->transfer(0x00); // Data preamble
        spi->transfer(0x00);
        spi->transfer(val >> 8);
        spi->transfer(val & 0xff);
      };

      send_param((2 << 4)); // 4 bits per pixel
      send_param(0);        // X
      send_param(0);        // Y
      send_param(width);    // Width
      send_param(height);   // Height

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100);

      // 3. Send Data Write Preamble (0x0000)
      ready();
      spi->beginTransaction(spi_settings);
      digitalWrite(pin_spi_cs, LOW);

      spi->transfer(0x00);
      spi->transfer(0x00);

      // 4. Transfer image data as 16-bit words (swapping high/low bytes if endianness requires)
      for (size_t i = 0; i < image_data_len; i += 2) {
        // Pack two bytes into a 16-bit word for the IT8951 data FIFO
        uint8_t b1 = image_data[i];
        uint8_t b2 = image_data[i + 1];
        spi->transfer(b1);
        spi->transfer(b2);
      }

      digitalWrite(pin_spi_cs, HIGH);
      delayMicroseconds(100);

      // 5. Send Image End Command (0x0022)
      ready();
      spi->beginTransaction(spi_settings);
      digitalWrite(pin_spi_cs, LOW);

      spi->transfer(0x60);
      spi->transfer(0x00);
      spi->transfer(0x00);
      spi->transfer(0x22);

      digitalWrite(pin_spi_cs, HIGH);
      spi->endTransaction();
    }

void update_display(uint16_t width, uint16_t height, uint16_t mode) {
      // 1. Wait until LUTAFSR register (0x1224) is 0 (display engine free)
      while (read_register(0x1224) != 0) {
        delay(5);
      }

      // Helper to send a data argument word preceded by the 0x0000 data preamble
      auto send_arg = [this](uint16_t val) {
        ready();
        spi->beginTransaction(spi_settings);
        digitalWrite(pin_spi_cs, LOW);
        
        // Data write preamble (0x0000)
        spi->transfer(0x00);
        spi->transfer(0x00);
        
        // Argument value
        spi->transfer(val >> 8);
        spi->transfer(val & 0xff);
        
        digitalWrite(pin_spi_cs, HIGH);
        spi->endTransaction();
        delayMicroseconds(50);
      };

      // 2. Send Command Preamble (0x6000) + DPY_AREA Command (0x0034)
      ready();
      spi->beginTransaction(spi_settings);
      digitalWrite(pin_spi_cs, LOW);
      
      spi->transfer(0x60);
      spi->transfer(0x00);
      spi->transfer(0x00);
      spi->transfer(0x34); // DPY_AREA
      
      digitalWrite(pin_spi_cs, HIGH);
      spi->endTransaction();
      delayMicroseconds(50);

      // 3. Send all 5 required arguments one by one with their data preambles
      send_arg(0);      // X coordinate
      send_arg(0);      // Y coordinate
      send_arg(width);  // Width (1200)
      send_arg(height); // Height (825)
      send_arg(mode);   // Update Mode (e.g. 2 for GC16)

      // 4. Wait for the display refresh engine to finish processing the frame
      delay(50);
      while (read_register(0x1224) != 0) {
        delay(10);
      }
    }
*/
};

#endif