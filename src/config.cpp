#include "config.h"

const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;
const int daylightOffset_sec = 0;
const int beanAnimSpeed = 150;
const int scrollSpeed = 250;

const char *testStreamers[TEST_DATA_SIZE] = {
    "Glorp", "Walorp", "PLORPIA", "glorp_streamer"};
const char *testTitles[TEST_DATA_SIZE] = {
    "Testing glorpOS webhook on ESP32 in real time",
    "Chilling in Valorant ranked! [Drops On]",
    "REACTING TO MEMES AND TIKTOKS ALL DAY !!!",
    "Fortnite with squad! New season update hype!"};
