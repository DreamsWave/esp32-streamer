#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h" // Встроенная библиотека для работы со временем
#include "bitmaps.h"

// --- НАСТРОЙКИ WI-FI ---
const char *ssid = "Wokwi-GUEST";
const char *password = "";

WebServer server(80);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int state = 0; // 0 = idle, 1 = active
int currentScene = 0;
int loadingSceneProgress = 0;
int mainSceneProgress = 0;
String streamerName = "STREAMER";
String streamerTitle = "Cool title";
unsigned long testTimer = 0;
bool testTriggered = false;

unsigned long lastBeanAnimMillis = 0;
int currentBeanFrame = 0;
const int beanAnimSpeed = 150; // Скорость анимации (в миллисекундах на кадр)

// --- НАСТРОЙКИ NTP ВРЕМЕНИ ---
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600; // Часовой пояс (например, UTC+3 Москва: 3 * 3600 = 10800)
const int daylightOffset_sec = 0;    // Смещение летнего времени (0, если нет)

// --- ПЕРЕМЕННЫЕ ДЛЯ БЕГУЩЕЙ СТРОКИ ---
unsigned long lastScrollMillis = 0;
unsigned long scrollDelayTimer = 0; // Таймер для отложенного старта скроллинга
int scrollPos = 0;
const int scrollSpeed = 250;
int closingCircleRadius = 0; // Радиус расширяющегося черного круга

// --- БАЗА ДАННЫХ ДЛЯ РАНДОМНОГО ТЕСТА В WOKWI ---
const int TEST_DATA_SIZE = 4;
const char *testStreamers[TEST_DATA_SIZE] = {
    "shroud",
    "xQc",
    "Ninja",
    "glorp_streamer"};
const char *testTitles[TEST_DATA_SIZE] = {
    "Chilling in Valorant ranked! [Drops On]",
    "REACTING TO MEMES AND TIKTOKS ALL DAY !!!",
    "Fortnite with squad! New season update hype!",
    "Testing glorpOS webhook on ESP32 in real time"};

void drawRetroProgressBar(int x, int y, int width, int height, int progress);
void drawProgressBar(int x, int y, int width, int height, int progress);
void drawWifiIcon(int x, int y);
String getLocalTimeStr();
void drawAnimatedBean(int x, int y);

void handleWebhook()
{
  if (server.hasArg("plain") == false)
  {
    server.send(400, "text/plain", "Body not found");
    return;
  }

  String jsonBody = server.arg("plain");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonBody);

  if (error)
  {
    server.send(400, "text/plain", "JSON Parsing Failed");
    return;
  }

  if (doc["user_name"].is<const char *>())
    streamerName = doc["user_name"].as<String>();
  if (doc["title"].is<const char *>())
    streamerTitle = doc["title"].as<String>();

  loadingSceneProgress = 0;
  mainSceneProgress = 0;
  currentScene = 0;
  scrollPos = 0;
  scrollDelayTimer = 0; // Сбрасываем таймер задержки текста для нового стрима
  state = 1;
  closingCircleRadius = 0;

  display.ssd1306_command(SSD1306_DISPLAYON);

  server.send(200, "text/plain", "glorpOS Activated");
}

void setup()
{
  Serial.begin(115200);

  randomSeed(analogRead(0));

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  // --- ЛОГИ ЗАГРУЗКИ НА ЭКРАНЕ ПРИ СТАРТЕ ---
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

  // Инициализация и синхронизация времени через NTP
  display.println(F("Syncing NTP Time..."));
  display.display();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  display.println(F("Starting Server..."));
  display.display();

  server.on("/stream-trigger", HTTP_POST, handleWebhook);
  server.begin();

  display.println(F("Server: OK! Port 80"));
  display.println(F("Entering IDLE state..."));
  display.display();

  delay(4000);

  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);

  Serial.println(F("Setup finished. System is in IDLE. Waiting for triggers..."));
}

