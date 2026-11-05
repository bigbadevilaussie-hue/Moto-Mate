# Bike Mate MkII - Project Spec (Moto-Mate)

## Core Design Philosophy
* Rule #1: If it's not fun or overengineered, I don't want it.
* Rule #12: Steal with pride. (Learn from it, bend it to your will).

## 🚨 BOARD TRAP WARNING (HW-747 Super Mini)
The exact board being used (ESP32-S3 Super Mini HW-747) has a **deceptive pinout**. 
*   **Only 15 pins are on the outer edge headers** (GPIO 1-13, TX/RX).
*   **The rest are on tiny underside pads** (GPIO 14-18, 21). 
*   **GPIO 19/20 are not broken out at all** (USB D+/D-).
*   **The 5V pin is directly connected to USB VBUS.** (No diode). You must unplug the buck or add a Schottky diode when flashing via USB to prevent backfeeding your Mac.
*   **Onboard RGB LED (GPIO 48):** Likely a WS2812. It can draw 1mA just sitting there. **Must be physically desoldered or cut off.**

## Development Environment
*   OS: iMac (2014 5K) running macOS Sonoma.
*   Toolchain: Arduino IDE / CLI.
*   Core: ESP32 Arduino Core v2.0.17 ONLY (Do NOT update to 3.x. Do NOT suggest NimBLE).
*   **OTA Only:** Board will be flashed via USB once, then sealed. OTA over Wi-Fi is the only update path.

## Power & Electrical Specs
*   Power Source: 12V AGM Motorbike Battery.
*   Connection: Permanently connected (Kill switch is the only physical disconnect).
*   Sleep Constraint: Target sleep current <= 0.001 Amps (1mA).
*   Input Protection: Must survive engine cranking (TVS diode, series diode, LC filter, 1000uF cap).
*   **Buck Converters:** MP1584EN for always-on rail (measure quiescent!). Mini 360 (5x available) for switched 12V -> 5V rail (GPS).
*   12V Switched Line: Clamped OFF by default (IRF4905). Does NOT wake the ESP32. Only activates when engine is running (Voltage > 13.2V).
*   Always-On 12V: Powers the Buck Converter (MP1584EN), Buzzer, and Voltage Sensor.

## Operational Logic & Wake Cycles
*   Primary Wake (Parked): Internal RTC timer wakes the ESP32 every 5 minutes. *(Optional: External crystal for better accuracy, fallback to internal RC).*
*   10-Second Telemetry Window: Boots instantly, connects BLE, sends telemetry, appends log, sleeps.
*   **Engine Run Detection (Hysteresis):** Enter Riding Mode at >13.2V for 5 seconds. Exit at V < 13.0V for 30 seconds OR no motion for 5 minutes (MPU-6050).
*   Riding Mode: If Engine Running, stay awake continuously. Power GPS, log data, skip the 5-min cycle.
*   GPS Logic: Only powered when engine is running via Mini 360 (5V). Cold start delay is acceptable. Log explicit "no fix" rather than 0,0.
*   **GPS Serial:** Use `Serial1.begin(9600, SERIAL_8N1, 44, 43)` and enable USB CDC on boot.

## Button Logic (Wake Button on GPIO 13)
*   Single Click: "Rider Ready" mode. 30-second countdown UI.
*   Double Click: Battery Voltage Check (5-second display).
*   Triple Click: Full Maintenance Mode (Wi-Fi Max Drain).
*   State Machine Requirement: 300ms debounce, 500ms click window, 2-second long-press. External 10k pull-up required.

## Memory & Logging
*   Variables: Use RTC_DATA_ATTR for variables that must survive deep sleep.
*   Log Files: Use LittleFS. Open in Append mode to preserve data across cold boots.
*   **FS Requirement:** Strategy for file corruption recovery, log rotation, and FS full behavior required. (Buffer in RAM during Riding Mode, flush every 10-30s).

## Final Pinout (ESP32-S3 Super Mini) - EDGE PINS ONLY
*   **GPIO 1:** Voltage Divider (ADC)
*   **GPIO 2:** Buzzer (via 2N2222 NPN)
*   **GPIO 3:** Red LED (Fake Alarm / Awake status) 
*   **GPIO 4:** TFT Backlight (via IRLZ44N MOSFET)
*   **GPIO 5:** 12V MOSFET Control **+ Green LED** (Green LED wired directly to this pin; lights up when 12V switched line is ON)
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

## Hardware Protection Updates (From AI Review)
*   **Voltage Divider:** 470k / 100k with a 1µF capacitor to GND.
*   **IRF4905 Protection:** 15V Zener diode across Gate and Source.
*   **GPS TX Protection:** 1kΩ series resistor on the ESP32 TX line.
*   **I2C Pull-ups:** External 4.7kΩ pull-ups to 3.3V.
*   **Firmware Sleep:** Use `gpio_hold_en()` and `gpio_deep_sleep_hold_en()` on GPIO 5, 4, 2, and GPS TX.
*   **Boot:** Check `esp_reset_reason()` and treat brownout as a logged event.
*   **Buck:** If MP1584EN quiescent > 0.5mA, swap to TPS560430 or LMR36006 (36V+).

