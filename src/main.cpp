#include "M5Dial.h"
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <time.h>
#include "config.h"

#ifdef DEBUG
  #define DBG_BEGIN(baud)   Serial.begin(baud)
  #define DBG_PRINT(...)    Serial.print(__VA_ARGS__)
  #define DBG_PRINTLN(...)  Serial.println(__VA_ARGS__)
  #define DBG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  #define DBG_BEGIN(baud)
  #define DBG_PRINT(...)
  #define DBG_PRINTLN(...)
  #define DBG_PRINTF(...)
#endif

// Grove Port A: GPS TX -> G13 (ESP32 RX), GPS RX -> G15 (ESP32 TX)
#define GPS_RX_PIN         13
#define GPS_TX_PIN         15
#define GPS_BAUD           9600
#define GPS_UPDATE_PERIOD  10000  // ms between GPS fixes (UBX-CFG-RATE)

#define NTP_PORT        123
#define NTP_PACKET_SIZE 48
#define NTP_EPOCH_OFFSET 2208988800UL  // seconds between 1 Jan 1900 and 1 Jan 1970

TinyGPSPlus gps;
AsyncUDP    udp;
bool        fixAcquired    = false;
long        lastEncoderPos = 0;
int         utcOffsetHours = 0;   // adjusted by encoder, range -12..+14

// Snapshot updated each time a fresh GPS sentence is parsed
volatile uint32_t gpsNtpSeconds = 0;  // NTP seconds at last GPS update
volatile uint32_t gpsSnapshotMs = 0;  // millis() at that moment

// --- GPS configuration ------------------------------------------------------

// Send UBX-CFG-RATE to set the NEO-6M measurement period
void setGpsUpdateRate(uint16_t periodMs) {
    uint8_t payload[6] = {
        (uint8_t)(periodMs & 0xFF), (uint8_t)(periodMs >> 8),  // measRate (ms)
        0x01, 0x00,  // navRate = 1 cycle
        0x01, 0x00,  // timeRef = GPS time
    };

    // Fletcher checksum over class, id, length, payload
    uint8_t ckA = 0, ckB = 0;
    uint8_t header[] = {0x06, 0x08, 0x06, 0x00};
    for (uint8_t b : header)  { ckA += b; ckB += ckA; }
    for (uint8_t b : payload) { ckA += b; ckB += ckA; }

    Serial2.write(0xB5); Serial2.write(0x62);  // UBX sync chars
    Serial2.write(header, 4);
    Serial2.write(payload, 6);
    Serial2.write(ckA);  Serial2.write(ckB);
    Serial2.flush();
    DBG_PRINTF("GPS update rate set to %ums\n", periodMs);
}

// --- Time helpers -----------------------------------------------------------

// Convert TinyGPS date+time to NTP seconds (call only when gps.time.isUpdated())
uint32_t gpsToNtp() {
    struct tm t = {};
    t.tm_year = gps.date.year() - 1900;
    t.tm_mon  = gps.date.month() - 1;
    t.tm_mday = gps.date.day();
    t.tm_hour = gps.time.hour();
    t.tm_min  = gps.time.minute();
    t.tm_sec  = gps.time.second();
    t.tm_isdst = 0;
    time_t unix = mktime(&t);
    return (uint32_t)(unix + NTP_EPOCH_OFFSET);
}

// Current NTP time interpolated from last GPS snapshot + elapsed millis
void currentNtpTime(uint32_t &seconds, uint32_t &fraction) {
    uint32_t elapsed = millis() - gpsSnapshotMs;
    seconds  = gpsNtpSeconds + elapsed / 1000;
    // NTP fraction: 2^32 units per second, so 1ms = 4294967 units
    fraction = (elapsed % 1000) * 4294967UL;
}

// Time with UTC offset applied — uses interpolated NTP time so gmtime handles rollovers
void getDisplayTime(int &h, int &m) {
    uint32_t sec, frac;
    currentNtpTime(sec, frac);
    time_t unix = (time_t)(sec - NTP_EPOCH_OFFSET) + utcOffsetHours * 3600;
    struct tm *t = gmtime(&unix);
    h = t->tm_hour; m = t->tm_min;
}

