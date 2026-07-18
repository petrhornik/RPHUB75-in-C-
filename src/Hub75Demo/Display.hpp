#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "driver/spi_master.h"
#include "driver/gpio.h"

/// 0xRRGGBB packed color, matching the `Rgb` type used on the TS side.
using Rgb = uint32_t;

struct SPIConfig {
    int pin_ck;
    int pin_cs;
    int pin_d0;
    int pin_d1;
    int pin_d2;
    int pin_d3;
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
 * Direct C++ port of the Jaculus `Display` class for ESP-IDF / ESP32-S3,
 * extended with raw pixel access and simple image drawing so it can back
 * a text/graphics layer (see HubGfx.hpp).
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

    /// Draws an RGB888 image (row-major, top-down, 3 bytes/pixel) at (x, y).
    /// Out-of-bounds pixels are clipped. Goes through setPixel(), so it
    /// respects the current `rotation`.
    void drawImage(int x, int y, int w, int h, const uint8_t* rgb888);

    /// Sends the framebuffer to the display over SPI.
    void show();

    int rotation() const { return _rotation; }
    int width() const { return _width; }
    int height() const { return _height; }

    /// Direct access to the framebuffer for advanced use (scrolling, custom
    /// blending, etc). Layout is RGB888, pixel index = (x + width * y) * 3.
    /// This is the buffer BEFORE any rotation (rotation is only applied
    /// inside setPixel()/drawImage()).
    uint8_t* rawPixels() { return _frame.data(); }
    size_t rawPixelsSize() const { return _frame.size(); }

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

    int _csPin;
    spi_device_handle_t _spi = nullptr;

    void setupSPI(const SPIConfig& spi);
    void buildSyncBuffer();
    void buildModesetBuffer();
    void spiTransfer(const uint8_t* data, size_t len);
};
