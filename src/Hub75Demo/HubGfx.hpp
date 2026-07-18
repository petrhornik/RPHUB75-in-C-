#pragma once

#include <Adafruit_GFX.h>
#include "Display.hpp"

/// Thin adapter that gives the raw Display class everything Adafruit_GFX
/// provides for free: text/fonts (print, setCursor, setTextSize...),
/// shapes (drawLine, drawRect, fillCircle...), and 1bpp/565 bitmaps
/// (drawBitmap, drawRGBBitmap). Only drawPixel() needs implementing.
class HubGfx : public Adafruit_GFX {
public:
    explicit HubGfx(Display& display)
        : Adafruit_GFX(display.width(), display.height())
        , _display(display)
    {}

    void drawPixel(int16_t x, int16_t y, uint16_t color565) override {
        if (x < 0 || y < 0 || x >= width() || y >= height()) return;

        // Adafruit_GFX colors are RGB565; our Display framebuffer is
        // RGB888, so expand each channel back out.
        uint8_t r5 = (color565 >> 11) & 0x1F;
        uint8_t g6 = (color565 >> 5) & 0x3F;
        uint8_t b5 = color565 & 0x1F;

        uint8_t r8 = (r5 * 255 + 15) / 31;
        uint8_t g8 = (g6 * 255 + 31) / 63;
        uint8_t b8 = (b5 * 255 + 15) / 31;

        Rgb rgb888 = (static_cast<Rgb>(r8) << 16) | (static_cast<Rgb>(g8) << 8) | b8;
        _display.setPixel(static_cast<float>(x), static_cast<float>(y), rgb888);
    }

    /// Packs 8-bit RGB into RGB565, for use with setTextColor()/fillScreen()
    /// and friends. Adafruit_GFX itself doesn't provide this (only some
    /// display-driver subclasses do), so we bring our own.
    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
        return (static_cast<uint16_t>(r & 0xF8) << 8)
             | (static_cast<uint16_t>(g & 0xFC) << 3)
             | (b >> 3);
    }

private:
    Display& _display;
};