## Case Constraints
*   Printer: Bambu Lab A1 Mini (180x180x180mm).
*   Max dimension: 175mm. Material: PETG-BASIC.
*   Board Size: 24x55 stripboard (~61mm x 140mm).
*   **Mechanical Requirements:** Need vibration mounting and splash resistance (grommets/strain relief). Add a 30mm parts cooler fan (only on during Riding Mode).

## Parts List (Confirmed)
1. Vero board 24x55 holes (plus 5 Pcs 6.5x14.5cm Stripboard)
2. 32.768kHz Crystal Oscillator 3x8mm (Optional)
3. ESP32-S3 Super Mini (HW-747) Board (x2, one for backup)
4. Load capacitors 2x 10-22pF (Optional)
5. 12V Input Protection (TVS/Caps)
6. 1.8" 128x160 TFT LCD (ST7735S, SPI)
7. IRLZ44N MOSFET (for TFT Backlight)
8. 2N2222 NPN Transistor (for Buzzer)
9. SHT40 Temp/Humidity Sensor (I2C)
10. Voltage Divider Resistors (470k & 100k)
11. MPU-6050 6-Axis Accelerometer/Gyro Module
12. 1x Red LED & 1x Green LED
13. Resettable Fuse 30V PTC Polymer (3A, 5PCS)
14. 4Pcs 12V DC Circular Rocker Switches
15. 15V Zener Diode (for IRF4905 gate protection)
16. 6x6mm Tactile Push Button (for GPIO 13)
17. 5x Mini 360 Buck Converters (using 1 for GPS)
18. 30mm 5V Fan (Parts Cooler)
19. 1x IRF4905 P-Channel MOSFET

## MkII Hardware Upgrades (To Order)
*   U.FL to SMA pigtail + External GPS Antenna
*   Screw Terminal Blocks (5mm pitch, 2-pin and 3-pin)
*   M3 Nylon Standoffs and screws
*   Female Header Strips (2.54mm pitch)
*   **Schottky Diode** (for 5V USB backfeed protection)

---

## AI Design Review Highlights

### Uncle Claude Highlights
*   **The Board Trap:** GPIO 14-18 and 21 are on underside pads, 19/20 don't exist. 5V backfeeds USB. GPIO 48 RGB LED will eat your sleep budget.
*   **AGM Surface Charge:** The 12.8V exit threshold will strand you in Riding Mode. Use V < 13.0V for 30s OR no motion for 5 mins.
*   **TPS62133 is wrong:** Its input max is 17V. You need a 36V+ buck like TPS560430 or LMR36006.
*   **GPS Power:** The NEO-6M needs 3.3V/5V, not 12V. You need a regulator on the switched line.
*   **Logging:** Write with a CRC, rotate at 64KB, buffer in RAM during Riding Mode.
*   **Crystal is Optional:** Internal RC is ±5% (15 seconds drift on 5 mins). Good enough for parked telemetry. Skip if soldering is risky.

### Auntie GPT Highlights
*   **UART Conflict:** *(Resolved)* GPIO 43/44 are fine because USB CDC handles serial.
*   **GPIO 3 Strapping:** Move the ADC to a non-strapping pin. *(Done: ADC is now GPIO 1)*
*   **Button Timing:** The click logic needs a state machine.
*   **Engine Detection Hysteresis:** A single threshold is not enough.
*   **Sleep Budget:** The 1mA target is not yet demonstrated. Measure it.

### Brother Grok Highlights (Brutal Mode)
*   **On the Core Philosophy:** *"It is not a design constraint. It is an excuse to keep adding complexity until the 1mA sleep target becomes theatre."*
*   **On the 1mA Target:** *"Until you have a real number with every sensor... the 1mA target is aspirational fan fiction."*
*   **On Adding Features:** *"Stop adding features. Make the existing ones reliable and measurable."*
*   **On the MPU-6050:** *"Overkill if you only need basic motion detection; it also draws power and adds I2C traffic. Justify it or drop it."* (Note: It now has a job as the motion-vote for exiting Riding Mode).
*   **On the Crystal:** *"Soldering quality on those underside pads + desoldering the RGB LED are the two things that will decide whether this survives on a bike."*

### Cousin Frenchie Highlights
*   **Fire Risk:** The real fire risk isn't the ESP32, it's the wiring. Stripboard on a motorcycle = vibrations = arcing.
*   **Battery Death:** At 5mA, the battery dies in ~1 month. At 1mA, it survives weeks.
*   **Anti-Vibration:** Grommets and strain relief are not optional, they are safety line #1.
