#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <cstring>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/TomThumb.h>

#include "config.h"
#include "display_utils.h"

#include "bitmaps/bottom_main.h"
#include "bitmaps/glorp.h"
#include "bitmaps/glorp_hover.h"
#include "bitmaps/top_status.h"
#include "bitmaps/rail_frames.h"
#include "bitmaps/signal.h"
#include "bitmaps/star-1.h"
#include "bitmaps/window_glorp_signal.h"
#include "bitmaps/window_moon.h"

constexpr int WINDOW_X = 8;
constexpr int WINDOW_Y = 21;
constexpr int WINDOW_CONTENT_X = WINDOW_X + 2;
constexpr int WINDOW_CONTENT_Y = WINDOW_Y + 4;
constexpr int WINDOW_CONTENT_WIDTH = 37;
constexpr int WINDOW_CONTENT_HEIGHT = 30;
constexpr int GLORP_HOVER_WIDTH = 27;
constexpr int GLORP_HOVER_HEIGHT = 29;
constexpr int GLORP_HOVER_END_Y_OFFSET = 5;
constexpr unsigned long GLORP_HOVER_STEP_INTERVAL = 40;
constexpr unsigned long GLORP_HOVER_DELAY = 5000;
constexpr unsigned long RAIL_FRAME_INTERVAL = 200;
constexpr unsigned long RAIL_REPEAT_DELAY = 2000;
constexpr unsigned long SIGNAL_WAVE_STEP_INTERVAL = 150;
constexpr int SIGNAL_X = 81;
constexpr int SIGNAL_Y = 37;
constexpr unsigned long STAR1_FRAME_INTERVAL_MIN = 1600;
constexpr unsigned long STAR1_FRAME_INTERVAL_MAX = 2400;
constexpr unsigned long STAR2_FRAME_INTERVAL_MIN = 425;
constexpr unsigned long STAR2_FRAME_INTERVAL_MAX = 575;
constexpr unsigned long STAR2_REPEAT_DELAY_MIN = 2700;
constexpr unsigned long STAR2_REPEAT_DELAY_MAX = 3300;
constexpr uint8_t STAR1_FRAME_COUNT = 2;
constexpr uint8_t STAR2_FRAME_COUNT = 5;
constexpr unsigned long WIFI_CONNECT_TIMEOUT = 20000;
constexpr unsigned long WIFI_RETRY_INTERVAL = 10000;
constexpr int MAIN_SCENE_DURATION_FRAMES = 200;
constexpr uint8_t FADE_LEVEL_COUNT = 16;

WebServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
GFXcanvas1 windowCanvas(WINDOW_CONTENT_WIDTH, WINDOW_CONTENT_HEIGHT);
uint8_t endingFrame[SCREEN_WIDTH * SCREEN_HEIGHT / 8];
uint8_t endingFadeLevel = 0;
unsigned long wifiAttemptStartedAt = 0;
unsigned long lastWifiRetryAt = 0;
bool wifiConnecting = false;
bool networkServicesStarted = false;

int state = 0;
int currentScene = 0;
int loadingSceneProgress = 0;
int loadingFadeProgress = 0;
int mainSceneProgress = 0;
int glorpHoverX = -GLORP_HOVER_WIDTH;
unsigned long glorpHoverDelayStart = 0;
unsigned long lastGlorpHoverStep = 0;
bool glorpHoverDelayStarted = false;
bool glorpHoverStarted = false;
bool glorpHoverFinished = false;

uint8_t railAnimationFrame = 0;
unsigned long lastRailFrameAt = 0;
bool railAnimationPaused = false;
uint8_t signalAnimationStage = 0;
unsigned long lastSignalWaveStepAt = 0;
struct StarAnimationState
{
  int16_t x;
  int16_t y;
  uint8_t frame;
  unsigned long lastFrameAt;
  unsigned long frameInterval;
  unsigned long repeatDelay;
  bool paused;
};

StarAnimationState star1Animations[] = {
    {69, 30, 0, 0, 0, 0, false},
    {52, 43, 0, 0, 0, 0, false},
    {3, 45, 0, 0, 0, 0, false}};