// Write a 32-bit value big-endian into a buffer
static void writeU32BE(uint8_t *buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >>  8) & 0xFF;
    buf[3] = (val      ) & 0xFF;
}

// --- Display helpers --------------------------------------------------------

const int CX = 120;  // display centre x
const int CY = 120;  // display centre y

// Small status messages used during WiFi/GPS init phases
void displayMessage(const char *line1, const char *line2 = nullptr) {
    M5Dial.Display.clear();
    M5Dial.Display.setTextFont(&fonts::FreeSans9pt7b);
    M5Dial.Display.setTextSize(1);
    if (line2) {
        M5Dial.Display.drawString(line1, CX, CY - 16);
        M5Dial.Display.drawString(line2, CX, CY + 16);
    } else {
        M5Dial.Display.drawString(line1, CX, CY);
    }
}

// Full clock face
// Layout (240x240 circular):
//   y=100  H:MM     — Orbitron_Light_32 size 2  (~64px tall)
//   y=162  AM/PM    — Orbitron_Light_32 size 1  (~32px tall)
//   y=200  UTC±N    — FreeSans9pt7b size 1
void displayClock(bool valid, int h24, int m, const char *label) {
    M5Dial.Display.clear();

    if (valid) {
        bool pm  = h24 >= 12;
        int  h12 = h24 % 12;
        if (h12 == 0) h12 = 12;

        char hmBuf[8];
        snprintf(hmBuf, sizeof(hmBuf), "%d:%02d", h12, m);

        M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
        M5Dial.Display.setTextSize(2);
        M5Dial.Display.drawString(hmBuf, CX, 100);

        M5Dial.Display.setTextSize(1);
        M5Dial.Display.drawString(pm ? "PM" : "AM", CX, 162);

        M5Dial.Display.setTextFont(&fonts::FreeSans9pt7b);
        M5Dial.Display.drawString(label, CX, 200);
    } else {
        M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
        M5Dial.Display.setTextSize(2);
        M5Dial.Display.drawString("--:--", CX, 100);

        M5Dial.Display.setTextFont(&fonts::FreeSans9pt7b);
        M5Dial.Display.setTextSize(1);
        M5Dial.Display.drawString("waiting for GPS", CX, 192);
    }
}

// --- NTP server -------------------------------------------------------------

void startNtpServer() {
    if (!udp.listen(NTP_PORT)) {
        DBG_PRINTLN("Failed to start NTP server on port 123");
        return;
    }
    DBG_PRINTLN("NTP server listening on port 123");

    udp.onPacket([](AsyncUDPPacket packet) {
        if (packet.length() < NTP_PACKET_SIZE) return;

        uint8_t response[NTP_PACKET_SIZE] = {};

        // Serve synchronised time as soon as GPS has valid time (includes hot-start
        // cached RTC time — reliable without needing a full location fix)
        uint8_t liVnMode;
        if (gps.time.isValid() && gps.date.isValid()) {
            liVnMode = 0x24;  // LI=0 (no warning), VN=4, Mode=4 (server)
        } else {
            liVnMode = 0xE4;  // LI=3 (unsynchronised), VN=4, Mode=4
        }

        uint32_t nowSec, nowFrac;
        currentNtpTime(nowSec, nowFrac);

        response[0]  = liVnMode;
        response[1]  = 1;       // Stratum 1 — primary GPS reference
        response[2]  = 4;       // Poll interval (2^4 = 16s)
        response[3]  = 0xFA;    // Precision (-6 ~ 15ms)

        // Root delay and root dispersion (both ~0 for GPS)
        writeU32BE(response + 4, 0);
        writeU32BE(response + 8, 0);

        // Reference identifier: "GPS\0"
        response[12] = 'G'; response[13] = 'P';
        response[14] = 'S'; response[15] = 0;

        // Reference timestamp (when clock was last set — the GPS snapshot)
        writeU32BE(response + 16, gpsNtpSeconds);
        writeU32BE(response + 20, 0);

        // Originate timestamp — copy T1 from client request (bytes 40-47)
        response[24] = packet.data()[40];
        response[25] = packet.data()[41];
        response[26] = packet.data()[42];
        response[27] = packet.data()[43];
        response[28] = packet.data()[44];
        response[29] = packet.data()[45];
        response[30] = packet.data()[46];
        response[31] = packet.data()[47];

        // Receive timestamp
        writeU32BE(response + 32, nowSec);
        writeU32BE(response + 36, nowFrac);

        // Transmit timestamp (re-sample just before sending)
        currentNtpTime(nowSec, nowFrac);
        writeU32BE(response + 40, nowSec);
        writeU32BE(response + 44, nowFrac);

        packet.write(response, NTP_PACKET_SIZE);

        DBG_PRINTF("NTP request from %s — served %lu.%03lu\n",
            packet.remoteIP().toString().c_str(),
            (unsigned long)nowSec,
            (unsigned long)(nowFrac / 4294967UL));
    });
}

