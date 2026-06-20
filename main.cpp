#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NonBlockingRTTTL.h>
#include <time.h>

// --- Hardware Pins ---
// Custom I2C pins to avoid conflict with the camera
#define OLED_SDA 14
#define OLED_SCL 15
#define BUZZER_PIN 13

// --- Display Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Network Credentials ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// --- Audio Melodies ---
// 10-Minute Warning: A brief, polite 3-note ascending chime
const char *warningMelody = "Warning:d=4,o=5,b=120:8c6,8e6,2g6";

// Adhan Alert: A distinct, longer pattern using a Hijaz-style scale
const char *adhanMelody = "Adhan:d=4,o=5,b=90:2c,2c#,4e,4f,2g,2g,4g,4f,4e,4c#,2c";

// --- State Variables ---
float latitude = 0.0;
float longitude = 0.0;
long tzOffsetSecs = 0; // Timezone offset in seconds
int lastDay = -1;

// Stores prayer times as minutes since midnight
int prayerTimes[5] = {0}; 
const char* prayerNames[5] = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};

bool warningPlayed[5] = {false};
bool adhanPlayed[5] = {false};

// --- Helper Functions ---

// Converts "HH:MM" API string to total minutes from midnight
int parseTime(const char* timeStr) {
    if (!timeStr) return 0;
    int h = (timeStr[0] - '0') * 10 + (timeStr[1] - '0');
    int m = (timeStr[3] - '0') * 10 + (timeStr[4] - '0');
    return h * 60 + m;
}

// 1. Get Location & Timezone based on current IP Address
bool fetchLocationData() {
    HTTPClient http;
    http.begin("http://ip-api.com/json/?fields=lat,lon,offset,status");
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        StaticJsonDocument<512> doc;
        deserializeJson(doc, payload);
        
        if (String(doc["status"].as<const char*>()) == "success") {
            latitude = doc["lat"];
            longitude = doc["lon"];
            tzOffsetSecs = doc["offset"];
            return true;
        }
    }
    return false;
}

// 2. Fetch Today's Timings via ISNA (Method 2)
bool fetchPrayerTimes() {
    HTTPClient http;
    char url[256];
    // method=2 is ISNA (Islamic Society of North America)
    sprintf(url, "http://api.aladhan.com/v1/timings?latitude=%f&longitude=%f&method=2", latitude, longitude);
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, payload);
        
        JsonObject timings = doc["data"]["timings"];
        prayerTimes[0] = parseTime(timings["Fajr"]);
        prayerTimes[1] = parseTime(timings["Dhuhr"]);
        prayerTimes[2] = parseTime(timings["Asr"]);
        prayerTimes[3] = parseTime(timings["Maghrib"]);
        prayerTimes[4] = parseTime(timings["Isha"]);
        return true;
    }
    return false;
}

// 3. UI Renderer
void updateDisplay(int h, int m, int nextIndex, int minsLeft) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    // Left Column: List all 5 Prayers
    display.setTextSize(1);
    for (int i = 0; i < 5; i++) {
        display.setCursor(0, i * 12);
        if (i == nextIndex) display.print(">"); // Current target indicator
        else display.print(" ");
        
        display.print(prayerNames[i]);
        display.print(" ");
        
        int ph = prayerTimes[i] / 60;
        int pm = prayerTimes[i] % 60;
        if (ph < 10) display.print("0");
        display.print(ph);
        display.print(":");
        if (pm < 10) display.print("0");
        display.print(pm);
    }
    
    // Right Column: Clock & Countdown
    display.setCursor(72, 5);
    display.setTextSize(2); // Large clock
    if (h < 10) display.print("0");
    display.print(h);
    display.print(":");
    if (m < 10) display.print("0");
    display.print(m);
    
    display.setTextSize(1);
    display.setCursor(72, 30);
    display.print("Next:");
    display.setCursor(72, 42);
    display.print(prayerNames[nextIndex]);
    
    // Time remaining
    display.setCursor(72, 54);
    int hLeft = minsLeft / 60;
    int mLeft = minsLeft % 60;
    display.print("-");
    if (hLeft > 0) {
        display.print(hLeft);
        display.print("h ");
    }
    display.print(mLeft);
    display.print("m");
    
    display.display();
}

void setup() {
    Serial.begin(115200);
    
    // Initialize specific I2C pins for ESP32-CAM
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println("OLED init failed");
        for(;;);
    }
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Connecting WiFi...");
    display.display();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("Fetching Location...");
    display.display();
    
    while (!fetchLocationData()) {
        delay(2000); // Retry until IP resolves location
    }

    // Sync hardware clock with NTP based on local timezone offset
    configTime(tzOffsetSecs, 0, "pool.ntp.org");
    
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("Fetching Times...");
    display.display();
    
    while (!fetchPrayerTimes()) {
        delay(2000);
    }
}

void loop() {
    // Keep the RTTTL library executing audio non-blockingly
    rtttl::play(); 
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return; // Wait until NTP syncs
    
    int currentMins = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    
    // Midnight reset: Refresh the API for the new day's math
    if (timeinfo.tm_mday != lastDay) {
        if (lastDay != -1) {
            fetchPrayerTimes();
            for (int i = 0; i < 5; i++) {
                warningPlayed[i] = false;
                adhanPlayed[i] = false;
            }
        }
        lastDay = timeinfo.tm_mday;
    }
    
    // Calculate the next upcoming prayer
    int nextIndex = -1;
    for (int i = 0; i < 5; i++) {
        if (currentMins < prayerTimes[i]) {
            nextIndex = i;
            break;
        }
    }
    
    // If all prayers are finished today, the next is Fajr tomorrow
    int targetIndex = (nextIndex == -1) ? 0 : nextIndex;
    int targetTime = prayerTimes[targetIndex];
    if (nextIndex == -1) {
        targetTime += 24 * 60; 
    }
    
    int minsLeft = targetTime - currentMins;
    
    // Alarm triggers
    for (int i = 0; i < 5; i++) {
        // 10-Minute Warning
        if (!warningPlayed[i] && currentMins == (prayerTimes[i] - 10)) {
            rtttl::begin(BUZZER_PIN, warningMelody);
            warningPlayed[i] = true;
        }
        
        // Actual Adhan Time
        if (!adhanPlayed[i] && currentMins == prayerTimes[i]) {
            rtttl::begin(BUZZER_PIN, adhanMelody);
            adhanPlayed[i] = true;
        }
    }
    
    // We update the OLED frequently, but not fast enough to lag the audio
    static unsigned long lastOledUpdate = 0;
    if (millis() - lastOledUpdate > 1000) {
        updateDisplay(timeinfo.tm_hour, timeinfo.tm_min, targetIndex, minsLeft);
        lastOledUpdate = millis();
    }
    
    delay(50); // Slight delay saves CPU without choking the RTTTL audio pulse
}
