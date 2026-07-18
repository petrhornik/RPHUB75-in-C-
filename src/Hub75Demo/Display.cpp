#include "Display.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <algorithm>

#include "esp_err.h"

static inline void setPixelByIndex(uint8_t* view, int i, uint8_t r, uint8_t g, uint8_t b) {
    i *= 3;
    view[i]     = r;
    view[i + 1] = g;
    view[i + 2] = b;
}

Display::Display(const DisplayConfig& config)
    : _width(config.width)
    , _height(config.height)
    , _rotation(config.rotation)
    , _brightness(0.5f)
    , _csPin(config.spi.pin_cs)
{
    _frame.assign(static_cast<size_t>(_width) * _height * 3, 0);

    setupSPI(config.spi);
    buildSyncBuffer();
    buildModesetBuffer();
}

Display::~Display() {
    if (_spi) {
        spi_bus_remove_device(_spi);
        spi_bus_free(SPI2_HOST);
    }
}

void Display::setPixel(float xf, float yf, Rgb color) {
    int x = static_cast<int>(std::lround(xf));
    int y = static_cast<int>(std::lround(yf));

    if (x < 0 || y < 0 || x >= _width || y >= _height) return; // added: bounds check

    if (_rotation == 1) {
        int nx = y;
        int ny = _height - 1 - x;
        x = nx;
        y = ny;
    } else if (_rotation == -1) {
        int nx = _width - 1 - y;
        int ny = x;
        x = nx;
        y = ny;
    }

    // Fixed: this used to use `_height` as the row stride (a straight
    // port of the original TS), which only happens to work on square
    // panels. For width != height it scrambles/overlaps rows, which is
    // exactly the "torn" image you were seeing.
    int i = x + _width * y;

    uint8_t r = (color & 0xff0000) >> 16;
    uint8_t g = (color & 0x00ff00) >> 8;
    uint8_t b = color & 0x0000ff;

    setPixelByIndex(_frame.data(), i, r, g, b);
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
        int y = y0 + row;
        if (y < 0 || y >= _height) continue;

        for (int col = 0; col < w; col++) {
            int x = x0 + col;
            if (x < 0 || x >= _width) continue;

            const uint8_t* px = rgb888 + (static_cast<size_t>(row) * w + col) * 3;
            Rgb color = (static_cast<Rgb>(px[0]) << 16) | (static_cast<Rgb>(px[1]) << 8) | px[2];
            setPixel(static_cast<float>(x), static_cast<float>(y), color);
        }
    }
}

void Display::show() {
    // Sync + modeset + frame concatenated into one continuous CS-low burst,
    // see the notes in the header. CS is driven manually in spiTransfer().
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
    assert(value >= 0.0f && value <= 1.0f); // TS threw TypeError here
    _brightness = value;
    buildModesetBuffer();
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
    assert(_width < (1 << 16));

    _modeset[0] = 0xfb;
    _modeset[1] = 0x00;
    _modeset[2] = 0x09;
    _modeset[3] = static_cast<uint8_t>(255.0f * _brightness);

    uint16_t w = static_cast<uint16_t>(_width);
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
