#include "config.h"

#ifdef WOKWI_SIMULATOR
const char *ssid = "Wokwi-GUEST";
const char *password = "";
#else
#include "wifi_credentials.h"
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
#endif

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;
const int daylightOffset_sec = 0;
const int scrollSpeed = 250;

const char *testStreamers[TEST_DATA_SIZE] = {
    "Glorp", "Walorp", "PLORPIA", "glorp_streamer"};
const char *testTitles[TEST_DATA_SIZE] = {
    "Testing glorpOS webhook on ESP32 in real time",
    "Chilling in Valorant ranked! [Drops On]",
    "REACTING TO MEMES AND TIKTOKS ALL DAY !!!",
    "Fortnite with squad! New season update hype!"};
