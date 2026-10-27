#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "bitmaps.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int currentScene = 0;
int loadingSceneProgress = 0;
int mainSceneProgress = 0;
String streamerName = "STREAMER";
String streamerTitle = "Cool title";

void drawRetroProgressBar(int x, int y, int width, int height, int progress);
void drawProgressBar(int x, int y, int width, int height, int progress);

void setup()
{
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.clearDisplay();
  display.display();
}

void loop()
{
  display.clearDisplay();

  if (currentScene == 0)
  {
    display.drawBitmap(49, 5, glorp_bitmap, 30, 32, WHITE);
    drawRetroProgressBar(27, 36, 74, 9, loadingSceneProgress);

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(42, 49);
    display.print(F("glorpOS"));

    loadingSceneProgress += 4;

    if (loadingSceneProgress >= 100)
    {
      loadingSceneProgress = 100;

      display.display();
      delay(30);

      currentScene = 1;
      return;
    }
  }
  else if (currentScene == 1)
  {
    display.drawBitmap(0, 0, main_scene_gui_bitmap, 128, 64, WHITE);

    drawProgressBar(2, 28, 120, 1, mainSceneProgress);

    mainSceneProgress += 1;
    if (mainSceneProgress >= 100)
    {
      mainSceneProgress = 100;

      display.display();
      display.clearDisplay();

      currentScene = 2;
      return;
    }

    display.setTextSize(0);
    display.setTextColor(BLACK);
    display.setCursor(15, 5);
    display.print(streamerName);

    display.setCursor(6, 19);
    display.setTextColor(WHITE);
    display.print(streamerTitle);
  }

  display.display();
  delay(30);
}

void drawRetroProgressBar(int x, int y, int width, int height, int progress)
{
  display.drawRoundRect(x, y, width, height, 1, WHITE);

  int blockWidth = 2;
  int blockHeight = 5;
  int gap = 1;
  int step = blockWidth + gap;

  int innerMaxWidth = width - 4;
  int maxBlocks = (innerMaxWidth + gap) / step;
  int blocksToDraw = (maxBlocks * progress) / 100;

  for (int i = 0; i < blocksToDraw; i++)
  {
    int blockX = x + 2 + (i * step);
    display.fillRect(blockX, y + 2, blockWidth, blockHeight, WHITE);
  }
}

void drawProgressBar(int x, int y, int width, int height, int progress) {
  int currentWidth = (width * progress) / 100;
  
  if (currentWidth > 0) {
    display.fillRect(x + 2, y + 2, currentWidth, height, WHITE);
  }
}
