# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Flash Commands

```bash
# Build the project
pio run

# Build and upload to device
pio run --target upload

# Open serial monitor (115200 baud)
pio device monitor

# Build, upload, and monitor in one step
pio run --target upload && pio device monitor

# Clean build artifacts
pio run --target clean
```

## Hardware Target

- **Board:** M5Stack M5Dial (ESP32-S3, `m5stack-stamps3`)
- **Framework:** Arduino
- **USB:** CDC over USB (native USB serial — no UART adapter needed)
- **Serial baud:** 115200

Key hardware on the M5Dial:
- Rotary encoder (GPIO 40/41)
- Circular TFT display (~240×240)
- Single button (BtnA, accessed via the encoder knob press)
- RFID reader (MFRC522)
- Speaker, microphone, RTC

## Code Architecture

All application code lives in `src/main.cpp` as a standard Arduino sketch (`setup()` / `loop()`).

### Library Stack

```
M5Dial.h          ← board-level wrapper (encoder, RFID init)
  └─ M5Unified    ← unified hardware API (display, buttons, speaker, IMU, RTC, power)
       └─ M5GFX   ← graphics primitives and font rendering
```

- `M5Dial.begin(cfg, enableEncoder, enableRFID)` — initializes all hardware
- `M5Dial.update()` — must be called each loop iteration to update button/encoder state
- `M5Dial.Display` — M5GFX display object (drawString, clear, setTextColor, etc.)
- `M5Dial.Encoder` — PJRC-style encoder: `read()`, `write(val)`, `readAndReset()`
- `M5Dial.BtnA` — button: `wasPressed()`, `pressedFor(ms)`
- `M5Dial.Speaker` — `tone(freq, durationMs)`

### Display

- Display center: `M5Dial.Display.width()/2`, `M5Dial.Display.height()/2`
- Text datum `middle_center` centers text on the given coordinates
- Available fonts in M5GFX: `fonts::Orbitron_Light_32`, `fonts::FreeSans9pt7b`, etc.

## Dependencies

Managed by PlatformIO, declared in `platformio.ini`:
- `m5stack/M5Dial@^1.0.3` — fetched to `.pio/libdeps/m5stack-stamps3/`