// --- Setup / Loop -----------------------------------------------------------

void setup() {
    auto cfg = M5.config();
#ifdef DEBUG
    cfg.serial_baudrate = 115200;
#endif
    M5Dial.begin(cfg, true, false);
    DBG_BEGIN(115200);

    // Ensure mktime() treats tm as UTC
    setenv("TZ", "UTC0", 1);
    tzset();

    // Display setup — colour and datum are set once; font/size vary per draw call
    M5Dial.Display.setBrightness(64);  // ~25% — adjust 0-255 to taste
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextDatum(middle_center);

    // GPS
    Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    delay(100);  // allow GPS UART to settle before sending UBX config
    setGpsUpdateRate(GPS_UPDATE_PERIOD);
    DBG_PRINTLN("GPS initialised");

    // WiFi
    displayMessage("WiFi...");
    DBG_PRINTF("Connecting to %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        // Keep feeding GPS while waiting so we don't miss sentences
        while (Serial2.available()) gps.encode(Serial2.read());
        delay(250);
    }
    WiFi.setSleep(false);  // disable modem sleep — reduces UDP latency for NTP
    DBG_PRINTF("WiFi connected — IP: %s\n", WiFi.localIP().toString().c_str());

    char ipLine[20];
    snprintf(ipLine, sizeof(ipLine), "%s", WiFi.localIP().toString().c_str());
    displayMessage("Connected", ipLine);
    delay(2000);

    displayClock(false, 0, 0, "UTC");
    DBG_PRINTLN("Waiting for GPS fix...");

    startNtpServer();
}

void loop() {
    M5Dial.update();

    while (Serial2.available()) {
        gps.encode(Serial2.read());
    }

    // Snapshot time whenever a fresh GPS sentence arrives
    if (gps.time.isUpdated() && gps.date.isValid() && gps.time.isValid()) {
        gpsNtpSeconds = gpsToNtp();
        gpsSnapshotMs = millis();
    }

    if (!fixAcquired && gps.location.isValid()) {
        fixAcquired = true;
        DBG_PRINTLN("GPS fix acquired!");
    }

    static uint32_t lastDisplay = 0;

    // Button press resets offset to UTC
    if (M5Dial.BtnA.wasPressed()) {
        utcOffsetHours = 0;
        M5Dial.Encoder.write(0);
        lastEncoderPos = 0;
        lastDisplay    = 0;  // force immediate redraw
    }

    // Encoder adjusts UTC offset (each step = 1 hour)
    long pos = M5Dial.Encoder.read();
    if (pos != lastEncoderPos) {
        utcOffsetHours += (int)(pos - lastEncoderPos);
        utcOffsetHours  = constrain(utcOffsetHours, -12, 14);
        lastEncoderPos  = pos;
        lastDisplay     = 0;  // force immediate redraw
    }

    // Build timezone label
    char label[10];
    if (utcOffsetHours == 0)
        strcpy(label, "UTC");
    else
        snprintf(label, sizeof(label), "UTC%+d", utcOffsetHours);

    // Update display on minute change or immediately after encoder change
    static int lastMinute = -1;
    int curMinute = -1;
    if (gps.time.isValid()) curMinute = (gps.time.minute() + utcOffsetHours * 60) % 60;
    if (lastDisplay == 0 || curMinute != lastMinute) {
        lastMinute  = curMinute;
        lastDisplay = millis();
        if (gps.time.isValid() && gps.date.isValid()) {
            int h, m;
            getDisplayTime(h, m);
            displayClock(true, h, m, label);
        } else {
            displayClock(false, 0, 0, label);
        }
    }
}
