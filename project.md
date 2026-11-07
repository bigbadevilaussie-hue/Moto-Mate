# Moto-Mate MkII - Project Spec

## Core Design Philosophy
* Rule #1: If it's not fun or overengineered, I don't want it.
* Rule #12: Steal with pride. (Learn from it, bend it to your will).
* **Clean Slate:** MkI was a 2 AM patchwork of ticking bombs. MkII is a clean architecture built from scratch.

## 🚨 BOARD TRAP WARNING (HW-747 Super Mini)
*   **Only 15 pins are on the outer edge headers** (GPIO 1-13, TX/RX).
*   **The rest are on tiny underside pads** (GPIO 14-18, 21). 
*   **GPIO 19/20 are not broken out** (USB D+/D-).
*   **5V pin is directly connected to USB VBUS.** (Add a Schottky diode or unplug the buck when flashing).
*   **Onboard RGB LED (GPIO 48):** Must be desoldered to hit the 1mA sleep budget.

## Development Environment
*   OS: iMac (2014 5K) running macOS Sonoma.
*   Toolchain: Arduino IDE / CLI (Core v2.0.17 ONLY).
*   **OTA Only:** Board will be flashed via USB once, then sealed.
*   **Library Footprint:** All required libraries consume ~68KB Flash (4%) and ~1.2KB RAM. Total baseline compile: 300KB (15% of 1.9MB).

## Radio & Wi-Fi (Lessons from C3)
*   **No Coexistence:** BLE and Wi-Fi will never run simultaneously. 
*   **Transitions:** When entering Maintenance Mode (Wi-Fi), kill BLE first, wait 500ms, then bring up Wi-Fi.

## Power & Electrical Specs
*   Power Source: 12V AGM Motorbike Battery.
*   Sleep Constraint: Target sleep current <= 0.001 Amps (1mA).
*   **Buck Converters:** MP1584EN for always-on rail. Mini 360 for switched 12V -> 5V (GPS).
*   12V Switched Line: Clamped OFF by default (IRF4905).
*   **GPS Power:** 5V from switched buck, filtered through a 10uH inductor (L1) and 100nF + 470uF caps (C5, C6).

## Operational Logic & Wake Cycles
*   Primary Wake (Parked): RTC timer every 5 minutes.
*   **Engine Run Detection:** Enter Riding Mode at >13.2V for 5s. Exit at <13.0V for 30s OR no motion for 5m.
*   Riding Mode: Stay awake continuously. Power GPS, log data.
*   GPS Serial: `Serial1.begin(9600, SERIAL_8N1, 44, 43)` (UART1). UART0 is used for USB CDC debugging and is not connected to the GPS.

## Button Logic (Wake Button on GPIO 13)
*   Single Click: "Rider Ready" mode. Double Click: Battery Check. Triple Click: Maintenance Mode.
*   State Machine: 300ms debounce, 500ms click window, 2s long-press.

## Storage Strategy (LittleFS)
*   **Wake Log:** `wakes_YYYY-MM-DD.csv` (appended every 5 mins).
*   **Ride Log:** `ride_*.csv` (created during Riding Mode).
*   **4 AM Routine:** ESP32 wakes, connects to OSA via BLE, seals current file, gzips it, uploads it, deletes local copy, and opens a fresh file.
*   **Fail-Safe:** Keep max 7 days of un-uploaded logs. Delete oldest if LittleFS is full.

## Final Pinout (ESP32-S3 Super Mini) - EDGE PINS ONLY
*   **GPIO 1:** Voltage Divider (ADC)
*   **GPIO 2:** Buzzer (via 2N2222)
*   **GPIO 3:** Red LED (Fake Alarm) 
*   **GPIO 4:** TFT Backlight (Direct Drive, active-high)
*   **GPIO 5:** 12V MOSFET Control **+ Green LED**
*   **GPIO 6:** I2C SDA
*   **GPIO 7:** I2C SCL
*   **GPIO 8:** TFT Reset
*   **GPIO 9:** TFT DC
*   **GPIO 10:** TFT CS
*   **GPIO 11:** TFT MOSI
*   **GPIO 12:** TFT SCLK
*   **GPIO 13:** Wake Button
*   **GPIO 43:** GPS TX (ESP RX)
*   **GPIO 44:** GPS RX (ESP TX)
*   **GPIO 15/16 (Underside pads):** RTC Crystal (Optional).

## Hardware Protection Updates
*   **Voltage Divider:** 470k / 100k with 1µF capacitor to GND.
*   **IRF4905 Protection:** 15V Zener diode across Gate and Source.
*   **GPS TX Protection:** 1kΩ series resistor.
*   **I2C Pull-ups:** External 4.7kΩ to 3.3V.
*   **Firmware Sleep:** Use `gpio_hold_en()` on GPIO 5, 4, 2, and GPS TX.

## Case Constraints
*   Printer: Bambu Lab A1 Mini. Material: PETG-BASIC.
*   Board Size: 24x55 stripboard (~61mm x 140mm).
*   **Case Design:** U-shape tube with removable front/back caps (M3 screws). Prints standing on its open end. Front faceplate has a "verandah" for the screen, holes for LEDs, wake button, and SHT40 vents.

## Parts List (Confirmed)
1. Vero board 24x55 holes
2. 32.768kHz Crystal Oscillator (Optional)
3. ESP32-S3 Super Mini (HW-747) x2
4. 1.8" 128x160 TFT LCD (ST7735S, SPI)
5. IRLZ44N MOSFET (TFT Backlight)
6. 2N2222 NPN Transistor (Buzzer)
7. SHT40 Temp/Humidity Sensor (I2C)
8. MPU-6050 6-Axis Accelerometer/Gyro Module (I2C, powered from 3.3V, sleep mode enabled)
9. 1x Red LED & 1x Green LED (5mm)
10. 1x IRF4905 P-Channel MOSFET
11. Resettable Fuse 30V PTC Polymer (3A)
12. 15V Zener Diode
13. **16mm Panel-Mount Momentary Push Button (Red)** - Wake Button
14. 5x Mini 360 Buck Converters
15. 30x30x10mm 5V Fan (0.15A, Parts Cooler)
16. **KF301-5.0 Screw Terminal Blocks (2-pin & 3-pin)**
17. **M3 Screws (Countersunk) & Standoffs**

## MkII Hardware Upgrades (To Order)
*   U.FL to SMA pigtail + External GPS Antenna
*   Schottky Diode

---

## AI Design Review Highlights
*(The full quotes are in the MkI repo, but the key takeaways are:)*
*   **Claude:** Move MOSFET to GPIO 17 (avoid USB D+). Use V < 13.0V for riding exit. Crystal is optional.
*   **GPT:** Button needs a state machine. Hysteresis is mandatory. Measure the 1mA target.
*   **Grok:** Stop adding features. Measure the sleep current. Desolder the RGB LED.
*   **Frenchie:** The real fire risk is the vibration. Use grommets and strain relief.
