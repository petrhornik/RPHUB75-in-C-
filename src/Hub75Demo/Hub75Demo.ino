#include "Display.hpp"

Display display; // the only object you need now

// A tiny 8x8 RGB888 "texture" generated at compile time (row-major,
// top-down, 3 bytes/pixel) so the sketch runs with no extra assets.
static uint8_t texture[8 * 8 * 3];

static void buildDemoTexture() {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            uint8_t* px = &texture[(y * 8 + x) * 3];
            px[0] = x * 32;
            px[1] = y * 32;
            px[2] = 255 - x * 32;
        }
    }
}

void setup() {
    display.begin(); // your board's pins, 64x64, correct rotation -- all defaults
    display.setBrightness(0.1f);
    buildDemoTexture();
}

void loop() {
    static float t = 0;
    t += 0.05f;

    display.clear();

    // Individual pixels -- plain 0xRRGGBB, full precision.
    display.setPixel(0, 0, 0xFF0000);
    display.setPixel(display.width() - 1, display.height() - 1, 0x00FF00);
    display.setPixel(display.width() - 1, 0, 0x0000FF);
    display.setPixel(0, display.height() - 1, 0xFFFFFF);

    // Text, one call.
    display.drawText(2, 2, "Ahoj", 0xFFFFFF);

    // Původní výpočet pro osu X
    int max_x = display.width() - 8;
    int tx = (max_x / 2) + static_cast<int>((max_x / 2) * sin(t));

    // Nový výpočet pro osu Y
    int min_y = 14;                                 // Začátek pohybu
    int max_y = display.height() - 8;           // 2px mezera odspodu, 8px výška textury
    int range_y = max_y - min_y;                    // Celková dráha pohybu

    // Spočítáme aktuální Y pozici (použijeme kosinus pro nezávislý pohyb)
    int ty = min_y + (range_y / 2) + static_cast<int>((range_y / 2) * cos(t * 1.3f));

    // Vykreslení textury s novými souřadnicemi tx a ty
    display.drawImage(tx, ty, 8, 8, texture);

    // Shapes, also one call, also 0xRRGGBB.
    display.drawRect(0, 12, display.width(), 2, 0x3C3C3C);

    display.show();
    delay(20); // ~60 fps = 16 & ~30fps = 33
}

/*
 * Converting a real image into the RGB888 layout drawImage() expects:
 *
 *   from PIL import Image
 *   img = Image.open("texture.png").convert("RGB").resize((8, 8))
 *   data = img.tobytes()
 *   with open("texture.h", "w") as f:
 *       f.write("static const uint8_t texture[] = {")
 *       f.write(",".join(str(b) for b in data))
 *       f.write("};\n")
 */
