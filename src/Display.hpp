#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "driver/spi_master.h"
#include "driver/gpio.h"

/// 0xRRGGBB packed color, matching the `Rgb` type used on the TS side.
using Rgb = uint32_t;

struct SPIConfig {
    gpio_num_t pin_ck;
    gpio_num_t pin_cs;
    gpio_num_t pin_d0;
    gpio_num_t pin_d1;
    gpio_num_t pin_d2;
    gpio_num_t pin_d3;
    int baud;
};

struct DisplayConfig {
    int width;
    int height;
    int rotation;   // -1, 0, or 1 (same convention as the TS class)
    SPIConfig spi;
};

/**
 * Holds a framebuffer and talks to a HUB75 bridge over quad SPI.
 * Direct C++ port of the Jaculus `Display` class for ESP-IDF / ESP32-S3.
 */
class Display {
public:
    explicit Display(const DisplayConfig& config);
    ~Display();

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    /// Modifies a single pixel in the framebuffer only. Call show() to send it out.
    void setPixel(float x, float y, Rgb color);

    /// Clears the framebuffer (fast path, all zeros).
    void clear();

    /// Fills the whole framebuffer with a solid color.
    void fill(Rgb color);

    /// Sends the framebuffer to the display over SPI.
    void show();

    int rotation() const { return _rotation; }
    int width() const { return _width; }
    int height() const { return _height; }

    const std::vector<uint8_t>& frame() const { return _frame; }

    float brightness() const { return _brightness; }
    void setBrightness(float value);

private:
    // Safe chunk size for a single DMA-backed SPI transaction. Newer
    // ESP-IDF + ESP32-S3 (GDMA) can often do more in one shot, but 4092 B
    // is a portable, always-safe limit; we just loop for bigger frames.
    static constexpr size_t kMaxChunk = 4092;

    int _width;
    int _height;
    int _rotation;
    float _brightness;

    std::vector<uint8_t> _frame;     // width * height * 3 (RGB888)
    uint8_t _sync[32];                // 16 x little-endian uint16
    uint8_t _modeset[8];
    std::vector<uint8_t> _txBuffer;   // scratch buffer used by show()

    gpio_num_t _csPin;
    spi_device_handle_t _spi = nullptr;

    void setupSPI(const SPIConfig& spi);
    void buildSyncBuffer();
    void buildModesetBuffer();
    void spiTransfer(const uint8_t* data, size_t len);
};