StarAnimationState star2Animations[] = {
    {51, 26, 0, 0, 0, 0, false},
    {97, 22, 0, 0, 0, 0, false}};
String streamerName = "STREAMER";
String streamerTitle = "Cool title";

#ifdef WOKWI_SIMULATOR
unsigned long testTimer = 0;
bool testTriggered = false;
#endif
unsigned long lastScrollMillis = 0;
unsigned long scrollDelayTimer = 0;
int scrollPos = 0;

void handleWebhook();

void applyDitherFade(uint8_t level, bool fadeIn)
{
  static constexpr uint8_t bayerMatrix[4][4] = {
      {0, 8, 2, 10},
      {12, 4, 14, 6},
      {3, 11, 1, 9},
      {15, 7, 13, 5}};
  uint8_t *buffer = display.getBuffer();
  level = constrain(level, 0, FADE_LEVEL_COUNT);

  for (int16_t y = 0; y < SCREEN_HEIGHT; y++)
  {
    for (int16_t x = 0; x < SCREEN_WIDTH; x++)
    {
      const bool clearPixel = fadeIn
                                  ? bayerMatrix[y & 3][x & 3] >= level
                                  : bayerMatrix[y & 3][x & 3] < level;
      if (clearPixel)
      {
        const uint16_t index = x + (y / 8) * SCREEN_WIDTH;
        buffer[index] &= static_cast<uint8_t>(~(1U << (y & 7)));
      }
    }
  }
}

void showWifiFailure()
{
  Serial.printf("WiFi connection failed (status %d); retrying.\n",
                static_cast<int>(WiFi.status()));

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(F("WiFi connection failed"));
  display.println(F("Retrying..."));
  display.display();
}

void startNetworkServices()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(F("WiFi Connected!"));
  display.print(F("IP: "));
  display.println(WiFi.localIP());
  display.println(F("Syncing NTP Time..."));
  display.display();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/stream-trigger", HTTP_POST, handleWebhook);
  server.begin();
  networkServicesStarted = true;

  display.println(F("Server: OK! Port 80"));
  display.println(F("Entering IDLE state..."));
  display.display();

  delay(4000);
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
}

void resetGlorpHoverAnimation()
{
  glorpHoverX = -GLORP_HOVER_WIDTH;
  glorpHoverDelayStart = 0;
  lastGlorpHoverStep = 0;
  glorpHoverDelayStarted = false;
  glorpHoverStarted = false;
  glorpHoverFinished = false;
}

void resetRailAnimation()
{
  railAnimationFrame = 0;
  lastRailFrameAt = millis();
  railAnimationPaused = false;
  signalAnimationStage = 0;
  lastSignalWaveStepAt = lastRailFrameAt;
}

void resetStarAnimations()
{
  const unsigned long now = millis();
  for (StarAnimationState &star : star1Animations)
  {
    star.frame = 0;
    star.frameInterval = random(STAR1_FRAME_INTERVAL_MIN,
                                STAR1_FRAME_INTERVAL_MAX + 1);
    star.lastFrameAt = now - random(0, star.frameInterval);
    star.paused = false;
  }

  for (StarAnimationState &star : star2Animations)
  {
    star.frame = 0;
    star.frameInterval = random(STAR2_FRAME_INTERVAL_MIN,
                                STAR2_FRAME_INTERVAL_MAX + 1);
    star.lastFrameAt = now - random(0, star.frameInterval);
    star.repeatDelay = STAR2_REPEAT_DELAY_MIN;
    star.paused = false;
  }
}

