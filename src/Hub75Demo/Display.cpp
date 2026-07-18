#include "Display.hpp"

#include <cassert>
#include <cstring>
#include <algorithm>

#include "esp_err.h"

static inline void setPixelByIndex(uint8_t* view, int i, uint8_t r, uint8_t g, uint8_t b) {
    i *= 3;
    view[i]     = r;
    view[i + 1] = g;
    view[i + 2] = b;
}

Display::~Display() {
    if (_spi) {
        spi_bus_remove_device(_spi);
        spi_bus_free(SPI2_HOST);
    }
}

void Display::begin(const DisplayConfig& config) {
    // WIDTH/HEIGHT (raw, unrotated) and _width/_height (rotation-aware) are
    // inherited from Adafruit_GFX. setRotation() takes care of computing
    // _width/_height from WIDTH/HEIGHT for us.
    WIDTH = config.width;
    HEIGHT = config.height;
    setRotation(static_cast<uint8_t>(config.rotation));

    _csPin = config.spi.pin_cs;

    _frame.assign(static_cast<size_t>(WIDTH) * HEIGHT * 3, 0);

    setupSPI(config.spi);
    buildSyncBuffer();
    buildModesetBuffer();
}

void Display::writePixelRaw(int x, int y, Rgb color888) {
    // Bounds are checked in rotation-aware space (_width/_height), same as
    // every other Adafruit_GFX-based display driver.
    if (x < 0 || x >= _width || y < 0 || y >= _height) return;

    int rx = x, ry = y;
    switch (getRotation()) {
        case 1: { int t = rx; rx = WIDTH - 1 - ry; ry = t; break; }
        case 2: rx = WIDTH - 1 - rx; ry = HEIGHT - 1 - ry; break;
        case 3: { int t = rx; rx = ry; ry = HEIGHT - 1 - t; break; }
        default: break; // 0: no transform
    }

    int i = rx + WIDTH * ry; // stride is always the RAW (unrotated) width

    uint8_t r = (color888 & 0xff0000) >> 16;
    uint8_t g = (color888 & 0x00ff00) >> 8;
    uint8_t b = color888 & 0x0000ff;
    setPixelByIndex(_frame.data(), i, r, g, b);
}

void Display::setPixel(int x, int y, Rgb color) {
    writePixelRaw(x, y, color);
}

void Display::drawPixel(int16_t x, int16_t y, uint16_t color565) {
    writePixelRaw(x, y, rgb565to888(color565));
}

void Display::drawText(int x, int y, const char* text, Rgb color, uint8_t size) {
    setTextColor(rgb888to565(color));
    setTextSize(size);
    setCursor(x, y);
    print(text);
}

void Display::drawLine(int x0, int y0, int x1, int y1, Rgb color) {
    Adafruit_GFX::drawLine(x0, y0, x1, y1, rgb888to565(color));
}

void Display::drawRect(int x, int y, int w, int h, Rgb color) {
    Adafruit_GFX::drawRect(x, y, w, h, rgb888to565(color));
}

void Display::fillRect(int x, int y, int w, int h, Rgb color) {
    Adafruit_GFX::fillRect(x, y, w, h, rgb888to565(color));
}

void Display::drawCircle(int x, int y, int r, Rgb color) {
    Adafruit_GFX::drawCircle(x, y, r, rgb888to565(color));
}

void Display::fillCircle(int x, int y, int r, Rgb color) {
    Adafruit_GFX::fillCircle(x, y, r, rgb888to565(color));
}

void Display::clear() {
    std::fill(_frame.begin(), _frame.end(), 0);
}

void Display::fill(Rgb color) {
    uint8_t r = (color & 0xff0000) >> 16;
    uint8_t g = (color & 0x00ff00) >> 8;
    uint8_t b = color & 0x0000ff;
    for (size_t i = 0; i < _frame.size() / 3; i++)
        setPixelByIndex(_frame.data(), i, r, g, b);
}

void Display::drawImage(int x0, int y0, int w, int h, const uint8_t* rgb888) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            const uint8_t* px = rgb888 + (static_cast<size_t>(row) * w + col) * 3;
            Rgb color = (static_cast<Rgb>(px[0]) << 16) | (static_cast<Rgb>(px[1]) << 8) | px[2];
            writePixelRaw(x0 + col, y0 + row, color);
        }
    }
}

