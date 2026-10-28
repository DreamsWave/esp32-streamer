#include "display_utils.h"
#include <WiFi.h>
#include "bitmaps/cat.h"
#include "bitmaps/bean.h"
#include "config.h"

unsigned long lastBeanAnimMillis = 0;
int currentBeanFrame = 0;
constexpr int CAT_WIDTH = 16;
constexpr int CAT_HEIGHT = 16;
constexpr int CAT_GROUND_Y = 39;
constexpr int CAT_FALL_SPEED = 1;
constexpr unsigned long CAT_FALL_STEP_MS = 40;
constexpr unsigned long CAT_LAND_FRAME_MS = 140;
constexpr unsigned long CAT_SIT_FRAME_MS = 180;
constexpr unsigned long CAT_WALK_FRAME_MS = 120;
constexpr unsigned long CAT_WALK_STEP_MS = 100;
constexpr int CAT_FIRST_WALK_DISTANCE = 30;
constexpr int CAT_SECOND_WALK_DISTANCE = 16;

enum class CatState
{
    Falling,
    Landing,
    Sitting,
    Walking
};

const unsigned char *const catSitFrames[] = {
    cat_bitmap_sit_1, cat_bitmap_sit_2, cat_bitmap_sit_3, cat_bitmap_sit_4};
const unsigned char *const catLandFrames[] = {cat_bitmap_land_1, cat_bitmap_land_2};
const unsigned char *const catWalkFrames[] = {cat_bitmap_walk_1, cat_bitmap_walk_2};

CatState catState = CatState::Falling;
int catX = 110;
int catY = -CAT_HEIGHT;
int catFrame = 0;
int catWalksCompleted = 0;
int catWalkTargetX = catX;
unsigned long catStateStartedAt = 0;
unsigned long catLastFrameAt = 0;
unsigned long catLastFallStepAt = 0;
unsigned long catLastWalkStepAt = 0;
unsigned long catSitDuration = 0;

void startCatSitting(unsigned long now, unsigned long duration)
{
    catState = CatState::Sitting;
    catStateStartedAt = now;
    catLastFrameAt = now;
    catFrame = 0;
    catSitDuration = duration;
}

void drawCatFrame(const unsigned char *bitmap)
{
    display.drawBitmap(catX, catY, bitmap, CAT_WIDTH, CAT_HEIGHT, WHITE);
}

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

void drawWifiIcon(int x, int y)
{
    long rssi = WiFi.RSSI();
    if (rssi == 0)
        rssi = -50;
    if (rssi > -90)
        display.drawFastVLine(x, y + 4, 1, WHITE);
    if (rssi > -80)
        display.drawFastVLine(x + 2, y + 2, 3, WHITE);
    if (rssi > -70)
        display.drawFastVLine(x + 4, y + 0, 5, WHITE);
}

void drawAnimatedBean(int x, int y)
{
    if (millis() - lastBeanAnimMillis > beanAnimSpeed)
    {
        currentBeanFrame++;
        if (currentBeanFrame >= bean_bitmap_allArray_LEN)
            currentBeanFrame = 0;
        lastBeanAnimMillis = millis();
    }
    display.drawBitmap(x, y, bean_bitmap_allArray[currentBeanFrame], 6, 6, WHITE);
}

void resetAnimatedCat()
{
    const unsigned long now = millis();
    catState = CatState::Falling;
    catX = 110;
    catY = -CAT_HEIGHT;
    catFrame = 0;
    catWalksCompleted = 0;
    catWalkTargetX = catX;
    catStateStartedAt = now;
    catLastFrameAt = now;
    catLastFallStepAt = now;
    catLastWalkStepAt = now;
}

void drawAnimatedCat()
{
    const unsigned long now = millis();
    switch (catState)
    {
    case CatState::Falling:
        while (now - catLastFallStepAt >= CAT_FALL_STEP_MS && catY < CAT_GROUND_Y)
        {
            catY += CAT_FALL_SPEED;
            catLastFallStepAt += CAT_FALL_STEP_MS;
        }
        if (catY >= CAT_GROUND_Y)
        {
            catY = CAT_GROUND_Y;
            catState = CatState::Landing;
            catStateStartedAt = now;
            catLastFrameAt = now;
            catFrame = 0;
        }
        drawCatFrame(cat_bitmap_fall);
        break;

    case CatState::Landing:
        if (now - catStateStartedAt >= CAT_LAND_FRAME_MS * 2)
        {
            startCatSitting(now, 2000);
            drawCatFrame(catSitFrames[catFrame]);
        }
        else
        {
            catFrame = (now - catStateStartedAt) / CAT_LAND_FRAME_MS;
            drawCatFrame(catLandFrames[catFrame]);
        }
        break;

    case CatState::Sitting:
        if (catWalksCompleted < 2 && now - catStateStartedAt >= catSitDuration)
        {
            catState = CatState::Walking;
            catStateStartedAt = now;
            catLastFrameAt = now;
            catLastWalkStepAt = now;
            catFrame = 0;
            const int distance = catWalksCompleted == 0
                                     ? CAT_FIRST_WALK_DISTANCE
                                     : CAT_SECOND_WALK_DISTANCE;
            catWalkTargetX = catX > distance ? catX - distance : 0;
            drawCatFrame(catWalkFrames[catFrame]);
        }
        else
        {
            if (now - catLastFrameAt >= CAT_SIT_FRAME_MS)
            {
                catFrame = (catFrame + (now - catLastFrameAt) / CAT_SIT_FRAME_MS) % 4;
                catLastFrameAt += ((now - catLastFrameAt) / CAT_SIT_FRAME_MS) * CAT_SIT_FRAME_MS;
            }
            drawCatFrame(catSitFrames[catFrame]);
        }
        break;

    case CatState::Walking:
        if (now - catLastFrameAt >= CAT_WALK_FRAME_MS)
        {
            catFrame = (catFrame + (now - catLastFrameAt) / CAT_WALK_FRAME_MS) % 2;
            catLastFrameAt += ((now - catLastFrameAt) / CAT_WALK_FRAME_MS) * CAT_WALK_FRAME_MS;
        }
        while (now - catLastWalkStepAt >= CAT_WALK_STEP_MS && catX > catWalkTargetX)
        {
            catX--;
            catLastWalkStepAt += CAT_WALK_STEP_MS;
        }
        if (catX <= catWalkTargetX)
        {
            catX = catWalkTargetX;
            catWalksCompleted++;
            startCatSitting(now, catWalksCompleted == 1 ? 5000 : 0);
            drawCatFrame(catSitFrames[catFrame]);
        }
        else
        {
            drawCatFrame(catWalkFrames[catFrame]);
        }
        break;
    }
}

bool animatedCatFinished()
{
    return catState == CatState::Sitting && catWalksCompleted >= 2;
}

String getLocalTimeStr()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return "00:00";
    char timeStringBuff[6];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
    return String(timeStringBuff);
}