void drawGlorpHoverAnimation()
{
  unsigned long now = millis();
  if (!glorpHoverDelayStarted)
  {
    glorpHoverDelayStart = now;
    glorpHoverDelayStarted = true;
  }

  if (!glorpHoverStarted && now - glorpHoverDelayStart >= GLORP_HOVER_DELAY)
  {
    glorpHoverStarted = true;
    lastGlorpHoverStep = now;
  }

  if (glorpHoverStarted && !glorpHoverFinished &&
      now - lastGlorpHoverStep >= GLORP_HOVER_STEP_INTERVAL)
  {
    glorpHoverX++;
    lastGlorpHoverStep = now;
    if (glorpHoverX >= WINDOW_CONTENT_WIDTH)
      glorpHoverFinished = true;
  }

  windowCanvas.fillScreen(BLACK);
  windowCanvas.drawBitmap(-2, -4, window_moon_bitmap, 41, 41, WHITE);
  if (glorpHoverStarted && !glorpHoverFinished)
  {
    const int travelDistance = WINDOW_CONTENT_WIDTH + GLORP_HOVER_WIDTH;
    const int horizontalProgress = glorpHoverX + GLORP_HOVER_WIDTH;
    const int glorpHoverY = (horizontalProgress * GLORP_HOVER_END_Y_OFFSET +
                             travelDistance / 2) /
                            travelDistance;
    windowCanvas.drawBitmap(glorpHoverX, glorpHoverY, glorp_hover_black_bitmap,
                            GLORP_HOVER_WIDTH, GLORP_HOVER_HEIGHT, BLACK);
    windowCanvas.drawBitmap(glorpHoverX, glorpHoverY, glorp_hover_bitmap,
                            GLORP_HOVER_WIDTH, GLORP_HOVER_HEIGHT, WHITE);
  }
  display.drawBitmap(WINDOW_CONTENT_X, WINDOW_CONTENT_Y, windowCanvas.getBuffer(),
                     WINDOW_CONTENT_WIDTH, WINDOW_CONTENT_HEIGHT, WHITE, BLACK);

  display.drawFastHLine(WINDOW_X + 1, WINDOW_Y + 3, 39, WHITE);
  display.drawFastVLine(WINDOW_X + 1, WINDOW_Y + 4, WINDOW_CONTENT_HEIGHT, WHITE);
  display.drawFastVLine(WINDOW_X + 39, WINDOW_Y + 4, WINDOW_CONTENT_HEIGHT, WHITE);
}

void drawRailAnimation()
{
  const unsigned long now = millis();
  if (railAnimationPaused)
  {
    if (now - lastRailFrameAt >= RAIL_REPEAT_DELAY)
    {
      railAnimationPaused = false;
      lastRailFrameAt = now;
    }
  }
  else if (now - lastRailFrameAt >= RAIL_FRAME_INTERVAL)
  {
    if (railAnimationFrame == RAIL_FRAME_COUNT - 1)
    {
      railAnimationFrame = 0;
      railAnimationPaused = true;
    }
    else
    {
      railAnimationFrame++;
    }
    lastRailFrameAt = now;
  }

  const uint8_t firstPixel = pgm_read_byte(&railFrameOffsets[railAnimationFrame]);
  const uint8_t endPixel = pgm_read_byte(&railFrameOffsets[railAnimationFrame + 1]);
  for (uint8_t i = firstPixel; i < endPixel; i++)
  {
    const uint8_t x = pgm_read_byte(&railPixelCoordinates[i * 2]);
    const uint8_t y = pgm_read_byte(&railPixelCoordinates[i * 2 + 1]);
    display.drawPixel(x, y, WHITE);
  }
}

void drawSignalAnimation()
{
  const unsigned long now = millis();
  if (now - lastSignalWaveStepAt >= SIGNAL_WAVE_STEP_INTERVAL)
  {
    signalAnimationStage = (signalAnimationStage + 1) % 5;
    lastSignalWaveStepAt = now;
  }

  for (int y = 0; y < 9; y++)
  {
    for (int x = 0; x < 9; x++)
    {
      const uint8_t rowByte = pgm_read_byte(&signal_bitmap_[y * 2 + x / 8]);
      if ((rowByte & (0x80 >> (x & 7))) == 0)
        continue;

      const int wave = x + y;
      const bool visible = signalAnimationStage == 1
                               ? wave >= 11
                           : signalAnimationStage == 2
                               ? wave >= 8
                               : signalAnimationStage >= 3 && wave >= 5;
      if (visible)
        display.drawPixel(SIGNAL_X + x, SIGNAL_Y + y, WHITE);
    }
  }
}

