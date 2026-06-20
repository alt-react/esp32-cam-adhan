# esp32-cam-adhan
get daily adhan times and reminders via a screen and a buzzer on the esp32 cam

this code is meant to work with platform io (not Arduino IDE)

the esp32 cam will need to be connected to the Internet at least once so that location can be described and adhan can be calculated automatically.

be sure to change the ssid and password in main.cpp to allow the esp32 cam to connect to the internet

Wiring Check
OLED SDA \rightarrow ESP32-CAM GPIO 14
OLED SCL \rightarrow ESP32-CAM GPIO 15
OLED VCC \rightarrow 3.3V
OLED GND \rightarrow GND
Buzzer Positive (+) \rightarrow ESP32-CAM GPIO 13
Buzzer Negative (-) \rightarrow GND