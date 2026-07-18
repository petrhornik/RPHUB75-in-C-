#include "Display.hpp"
#include "HubGfx.hpp"

// --- Adjust to your panel / wiring -----------------------------------
static constexpr int PANEL_WIDTH  = 64;
static constexpr int PANEL_HEIGHT = 64;

Display* display = nullptr;
HubGfx*  gfx      = nullptr;

// A tiny 8x8 RGB888 "texture" generated at compile time (row-major,
// top-down, 3 bytes/pixel) so the sketch runs with no extra assets.
// Swap this for a real converted image -- see the note at the bottom
// of the file for a Python snippet that produces this exact layout.
static uint8_t texture[8 * 8 * 3];

static void buildDemoTexture() {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            uint8_t* px = &texture[(y * 8 + x) * 3];
            px[0] = x * 32;       // R ramps left -> right
            px[1] = y * 32;       // G ramps top -> bottom
            px[2] = 255 - x * 32; // B ramps the other way
        }
    }
}

void setup() {
    DisplayConfig config = {
        .width = PANEL_WIDTH,
        .height = PANEL_HEIGHT,
        .rotation = -1,
        .spi = {
            .pin_ck = 3,
            .pin_cs = 1,
            .pin_d0 = 38,
            .pin_d1 = 39,
            .pin_d2 = 41,
            .pin_d3 = 45,
            .baud   = 20 * 1000 * 1000,
        },
    };

    // Heap-allocated in setup(), not as a global object -- SPI/GPIO
    // hardware init should happen after the core has finished booting,
    // not from a global constructor that may run too early.
    display = new Display(config);
    gfx     = new HubGfx(*display);

    display->setBrightness(0.3f);
    buildDemoTexture();
}

void loop() {
    static float t = 0;
    t += 0.05f;

    display->clear();

    // 1) Individual pixels, straight through Display::setPixel().
    display->setPixel(0, 0, 0xFF0000);
    display->setPixel(PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 0x00FF00);

    // 2) Text, via Adafruit_GFX on top of Display.
    gfx->setTextColor(HubGfx::rgb565(255, 255, 255));
    gfx->setCursor(2, 2);
    gfx->setTextSize(1);
    gfx->print("Ahoj");

    // 3) A texture/bitmap, drawn straight into the RGB888 framebuffer
    //    (full color precision, no 565 downconversion).
    int tx = 20 + static_cast<int>(8 * sin(t));
    display->drawImage(tx, 16, 8, 8, texture);

    // 4) Shapes are free too, courtesy of Adafruit_GFX:
    gfx->drawRect(0, 12, PANEL_WIDTH, 2, HubGfx::rgb565(60, 60, 60));

    display->show();
    delay(33); // ~30 fps
}

/*
 * Converting a real image into the RGB888 layout drawImage() expects
 * (row-major, top-down, 3 bytes/pixel), with Python + Pillow:
 *
 *   from PIL import Image
 *   img = Image.open("texture.png").convert("RGB").resize((8, 8))
 *   data = img.tobytes()  # already R,G,B,R,G,B,... row by row
 *   with open("texture.h", "w") as f:
 *       f.write("static const uint8_t texture[] = {")
 *       f.write(",".join(str(b) for b in data))
 *       f.write("};\n")
 *
 * Then #include "texture.h" instead of buildDemoTexture().
 * (On ESP32/S3, PROGMEM is a no-op -- flash is memory-mapped -- so a
 * plain `const uint8_t texture[]` is fine without any pgm_read_* calls.)
 */