void drawStarAnimations()
{
  const unsigned long now = millis();
  for (StarAnimationState &star : star1Animations)
  {
    if (now - star.lastFrameAt >= star.frameInterval)
    {
      star.frame = (star.frame + 1) % STAR1_FRAME_COUNT;
      star.lastFrameAt = now;
      star.frameInterval = random(STAR1_FRAME_INTERVAL_MIN,
                                  STAR1_FRAME_INTERVAL_MAX + 1);
    }

    display.drawBitmap(star.x, star.y, star1_bitmap + star.frame * 3,
                       3, 3, WHITE);
  }

  for (StarAnimationState &star : star2Animations)
  {
    if (star.paused)
    {
      if (now - star.lastFrameAt >= star.repeatDelay)
      {
        star.frame = 0;
        star.paused = false;
        star.lastFrameAt = now;
        star.frameInterval = random(STAR2_FRAME_INTERVAL_MIN,
                                    STAR2_FRAME_INTERVAL_MAX + 1);
      }
    }
    else if (now - star.lastFrameAt >= star.frameInterval)
    {
      star.frame++;
      star.lastFrameAt = now;
      if (star.frame == STAR2_FRAME_COUNT - 1)
      {
        star.paused = true;
        star.repeatDelay = random(STAR2_REPEAT_DELAY_MIN,
                                  STAR2_REPEAT_DELAY_MAX + 1);
      }
      else
      {
        star.frameInterval = random(STAR2_FRAME_INTERVAL_MIN,
                                    STAR2_FRAME_INTERVAL_MAX + 1);
      }
    }

    display.drawBitmap(star.x, star.y, star2_bitmap + star.frame * 5,
                       5, 5, WHITE);
  }
}

void handleWebhook()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "text/plain", "Body not found");
    return;
  }
  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));

  if (doc["user_name"].is<const char *>())
    streamerName = doc["user_name"].as<String>();
  if (doc["title"].is<const char *>())
    streamerTitle = doc["title"].as<String>();

  loadingSceneProgress = 0;
  loadingFadeProgress = 0;
  mainSceneProgress = 0;
  endingFadeLevel = 0;
  currentScene = 0;
  scrollPos = 0;
  scrollDelayTimer = 0;
  resetGlorpHoverAnimation();
  resetRailAnimation();
  resetStarAnimations();
  state = 1;

  display.ssd1306_command(SSD1306_DISPLAYON);
  server.send(200, "text/plain", "glorpOS Activated");
}

void setup()
{
  Serial.begin(115200);
  randomSeed(analogRead(0));

  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.ssd1306_command(SSD1306_DISPLAYON);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(F("glorpOS Booting..."));
  display.println(F("---------------------"));
  display.print(F("Connecting to WiFi."));
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  wifiConnecting = true;
  wifiAttemptStartedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - wifiAttemptStartedAt < WIFI_CONNECT_TIMEOUT)
  {
    delay(500);
    display.print(F("."));
    display.display();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    wifiConnecting = false;
    startNetworkServices();
  }
  else
  {
    wifiConnecting = false;
    lastWifiRetryAt = millis();
    showWifiFailure();
  }
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    unsigned long now = millis();
    if (wifiConnecting && now - wifiAttemptStartedAt >= WIFI_CONNECT_TIMEOUT)
    {
      wifiConnecting = false;
      lastWifiRetryAt = now;
      showWifiFailure();
    }

    if (!wifiConnecting && now - lastWifiRetryAt >= WIFI_RETRY_INTERVAL)
    {
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      wifiConnecting = true;
      wifiAttemptStartedAt = now;
    }

    delay(10);
    return;
  }

  wifiConnecting = false;
  if (!networkServicesStarted)
    startNetworkServices();

  server.handleClient();

#ifdef WOKWI_SIMULATOR
  if (!testTriggered)
  {
    if (testTimer == 0)
      testTimer = millis();

    if (millis() - testTimer > 2000)
    {
      testTriggered = true;
      int randomStreamerIndex = random(0, TEST_DATA_SIZE);
      int randomTitleIndex = random(0, TEST_DATA_SIZE);

      streamerName = String(testStreamers[randomStreamerIndex]);
      streamerTitle = String(testTitles[randomTitleIndex]);

      loadingSceneProgress = 0;
      loadingFadeProgress = 0;
      mainSceneProgress = 0;
      endingFadeLevel = 0;
      currentScene = 0;
      scrollPos = 0;
      scrollDelayTimer = 0;
      resetGlorpHoverAnimation();
      resetRailAnimation();
      resetStarAnimations();
      state = 1;

      display.ssd1306_command(SSD1306_DISPLAYON);
      Serial.print(F("Wokwi Trigger: Simulated alert for @"));
      Serial.println(streamerName);
    }
  }
