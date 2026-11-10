#include "display_utils.h"
#include <WiFi.h>
#include "config.h"

void drawRetroProgressBar(int x, int y, int width, int height, int progress)
{
    display.drawRoundRect(x, y, width, height, 1, WHITE);
    int blockWidth = 2, blockHeight = 5, gap = 1, step = blockWidth + gap;
    int innerMaxWidth = width - 4;
    int maxBlocks = (innerMaxWidth + gap) / step;
    int blocksToDraw = (maxBlocks * progress) / 100;

    for (int i = 0; i < blocksToDraw; i++)
    {
        display.fillRect(x + 2 + (i * step), y + 2, blockWidth, blockHeight, WHITE);
    }
}

void drawProgressBar(int x, int y, int width, int height, int progress)
{
    int currentWidth = (width * progress) / 100;
    if (currentWidth > 0)
    {
        display.fillRect(x + 2, y + 2, currentWidth, height, WHITE);
    }
}

void drawChargeBar(int x, int y, int progress)
{
    constexpr int segmentCount = 9;
    constexpr int segmentWidth = 2;
    constexpr int segmentGap = 1;
    int boundedProgress = constrain(progress, 0, 100);
    int remainingSegments = ((100 - boundedProgress) * segmentCount + 99) / 100;

    for (int i = 0; i < remainingSegments; i++)
    {
        display.drawFastHLine(x + i * (segmentWidth + segmentGap), y, segmentWidth, WHITE);
    }
}

void drawWifiIcon(int x, int y)
{
    long rssi = WiFi.RSSI();
    if (rssi == 0)
        rssi = -50;
    if (rssi > -90)
        display.drawFastVLine(x, y + 4, 1, WHITE);
    if (rssi > -80)
        display.drawFastVLine(x + 2, y + 3, 2, WHITE);
    if (rssi > -70)
        display.drawFastVLine(x + 4, y + 1, 4, WHITE);
}

String getLocalTimeStr()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return "0000";
    char timeStringBuff[6];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%H%M", &timeinfo);
    return String(timeStringBuff);
}
