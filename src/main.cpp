#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"
#include "display_utils.h"

#include "bitmaps/bottom_main.h"
#include "bitmaps/glorp.h"
#include "bitmaps/top_status.h"
#include "bitmaps/window_glorp_signal.h"
#include "bitmaps/window_moon.h"

WebServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int state = 0;
int currentScene = 0;
int loadingSceneProgress = 0;
int mainSceneProgress = 0;
int closingCircleRadius = 0;

String streamerName = "STREAMER";
String streamerTitle = "Cool title";

#ifdef WOKWI_SIMULATOR
unsigned long testTimer = 0;
bool testTriggered = false;
#endif
unsigned long lastScrollMillis = 0;
unsigned long scrollDelayTimer = 0;
int scrollPos = 0;

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
  mainSceneProgress = 0;
  currentScene = 0;
  scrollPos = 0;
  scrollDelayTimer = 0;
  closingCircleRadius = 0;
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

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    display.print(F("."));
    display.display();
    Serial.print(".");
  }

  display.println(F("\nWiFi Connected!"));
  display.print(F("IP: "));
  display.println(WiFi.localIP());
  display.println(F("Syncing NTP Time..."));
  display.display();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/stream-trigger", HTTP_POST, handleWebhook);
  server.begin();

  display.println(F("Server: OK! Port 80"));
  display.println(F("Entering IDLE state..."));
  display.display();

  delay(4000);
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
}

void loop()
{
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
      mainSceneProgress = 0;
      currentScene = 0;
      scrollPos = 0;
      scrollDelayTimer = 0;
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
    display.drawBitmap(49, 19, glorp_bitmap, 30, 32, WHITE);
    drawRetroProgressBar(27, 50, 74, 9, loadingSceneProgress);

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(42, 5);
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
  // Scene 1: Stream Dashboard Display
  else if (currentScene == 1)
  {
    display.drawBitmap(0, 16, bottom_main_bitmap, 128, 48, WHITE);
    display.drawBitmap(90, 0, top_status_bitmap, 38, 16, WHITE);
    display.drawBitmap(8, 21, window_moon_bitmap, 41, 41, WHITE);
    display.drawBitmap(78, 32, window_glorp_signal_bitmap, 48, 30, WHITE);

    if (mainSceneProgress < 100)
      mainSceneProgress++;

    if (mainSceneProgress >= 100)
    {
      mainSceneProgress = 100;
      closingCircleRadius = 0;
      currentScene = 2;
      return;
    }

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(streamerName);
    drawChargeBar(102, 2, mainSceneProgress);

    if (scrollDelayTimer == 0)
      scrollDelayTimer = millis();

    String displayText = streamerTitle;
    int maxVisibleChars = 15;

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

    display.setCursor(0, 8);
    display.setTextColor(WHITE);
    display.print(displayText);

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(103, 7);
    display.print(getLocalTimeStr());

    drawWifiIcon(94, 9);
  }
  //   // Scene 2: Iris Out Closing Animation
  //   else if (currentScene == 2)
  //   {
  //     display.drawBitmap(0, 16, bottom_main_bitmap, 128, 48, WHITE);
  //     drawProgressBar(2, 28, 120, 1, 100);

  //     display.setTextSize(1);
  //     display.setTextColor(BLACK);
  //     display.setCursor(0, 0);
  //     display.print(streamerName);

  //     display.setCursor(0, 8);
  //     display.setTextColor(WHITE);
  //     display.print(streamerTitle.substring(0, 15));

  //     display.setTextSize(1);
  //     display.setTextColor(WHITE);
  //     display.setCursor(103, 7);
  //     display.print(getLocalTimeStr());
  //     drawWifiIcon(94, 9);

  //     display.fillCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, closingCircleRadius, BLACK);
  //     closingCircleRadius += 12;

  //     if (closingCircleRadius >= 75)
  //     {
  //       display.clearDisplay();
  //       display.display();
  //       display.ssd1306_command(SSD1306_DISPLAYOFF);
  //       state = 0;
  // #ifdef WOKWI_SIMULATOR
  //       testTimer = 0;
  //       testTriggered = false;
  // #endif
  //       return;
  //     }
  //   }

  display.display();
  delay(30);
}