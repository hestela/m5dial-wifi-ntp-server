# M5Dial GPS NTP Server

An NTP server running on an M5Stack M5Dial (ESP32-S3) that derives time from a UART GPS module. You can get the time from the ESP using ntpdate and also looking at the LCD screen. There's also a simple HTTP API for GPS coordinates.

## Features

- **Stratum-1 NTP server** — serves GPS-derived UTC time over WiFi (UDP port 123)
- **RTC holdover** — the u-blox NEO-6M has a battery to hold the most recent time reading. The M5Dial's BM8563 RTC is used as a backup time source.
- **HTTP API** — `GET /gps` returns a JSON object with coordinates, altitude, speed, and satellite count
- **Clock display** — 12-hour clock on the circular TFT; rotary encoder adjusts UTC offset (±1 hour per step, −12..+14); short button press resets offset to UTC
- **Display sleep** — blanks after 1 minute of inactivity; any encoder or button activity wakes it
- **Power off** — hold the encoder button for 4 seconds (with an on display countdown) to power down; press ecoder button to wake. no power is used when off, as long as a USB cable is not connected.

## Hardware

| Component | Notes |
|-----------|-------|
| M5Stack M5Dial | ESP32-S3, 240×240 circular TFT, rotary encoder, RTC |
| u-blox NEO-6M GPS module (GY-NEO6MV1 or 2) | Connected via Grove Port A |
| 1S 3000 mAh LiPo battery with Micro JST 1.25 connector| other capacities work fine but anything bigger and you will need to make the enclosure bigger|
| Micro JST 1.25 connectors | one needed to plug into M5Dial and another used to connect to the battery |
| usb type-c breakout board with 5.1k pull down resistors | used as a panel mount connect to the M5Dial for programming and charging |
| grove 4 pin cable with header | try to get one without the locking tab, otherwise you can easily twist off the tab as it doesnt fit in the M5Dial port |
| unfinished usb type-c cable with 4 wires (for usb 2.0 pwr+data) or a usb-a to c cable you can cut the end off of| connects M5Dial to breakout board |
| M3x6 heat set inserts | connects to the standoffs |
| M3x10 standoffs | connects to the screws for the lid |
| M3x16 screws | for lid |

### Wiring

The GPS module connects to the M5Dial's Grove Port A:

| GPS | M5Dial Grove A |
|-----|----------------|
| TX  | G13 (ESP32 RX2) |
| RX  | G15 (ESP32 TX2) |
| VCC | 5 V |
| GND | GND |
You can swap the RX/TX pins if you update the code, you mainly just need to get the VCC and GND correct.  
I used a GY-NEO6MV1 but that's not really available anymore, there is a MV2 now which seems to be the same thing. But its really just a u-blox NEO-6M.  
The exact GPS module shouldn't matter much as most of them are UART and many of themn support 5v or 3v, but it is best if you grab one that claims to support 5v logic level.


## Setup

1. Copy `include/config.h` and fill in your WiFi credentials:

   ```cpp
   #define WIFI_SSID     "your-network"
   #define WIFI_PASSWORD "your-password"
   ```

2. Build and flash with PlatformIO:

   ```bash
   pio run --target upload
   ```
Or you can use PlatformIO in vscode to build and upload.

3. The device hostname is `esp32-ntpserv`. Point NTP clients at its IP address (shown on the display after connecting).

### Debug output

Uncomment `-D DEBUG` in `platformio.ini` to enable serial logging at 115200 baud:

```bash
pio device monitor
```

## HTTP API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/gps`   | GET    | GPS fix data as JSON |

Example response:

```json
{
  "lat": 51.500729,
  "lon": -0.124625,
  "alt_m": 12.3,
  "satellites": 8,
  "speed_knots": 0.01,
  "course_deg": 0.00,
  "fix": true
}
```

Returns `503` with `{"error":"no GPS fix"}` if no fix is available yet.

## NTP behaviour

| Time source | Stratum | Ref ID |
|-------------|---------|--------|
| GPS fix | 1 | `GPS` |
| RTC holdover (no GPS) | 2 | `RTC` |
| No source | 16 (unsynchronised) | — |

Time is interpolated between GPS fixes using `millis()`, so sub-second accuracy is maintained between the 10-second GPS update intervals.

## Enclosure

`box.scad` is an OpenSCAD model for a box+lid 3D-printed enclosure.
The bottom has an opening for a usb type-c breakout board so you can connect usb type-c cables to program and/or charge the battery. You really need to get one that has the 5.1k ohm resistors, then you should be able to use any usb type-c cable even emarker ones.  
Heat set inserts are placed in the 4 posts in the corners and then you attach the standoffs and then can put the screws through the lid into the standoffs.  
I didn't do an amazing job planning out the space, everything just barely fits here.
