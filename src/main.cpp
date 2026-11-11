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