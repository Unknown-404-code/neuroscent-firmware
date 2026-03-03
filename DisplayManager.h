#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "Config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class DisplayManager {
public:
  static void init();

  // Core drawing
  static void clear();
  static void display();

  // Helper drawing functions
  static void drawHeader(const char *title);
  static void drawMenu(const char *title, const char **items, uint8_t numItems,
                       uint8_t selectedIndex, uint8_t scrollOffset);
  static void drawCenteredText(const char *text, uint8_t y);
  static void drawProgressBar(uint8_t x, uint8_t y, uint8_t width,
                              uint8_t height, uint8_t progressPercent);

  // Expose the display object if custom drawing is needed
  static Adafruit_SSD1306 &getDisplay();

private:
  static Adafruit_SSD1306 oled;
};

#endif // DISPLAY_MANAGER_H
