#include "M5Dial.h"

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5Dial.begin(cfg, true, false);
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
    M5Dial.Display.setTextSize(2);
    Serial.begin(115200);
    Serial.printf("test1234\n");
}

long oldPosition = -999;

void loop() {
    M5Dial.update();
    long newPosition = M5Dial.Encoder.read();

    if (newPosition != oldPosition) {

        if (newPosition > oldPosition) {
            Serial.printf("RIGHT new pos=%ld\n",newPosition);
        } else if (newPosition < oldPosition) {
            Serial.printf("LEFT new pos=%ld\n",newPosition);
        }

        //M5Dial.Speaker.tone(8000, 20);
        M5Dial.Display.clear();
        oldPosition = newPosition;
        Serial.println(newPosition);
        M5Dial.Display.drawString(String(newPosition),
                                  M5Dial.Display.width() / 2,
                                  M5Dial.Display.height() / 2);
    }
    if (M5Dial.BtnA.wasPressed()) {
        M5Dial.Encoder.readAndReset();
    }
    if (M5Dial.BtnA.pressedFor(5000)) {
        M5Dial.Encoder.write(100);
    }
}
