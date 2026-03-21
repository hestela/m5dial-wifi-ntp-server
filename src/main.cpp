#include "M5Dial.h"
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <time.h>
#include "config.h"

// Grove Port A: GPS TX -> G13 (ESP32 RX), GPS RX -> G15 (ESP32 TX)
#define GPS_RX_PIN 13
#define GPS_TX_PIN 15
#define GPS_BAUD   9600

#define NTP_PORT        123
#define NTP_PACKET_SIZE 48
#define NTP_EPOCH_OFFSET 2208988800UL  // seconds between 1 Jan 1900 and 1 Jan 1970

TinyGPSPlus gps;
AsyncUDP    udp;
bool        fixAcquired = false;

// --- Time helpers -----------------------------------------------------------

// Convert TinyGPS date+time to seconds since NTP epoch (1 Jan 1900 UTC)
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
    // mktime treats tm as local time; we need UTC — apply timezone offset back
    // Simplest fix: set TZ=UTC before calling mktime (done in setup)
    return (uint32_t)(unix + NTP_EPOCH_OFFSET);
}

// Write a 32-bit value big-endian into a buffer
static void writeU32BE(uint8_t *buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >>  8) & 0xFF;
    buf[3] = (val      ) & 0xFF;
}

// --- Display helpers --------------------------------------------------------

void displayMessage(const char *line1, const char *line2 = nullptr) {
    M5Dial.Display.clear();
    if (line2) {
        M5Dial.Display.drawString(line1,
            M5Dial.Display.width() / 2,
            M5Dial.Display.height() / 2 - 20);
        M5Dial.Display.drawString(line2,
            M5Dial.Display.width() / 2,
            M5Dial.Display.height() / 2 + 20);
    } else {
        M5Dial.Display.drawString(line1,
            M5Dial.Display.width() / 2,
            M5Dial.Display.height() / 2);
    }
}

// --- NTP server -------------------------------------------------------------

void startNtpServer() {
    if (!udp.listen(NTP_PORT)) {
        Serial.println("Failed to start NTP server on port 123");
        return;
    }
    Serial.println("NTP server listening on port 123");

    udp.onPacket([](AsyncUDPPacket packet) {
        if (packet.length() < NTP_PACKET_SIZE) return;

        uint8_t response[NTP_PACKET_SIZE] = {};

        uint8_t liVnMode;
        if (fixAcquired && gps.time.isValid() && gps.date.isValid()) {
            liVnMode = 0x24;  // LI=0 (no warning), VN=4, Mode=4 (server)
        } else {
            liVnMode = 0xE4;  // LI=3 (unsynchronised), VN=4, Mode=4
        }

        uint32_t now = gpsToNtp();

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

        // Reference timestamp (when clock was last set)
        writeU32BE(response + 16, now);
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
        writeU32BE(response + 32, now);
        writeU32BE(response + 36, 0);

        // Transmit timestamp
        writeU32BE(response + 40, now);
        writeU32BE(response + 44, 0);

        packet.write(response, NTP_PACKET_SIZE);

        Serial.printf("NTP request from %s — served %lu\n",
            packet.remoteIP().toString().c_str(), (unsigned long)now);
    });
}

// --- Setup / Loop -----------------------------------------------------------

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5Dial.begin(cfg, false, false);
    Serial.begin(115200);

    // Ensure mktime() treats tm as UTC
    setenv("TZ", "UTC0", 1);
    tzset();

    // Display setup
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setTextFont(&fonts::FreeSans9pt7b);
    M5Dial.Display.setTextSize(1);

    // GPS
    Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.println("GPS initialised");

    // WiFi
    displayMessage("WiFi...");
    Serial.printf("Connecting to %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        // Keep feeding GPS while waiting so we don't miss sentences
        while (Serial2.available()) gps.encode(Serial2.read());
        delay(250);
    }
    Serial.printf("WiFi connected — IP: %s\n", WiFi.localIP().toString().c_str());

    char ipLine[20];
    snprintf(ipLine, sizeof(ipLine), "%s", WiFi.localIP().toString().c_str());
    displayMessage("Connected", ipLine);
    delay(2000);

    displayMessage("GPS fix...");
    Serial.println("Waiting for GPS fix...");

    startNtpServer();
}

void loop() {
    M5Dial.update();

    while (Serial2.available()) {
        gps.encode(Serial2.read());
    }

    if (!fixAcquired && gps.location.isValid()) {
        fixAcquired = true;
        Serial.println("GPS fix acquired!");
    }

    // Update display once per second
    static uint32_t lastDisplay = 0;
    if (fixAcquired && millis() - lastDisplay >= 1000) {
        lastDisplay = millis();
        char timeBuf[12];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
            gps.time.hour(), gps.time.minute(), gps.time.second());
        displayMessage("UTC", timeBuf);
    }
}
