# Moto-Mate MkII - Mandatory Code Requirements

These are the non-negotiable software rules discovered during the hardware design review.

## 1. Deep Sleep & Power (Prime Directive)
*   **GPIO Hold:** Before calling `esp_deep_sleep_start()`, use `gpio_hold_en()` and `gpio_deep_sleep_hold_en()` on:
    *   GPIO 5 (12V MOSFET Control) - Lock LOW to keep MOSFET off.
    *   GPIO 4 (TFT Backlight) - Lock LOW to keep screen dark.
    *   GPIO 2 (Buzzer) - Lock LOW to keep it quiet.
    *   GPIO 43 (GPS TX / ESP RX) - Lock LOW to prevent back-powering the GPS.
*   **MPU-6050 Sleep:** The MPU is on the always-on 3.3V rail. Immediately after boot (and every time you wake), write to the `PWR_MGMT_1` register (0x6B) and set the SLEEP bit (0x40). This drops its current to ~10µA.
*   **TFT Sleep:** Send the ST7735 sleep command before going to deep sleep.

## 2. TFT Backlight (No MOSFET)
*   The BLK pin on the 1.8" ST7735 is **active-high**.
*   Drive **GPIO 4 HIGH** to turn the backlight ON.
*   Drive **GPIO 4 LOW** to turn the backlight OFF.
*   Do not use the IRLZ44N MOSFET for the backlight.

## 3. GPS (NEO-6M)
*   **Serial:** Use `Serial1.begin(9600, SERIAL_8N1, 44, 43)`. 
*   **Pins:** GPS TX goes to GPIO 44. GPIO 43 is not connected to the GPS RX (we only receive data).
*   **Data:** Parse NMEA sentences. Log an explicit "no fix" state rather than 0,0 coordinates.

## 4. I2C Bus
*   **SDA:** GPIO 6
*   **SCL:** GPIO 7
*   **Devices:** SHT40 and MPU-6050.
*   **Pull-ups:** Do not add external pull-ups unless necessary. The breakout boards usually have them. 

## 5. Button Logic (GPIO 13)
*   **State Machine:** 300ms debounce, 500ms click window, 2-second long-press.
*   **Single Click:** "Rider Ready" mode (30s countdown).
*   **Double Click:** Battery Voltage Check (5s display).
*   **Triple Click:** Maintenance Mode (Wi-Fi Max Drain).

## 6. Voltage Divider (GPIO 1)
*   **ADC Filter:** C2 must be **1µF** (not 100µF).
*   **Readings:** Use `analogReadMilliVolts()` and oversample.
*   **Hysteresis:** Enter Riding Mode at >13.2V for 5s. Exit at <13.0V for 30s OR no motion for 5m.

## 7. Radio Coexistence
*   **BLE and Wi-Fi:** Never run both at once. 
*   **Transition:** Kill BLE, wait 500ms, then bring up Wi-Fi.

## 8. Logging (LittleFS)
*   **Append Mode:** Open log files in append mode to preserve data across cold boots.
*   **4 AM Routine:** Seal the current wake log, gzip it, send via BLE to OSA, delete the local file, and open a fresh one.
*   **Ride Logs:** Send via BLE and delete immediately upon ACK.

## 9. Reverse Polarity
*   **Hardware:** Add a series 3A Schottky diode (or P-MOSFET ideal diode) before the fuse.
*   **Calibration:** Calibrate the engine-running voltage threshold against the voltage *after* the diode drop.