void Display::show() {
    _txBuffer.resize(sizeof(_sync) + sizeof(_modeset) + _frame.size());
    uint8_t* p = _txBuffer.data();
    std::memcpy(p, _sync, sizeof(_sync));
    p += sizeof(_sync);
    std::memcpy(p, _modeset, sizeof(_modeset));
    p += sizeof(_modeset);
    std::memcpy(p, _frame.data(), _frame.size());

    spiTransfer(_txBuffer.data(), _txBuffer.size());
}

void Display::setBrightness(float value) {
    assert(value >= 0.0f && value <= 1.0f);
    _brightness = value;
    buildModesetBuffer();
}

uint16_t Display::rgb888to565(Rgb color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    return (static_cast<uint16_t>(r & 0xF8) << 8)
         | (static_cast<uint16_t>(g & 0xFC) << 3)
         | (b >> 3);
}

Rgb Display::rgb565to888(uint16_t color) {
    uint8_t r5 = (color >> 11) & 0x1F;
    uint8_t g6 = (color >> 5) & 0x3F;
    uint8_t b5 = color & 0x1F;
    uint8_t r8 = (r5 * 255 + 15) / 31;
    uint8_t g8 = (g6 * 255 + 31) / 63;
    uint8_t b8 = (b5 * 255 + 15) / 31;
    return (static_cast<Rgb>(r8) << 16) | (static_cast<Rgb>(g8) << 8) | b8;
}

void Display::setupSPI(const SPIConfig& spi) {
    gpio_config_t csCfg = {};
    csCfg.pin_bit_mask = 1ULL << spi.pin_cs;
    csCfg.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&csCfg));
    gpio_set_level(static_cast<gpio_num_t>(_csPin), 1); // idle high

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num   = spi.pin_d0;
    buscfg.miso_io_num   = spi.pin_d1;
    buscfg.sclk_io_num   = spi.pin_ck;
    buscfg.quadwp_io_num = spi.pin_d2;
    buscfg.quadhd_io_num = spi.pin_d3;
    buscfg.max_transfer_sz = kMaxChunk;

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = spi.baud;
    devcfg.mode = 0;
    devcfg.spics_io_num = -1; // driven manually, see spiTransfer()
    devcfg.queue_size = 4;
    devcfg.flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_BIT_LSBFIRST;

    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &_spi));
}

void Display::buildSyncBuffer() {
    static constexpr uint16_t words[16] = {
        0xac92, 0x3bca, 0x41bf, 0x393d, 0xa74a, 0xae01, 0x155d, 0xfb70,
        0xf681, 0x2f6d, 0x4931, 0x0fa3, 0x77bf, 0xd756, 0x26f9, 0x4eb6,
    };
    for (int i = 0; i < 16; i++) {
        _sync[2 * i]     = words[i] & 0xff;
        _sync[2 * i + 1] = (words[i] >> 8) & 0xff;
    }
}

void Display::buildModesetBuffer() {
    _modeset[0] = 0xfb;
    _modeset[1] = 0x00;
    _modeset[2] = 0x09;
    _modeset[3] = static_cast<uint8_t>(255.0f * _brightness);

    uint16_t w = static_cast<uint16_t>(WIDTH);
    _modeset[4] = w & 0xff;
    _modeset[5] = (w >> 8) & 0xff;

    _modeset[6] = 0;
    _modeset[7] = 0;
}

void Display::spiTransfer(const uint8_t* data, size_t len) {
    gpio_set_level(static_cast<gpio_num_t>(_csPin), 0);

    size_t offset = 0;
    while (offset < len) {
        size_t chunk = std::min(len - offset, kMaxChunk);

        spi_transaction_t t = {};
        t.flags = SPI_TRANS_MODE_QIO;
        t.length = chunk * 8;
        t.tx_buffer = data + offset;

        ESP_ERROR_CHECK(spi_device_transmit(_spi, &t));
        offset += chunk;
    }

    gpio_set_level(static_cast<gpio_num_t>(_csPin), 1);
}