#endif

  if (state == 0)
  {
    delay(10);
    return;
  }

  display.clearDisplay();

  // Scene 0: glorpOS Loading Screen
  if (currentScene == 0)
  {
    loadingSceneProgress = min(loadingSceneProgress + 2, 100);
    loadingFadeProgress = min(loadingFadeProgress + 4, 100);
    display.drawBitmap(49, 19, glorp_bitmap, 30, 32, WHITE);
    drawRetroProgressBar(27, 50, 74, 9, loadingSceneProgress);

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(42, 5);
    display.print(F("glorpOS"));

    applyDitherFade(
        static_cast<uint8_t>(loadingFadeProgress * FADE_LEVEL_COUNT / 100), true);

    if (loadingSceneProgress >= 100)
    {
      display.display();
      delay(30);
      currentScene = 1;
      return;
    }
  }
  else if (currentScene == 2)
  {
    memcpy(display.getBuffer(), endingFrame, sizeof(endingFrame));
    applyDitherFade(endingFadeLevel, false);
    display.display();
    delay(30);

    if (endingFadeLevel >= FADE_LEVEL_COUNT)
    {
      display.clearDisplay();
      display.display();
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      state = 0;
      currentScene = 0;
#ifdef WOKWI_SIMULATOR
      testTimer = 0;
      testTriggered = false;
#endif
    }
    else
    {
      endingFadeLevel++;
    }
    return;
  }
  // Scene 1: Stream Dashboard Display
  else if (currentScene == 1)
  {
    display.drawBitmap(0, 16, bottom_main_bitmap, 128, 48, WHITE);
    display.drawBitmap(90, 0, top_status_bitmap, 38, 16, WHITE);
    display.drawBitmap(WINDOW_X, WINDOW_Y, window_moon_bitmap, 41, 41, WHITE);
    drawGlorpHoverAnimation();
    display.drawBitmap(78, 32, window_glorp_signal_bitmap, 48, 30, WHITE);
    drawRailAnimation();
    drawSignalAnimation();
    drawStarAnimations();

    if (mainSceneProgress < MAIN_SCENE_DURATION_FRAMES)
      mainSceneProgress++;

    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(streamerName);
    drawChargeBar(
        102, 2, mainSceneProgress * 100 / MAIN_SCENE_DURATION_FRAMES);

    if (scrollDelayTimer == 0)
      scrollDelayTimer = millis();

    String displayText = streamerTitle;
    int maxVisibleChars = 24;

    if (displayText.length() > maxVisibleChars)
    {
      String paddedText = streamerTitle + "   ***   ";

      if (millis() - scrollDelayTimer > 2000)
      {
        if (millis() - lastScrollMillis > scrollSpeed)
        {
          scrollPos++;
          if (scrollPos >= paddedText.length())
            scrollPos = 0;
          lastScrollMillis = millis();
        }
        displayText = paddedText.substring(scrollPos, scrollPos + maxVisibleChars);
      }
      else
      {
        displayText = paddedText.substring(0, maxVisibleChars);
      }

      if (displayText.length() < maxVisibleChars)
      {
        int remaining = maxVisibleChars - displayText.length();
        displayText += paddedText.substring(0, remaining);
      }
    }

    display.setFont(&TomThumb);
    display.setCursor(0, 15);
    display.setTextColor(WHITE);
    display.print(displayText);

    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(103, 7);
    display.print(getLocalTimeStr());

    drawWifiIcon(94, 9);
  }
  display.display();
  delay(30);

  if (currentScene == 1 &&
      mainSceneProgress >= MAIN_SCENE_DURATION_FRAMES &&
      glorpHoverFinished)
  {
    memcpy(endingFrame, display.getBuffer(), sizeof(endingFrame));
    endingFadeLevel = 0;
    currentScene = 2;
  }
}