void loop()
{
  server.handleClient();

  // --- БЛОК АВТО-ТЕСТИРОВАНИЯ ДЛЯ WOKWI (С РАНДОМОМ) ---
  if (!testTriggered)
  {
    if (testTimer == 0)
    {
      testTimer = millis();
    }

    if (millis() - testTimer > 5000)
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
      scrollDelayTimer = 0; // Сбрасываем таймер задержки перед началом теста
      state = 1;

      display.ssd1306_command(SSD1306_DISPLAYON);
      Serial.print(F("Wokwi Trigger: Simulated alert for @"));
      Serial.println(streamerName);
    }
  }

  if (state == 0)
  {
    delay(10);
    return;
  }

  display.clearDisplay();

  // --- СЦЕНА 0: ЗАГРУЗКА glorpOS ---
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
  // --- СЦЕНА 1: ДАННЫЕ О СТРИМЕ ---
  else if (currentScene == 1)
  {
    display.drawBitmap(0, 0, main_scene_gui_bitmap, 128, 64, WHITE);
    drawProgressBar(2, 28, 120, 1, mainSceneProgress);
    drawAnimatedBean(1, 57);

    mainSceneProgress += 1;

    if (mainSceneProgress >= 100)
    {
      mainSceneProgress = 100;
      closingCircleRadius = 0; // Сбрасываем радиус перед стартом
      currentScene = 2;        // Переключаемся на Сцену 2 (круг)
      return;
    }

    display.setTextSize(0);
    display.setTextColor(BLACK);
    display.setCursor(15, 5);
    display.print(streamerName);

    // --- ИНИЦИАЛИЗАЦИЯ ТАЙМЕРА ЗАДЕРЖКИ ПРИ СТАРТЕ СЦЕНЫ ---
    if (scrollDelayTimer == 0)
    {
      scrollDelayTimer = millis(); // Засекаем точное время входа на сцену
    }

    // --- ЛОГИКА БЕГУЩЕЙ СТРОКИ С ОТЛОЖЕННЫМ СТАРТОМ ---
    String displayText = streamerTitle;
    int maxVisibleChars = 19;

    if (displayText.length() > maxVisibleChars)
    {
      String paddedText = streamerTitle + "   ***   ";

      // Если с момента показа сцены прошло больше 2000 мс (2 секунды) — включаем скроллинг
      if (millis() - scrollDelayTimer > 2000)
      {
        if (millis() - lastScrollMillis > scrollSpeed)
        {
          scrollPos++;
          if (scrollPos >= paddedText.length())
          {
            scrollPos = 0;
          }
          lastScrollMillis = millis();
        }
        displayText = paddedText.substring(scrollPos, scrollPos + maxVisibleChars);
      }
      else
      {
        // Пока 2 секунды не прошло, жестко держим начало строки
        displayText = paddedText.substring(0, maxVisibleChars);
      }

      if (displayText.length() < maxVisibleChars)
      {
        int remaining = maxVisibleChars - displayText.length();
        displayText += paddedText.substring(0, remaining);
      }
    }

    display.setCursor(6, 19);
    display.setTextColor(WHITE);
    display.print(displayText);

    // --- ОТРИСОВКА ЧАСОВ И WI-FI В ПРАВОМ НИЖНЕМ УГЛУ ---
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(98, 57); // Позиционируем часы
    display.print(getLocalTimeStr());

    drawWifiIcon(91, 58); // Позиционируем иконку Wi-Fi правее часов
  }

  else if (currentScene == 2)
  {
    // 1. Отрисовываем статичный кадр интерфейса со Сцены 1
    display.drawBitmap(0, 0, main_scene_gui_bitmap, 128, 64, WHITE);
    drawProgressBar(2, 28, 120, 1, 100); // Полоса уже полная
    drawAnimatedBean(1, 57);             // Персонаж на месте

    display.setTextSize(0);
    display.setTextColor(BLACK);
    display.setCursor(15, 5);
    display.print(streamerName);

    display.setCursor(6, 19);
    display.setTextColor(WHITE);
    display.print(streamerTitle.substring(0, 19)); // Статичный текст, чтобы не дергался

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(98, 57);
    display.print(getLocalTimeStr());
    drawWifiIcon(91, 58);

    // 2. НАКЛАДЫВАЕМ ЧЕРНЫЙ КРУГ ИЗ ЦЕНТРА ЭКРАНА (64, 32)
    display.fillCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, closingCircleRadius, BLACK);

    closingCircleRadius += 12; // Скорость расширения (чем больше число, тем быстрее закроется)

    // 3. Когда круг полностью покрыл углы экрана (радиус 75) — тушим матрицу
    if (closingCircleRadius >= 75)
    {
      display.clearDisplay();
      display.display();

      display.ssd1306_command(SSD1306_DISPLAYOFF); // Аппаратное выключение
      state = 0;                                   // Переход в режим ожидания вебхука от Make.com

      // Сброс таймеров симуляции Wokwi для автоповтора теста
      testTimer = 0;
      testTriggered = false;

      Serial.println(F("Iris Out Finished. Screen is OFF (Idle)."));
      return;
    }
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

void drawProgressBar(int x, int y, int width, int height, int progress)
{
  int currentWidth = (width * progress) / 100;

  if (currentWidth > 0)
  {
    display.fillRect(x + 2, y + 2, currentWidth, height, WHITE);
  }
}

// Функция динамической отрисовки ретро-иконки Wi-Fi уровня сигнала
void drawWifiIcon(int x, int y)
{
  long rssi = WiFi.RSSI();

  // В Wokwi RSSI всегда равен 0, поэтому для симулятора форсируем отличный сигнал (-50)
  if (rssi == 0)
    rssi = -50;

  // 1-я палочка (Слабый сигнал)
  if (rssi > -90)
    display.drawFastVLine(x, y + 4, 1, WHITE);
  // 2-я палочка (Средний сигнал)
  if (rssi > -80)
    display.drawFastVLine(x + 2, y + 2, 3, WHITE);
  // 3-я палочка (Хороший сигнал)
  if (rssi > -70)
    display.drawFastVLine(x + 4, y + 0, 5, WHITE);
}

// Функция форматирования времени в строку "ЧХ:ММ"
String getLocalTimeStr()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    return "00:00"; // Если не удалось считать внутренние часы
  }

  char timeStringBuff[6]; // Буфер для строки "HH:MM\0"
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

void drawAnimatedBean(int x, int y)
{
  // Переключение кадров по таймеру
  if (millis() - lastBeanAnimMillis > beanAnimSpeed)
  {
    currentBeanFrame++;
    if (currentBeanFrame >= bean_bitmap_allArray_LEN)
    {
      currentBeanFrame = 0;
    }
    lastBeanAnimMillis = millis();
  }

  // Передаем строку многомерного массива напрямую
  display.drawBitmap(x, y, bean_bitmap_allArray[currentBeanFrame], 6, 6, WHITE);
}
