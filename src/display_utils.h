#ifndef DISPLAY_UTILS_H
#define DISPLAY_UTILS_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;

void drawRetroProgressBar(int x, int y, int width, int height, int progress);
void drawProgressBar(int x, int y, int width, int height, int progress);
void drawWifiIcon(int x, int y);
void drawAnimatedBean(int x, int y);
void resetAnimatedCat();
void drawAnimatedCat();
bool animatedCatFinished();
String getLocalTimeStr();

#endif
