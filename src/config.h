#ifndef CONFIG_H
#define CONFIG_H

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define TEST_DATA_SIZE 4

extern const char *ssid;
extern const char *password;
extern const char *ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;
extern const int beanAnimSpeed;
extern const int scrollSpeed;
extern const char *testStreamers[TEST_DATA_SIZE];
extern const char *testTitles[TEST_DATA_SIZE];

#endif
