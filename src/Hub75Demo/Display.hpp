#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include <Adafruit_GFX.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"

/// 0xRRGGBB packed color -- pass plain hex colors everywhere. There's no
/// RGB565 anywhere in this API; Display converts internally when it
/// needs to talk to Adafruit_GFX's text/shape routines.
using Rgb = uint32_t;

/// Defaults match matya's board. Override only what you need:
///
///   DisplayConfig config;      // all defaults
///   config.rotation = 2;       // e.g. flip 180 degrees
///   display.begin(config);
struct SPIConfig {
    int pin_ck = 3;
    int pin_cs = 1;
    int pin_d0 = 38;
    int pin_d1 = 39;
    int pin_d2 = 41;
    int pin_d3 = 45;
    int baud   = 20 * 1000 * 1000;
};

struct DisplayConfig {
    int width = 64;
    int height = 64;
    // 0 = no rotation, 1 = 90, 2 = 180, 3 = 270 degrees (clockwise).
    // Same convention as Adafruit_GFX's own setRotation().
    int rotation = 1;
    SPIConfig spi = {};
};

/**
 * A HUB75 panel driven over quad SPI, with text/shapes built in (via
 * Adafruit_GFX) -- this is the only object you need, no separate "gfx".
 *
 *   Display display;
 *
 *   void setup() {
 *       display.begin();                          // your board's defaults
 *   }
 *
 *   void loop() {
 *       display.clear();
 *       display.drawText(2, 2, "Ahoj", 0xFFFFFF);
 *       display.drawRect(0, 0, 10, 10, 0x00FF00);
 *       display.setPixel(5, 5, 0xFF0000);
 *       display.show();
 *   }
 */
class Display : public Adafruit_GFX {
public:
    Display() : Adafruit_GFX(0, 0) {}
    ~Display();

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    /// Sets up the framebuffer and SPI hardware. Call once from setup().
    void begin(const DisplayConfig& config = DisplayConfig{});

    // --- Simple, Jaculus-style drawing -- every color is 0xRRGGBB ------

    /// Sets a single pixel in the framebuffer (full RGB888 precision).
    /// Call show() to actually send the frame out.
    void setPixel(int x, int y, Rgb color);

    /// Draws text at (x, y) in one call. `size` matches Adafruit_GFX's
    /// setTextSize() (1 = 6x8px per character, 2 = double size, ...).
    void drawText(int x, int y, const char* text, Rgb color, uint8_t size = 1);

    void drawLine(int x0, int y0, int x1, int y1, Rgb color);
    void drawRect(int x, int y, int w, int h, Rgb color);
    void fillRect(int x, int y, int w, int h, Rgb color);
    void drawCircle(int x, int y, int r, Rgb color);
    void fillCircle(int x, int y, int r, Rgb color);

    /// Clears the framebuffer (fast path, all zeros).
    void clear();

    /// Fills the whole framebuffer with a solid color.
    void fill(Rgb color);

    /// Draws an RGB888 image (row-major, top-down, 3 bytes/pixel) at (x, y),
    /// full color precision, no RGB565 downconversion. Out-of-bounds pixels
    /// are clipped.
    void drawImage(int x, int y, int w, int h, const uint8_t* rgb888);

    /// Sends the framebuffer to the display over SPI.
    void show();

    /// Required by Adafruit_GFX -- called internally by print()/drawRect()/
    /// etc. You normally don't need to call this directly; use setPixel()
    /// instead (same result, but keeps things in plain 0xRRGGBB).
    void drawPixel(int16_t x, int16_t y, uint16_t color565) override;

    /// Direct access to the framebuffer for advanced use (scrolling, custom
    /// blending). Layout is RGB888, pixel index = (x + WIDTH * y) * 3, using
    /// the raw (pre-rotation) panel width as the stride -- this is the
    /// buffer BEFORE rotation (rotation happens inside setPixel()/drawPixel()).
    uint8_t* rawPixels() { return _frame.data(); }
    size_t rawPixelsSize() const { return _frame.size(); }

    float brightness() const { return _brightness; }
    void setBrightness(float value);

private:
    static constexpr size_t kMaxChunk = 4092;

    float _brightness = 0.5f;

    std::vector<uint8_t> _frame;      // WIDTH * HEIGHT * 3 (RGB888)
    uint8_t _sync[32] = {};
    uint8_t _modeset[8] = {};
    std::vector<uint8_t> _txBuffer;

    int _csPin = -1;
    spi_device_handle_t _spi = nullptr;

    void writePixelRaw(int x, int y, Rgb color888);

    static uint16_t rgb888to565(Rgb color);
    static Rgb rgb565to888(uint16_t color);

    void setupSPI(const SPIConfig& spi);
    void buildSyncBuffer();
    void buildModesetBuffer();
    void spiTransfer(const uint8_t* data, size_t len);
};
