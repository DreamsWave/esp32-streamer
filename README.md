# ESP32 Streamer

An ESP32-powered OLED display that shows a streamer alert animation when it receives an MQTT message. The production firmware connects to Wi-Fi, synchronizes its clock with NTP, and subscribes to an MQTT topic over TLS. A Wokwi build is also provided for trying the display animation without an MQTT broker.

## Hardware

- ESP32 DevKit (configured as `esp32dev` in PlatformIO)
- 128x64 SSD1306 I2C OLED display

Connect the OLED to the ESP32:

| OLED | ESP32 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

The Wokwi wiring is defined in [diagram.json](./diagram.json).

## Set up the project

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the **PlatformIO IDE** extension.
2. Clone this repository and open its folder in VS Code. PlatformIO will install the Espressif32 platform and libraries from `platformio.ini` on the first build.
3. For a physical ESP32, create `include/wifi_credentials.h` with your own Wi-Fi and EMQX credentials:

   ```cpp
   #define WIFI_SSID "your-wifi-name"
   #define WIFI_PASSWORD "your-wifi-password"
   #define MQTT_USERNAME "your-emqx-username"
   #define MQTT_PASSWORD "your-emqx-password"
   ```

   This file is intentionally ignored by Git. Do not commit or share it. The Wokwi build uses its own guest Wi-Fi settings and does not require this file.

4. Build and upload the production firmware:

   ```sh
   pio run -e esp32dev
   pio run -e esp32dev -t upload
   pio device monitor
   ```

   Alternatively, use the PlatformIO Build, Upload, and Monitor buttons in VS Code. Select the correct serial port if PlatformIO does not detect it automatically. Serial output runs at 115200 baud.

The broker host, TLS port, and topic are configured in `src/main.cpp`. The time zone offset and NTP server are configured in `src/config.cpp`. Change these if you use a different broker or time zone.

## Try it in Wokwi

Install the Wokwi extension for VS Code, then build the simulator firmware:

```sh
pio run -e wokwi
```

Start the simulation from VS Code with the Wokwi extension. It uses `wokwi.toml` and `diagram.json`. Wokwi generates a sample alert after startup; this simulator build intentionally does not connect to MQTT.

## Send a test alert

The ESP32 subscribes to the `esp32/stream-trigger` topic. Publish a JSON message with both `user_name` and `title` string fields:

```json
{"user_name":"LocalTest","title":"Тестовый заголовок стрима"}
```

Russian titles are displayed with Cyrillic glyphs and scroll without breaking UTF-8 characters.

### With MQTTX

Install [MQTTX](https://mqttx.app/) and create an MQTT connection using:

- Host: `t421e1c9.ala.eu-central-1.emqxsl.com`
- Port: `8883`
- Protocol: MQTT over TLS
- Username and password: your EMQX credentials

Connect, publish the JSON message above to `esp32/stream-trigger`, and confirm the alert appears on the OLED. The ESP32 must be powered on, connected to Wi-Fi, and connected to the broker. Keep credentials private; don't save them in shared or public connection exports.

### With Mosquitto command-line tools

Install the Mosquitto **clients** package (the broker package alone does not include `mosquitto_pub`). For example, on Ubuntu:

```sh
sudo apt install mosquitto-clients
```

In fish, enter credentials without echoing the password, publish, and remove the temporary shell variables:

```fish
read -P "MQTT username: " MQTT_USER
read -sP "MQTT password: " MQTT_PASS
echo

mosquitto_pub --tls-use-os-certs \
  -h t421e1c9.ala.eu-central-1.emqxsl.com -p 8883 \
  -u "$MQTT_USER" -P "$MQTT_PASS" \
  -t esp32/stream-trigger \
  -m '{"user_name":"LocalTest","title":"Testing from my PC"}'

set -e MQTT_USER MQTT_PASS
```

### With Make.com

Make.com can trigger the same alert by publishing an MQTT message to the broker and topic above, with the same JSON fields. Use your EMQX credentials in the Make connection. The shared scenario page is [ESP32 Streamer on Make.com](https://eu1.make.com/public/shared-scenario/ErwA4PCAXkm/esp32-streamer-dreamswave); configure or run the scenario there as appropriate. This is a Make scenario page, not an HTTP endpoint on the ESP32.

## Troubleshooting

- **The display stays off or blank:** check OLED power, ground, SDA/SCL wiring, and the I2C address (`0x3C` in `src/config.h`).
- **Wi-Fi does not connect:** verify `WIFI_SSID` and `WIFI_PASSWORD`, including capitalization, and check that the network allows 2.4 GHz devices.
- **MQTT does not connect:** check the EMQX username and password, confirm the ESP32 has internet access and valid NTP time, and check the serial monitor for MQTT connection status.
- **The test message has no effect:** ensure the device is subscribed to exactly `esp32/stream-trigger` and the payload contains string-valued `user_name` and `title` properties.
- **The time reads `0000`:** the device displays `0000` until NTP has synchronized. Check its internet access and the configured NTP server.
