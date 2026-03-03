#include "DisplayManager.h"
#include <Wire.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 DisplayManager::oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire,
                                      OLED_RESET);

void DisplayManager::init() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.println("Booting...");
  oled.display();
}

void DisplayManager::clear() { oled.clearDisplay(); }

void DisplayManager::display() { oled.display(); }

void DisplayManager::drawHeader(const char *title) {
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Inverted text for header
  oled.setCursor(0, 0);

  // Pad title with spaces to fill width
  String headerStr = String(title);
  while (headerStr.length() < 21)
    headerStr += " ";
  oled.print(headerStr);

  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // Reset to normal
}

void DisplayManager::drawMenu(const char *title, const char **items,
                              uint8_t numItems, uint8_t selectedIndex,
                              uint8_t scrollOffset) {
  clear();
  drawHeader(title);

  uint8_t startY = 12;
  uint8_t visibleItems = 5;

  for (uint8_t i = 0; i < visibleItems; i++) {
    uint8_t itemIndex = scrollOffset + i;
    if (itemIndex >= numItems)
      break;

    uint8_t yPos = startY + (i * 10);
    oled.setCursor(0, yPos);

    if (itemIndex == selectedIndex) {
      oled.print("> ");
    } else {
      oled.print("  ");
    }

    oled.print(items[itemIndex]);
  }
  display();
}

void DisplayManager::drawCenteredText(const char *text, uint8_t y) {
  int16_t x1, y1;
  uint16_t w, h;
  oled.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int16_t xPos = (SCREEN_WIDTH - w) / 2;
  if (xPos < 0)
    xPos = 0;

  oled.setCursor(xPos, y);
  oled.print(text);
}

void DisplayManager::drawProgressBar(uint8_t x, uint8_t y, uint8_t width,
                                     uint8_t height, uint8_t progressPercent) {
  oled.drawRect(x, y, width, height, SSD1306_WHITE);
  if (progressPercent > 100)
    progressPercent = 100;
  uint8_t fillWidth = ((width - 2) * progressPercent) / 100;
  if (fillWidth > 0) {
    oled.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
  }
}

Adafruit_SSD1306 &DisplayManager::getDisplay() { return oled; }
