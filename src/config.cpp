#include "config.h"

#ifdef WOKWI_SIMULATOR
const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *mqttUsername = "";
const char *mqttPassword = "";
#else
#include "wifi_credentials.h"
#ifndef MQTT_USERNAME
#error "Define MQTT_USERNAME in include/wifi_credentials.h"
#endif
#ifndef MQTT_PASSWORD
#error "Define MQTT_PASSWORD in include/wifi_credentials.h"
#endif
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *mqttUsername = MQTT_USERNAME;
const char *mqttPassword = MQTT_PASSWORD;
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
