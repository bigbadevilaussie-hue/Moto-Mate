# Moto-Mate MkII - Veroboard & Wiring Guide

## Netlist (Connections) - Corrected by Uncle Claude

### Power Input & Rail Generation
| Net | Connects To |
| :--- | :--- |
| BATT+ | 12V Terminal Block, Kill Switch (In) |
| 12V_FUSED | Kill Switch (Out), 3A Fuse (In) |
| 12V_PROT | 3A Fuse (Out), 1N5401 (Anode), 1000uF Cap (Positive), TVS (Cathode), MP1584EN IN+, IRF4905 Source, 470k Resistor (Top) |
| GND | 1N5401 (Cathode), 1000uF Cap (Negative), TVS (Anode), MP1584EN IN-, ESP32 GND, Terminal Block GND, 100k Resistor (Bottom), 10k Pull-downs, etc. |
| 5V_ALWAYS | MP1584EN OUT+, Schottky (Anode), Buzzer (via 2N2222), 470k/100k Divider (GND side) |
| 5V_ESP | Schottky (Cathode), ESP32 5V Pin |
| 12V_SWITCHED | IRF4905 Drain, Mini 360 IN+, USB Charger (+) |
| 5V_SWITCHED | Mini 360 OUT+, GPS VCC, 30mm Fan (+) |

### ESP32-S3 Connections
| GPIO | Function | Connects To |
| :--- | :--- | :--- |
| GPIO 1 | Voltage Divider (ADC) | Junction of 470k/100k divider (to GND) |
| GPIO 2 | Buzzer | 1k Resistor -> 2N2222 Base |
| GPIO 3 | Red LED | 330 ohm Resistor -> Red LED Anode |
| GPIO 4 | TFT Backlight | IRLZ44N Gate (Source to GND, 100k Pull-down to GND) |
| GPIO 5 | 12V MOSFET Control + Green LED | 1k Resistor -> 2N2222 Base, 330 ohm -> Green LED Anode |
| GPIO 6 | I2C SDA | SHT40 SDA, MPU-6050 SDA (No external pull-ups needed) |
| GPIO 7 | I2C SCL | SHT40 SCL, MPU-6050 SCL (No external pull-ups needed) |
| GPIO 8 | TFT Reset | ST7735 RESET |
| GPIO 9 | TFT DC | ST7735 A0/DC |
| GPIO 10 | TFT CS | ST7735 CS |
| GPIO 11 | TFT MOSI | ST7735 SDA/MOSI |
| GPIO 12 | TFT SCLK | ST7735 SCK |
| GPIO 13 | Wake Button | 16mm Button to GND (10k Pull-up to 3.3V) |
| GPIO 43 | GPS RX (ESP TX) | 1k Resistor -> GPS RX |
| GPIO 44 | GPS TX (ESP RX) | GPS TX |

### 12V Switched Line Control (IRF4905 + 2N2222)
| Component | Connection |
| :--- | :--- |
| IRF4905 Source | 12V_PROT |
| IRF4905 Drain | 12V_SWITCHED |
| IRF4905 Gate | 10k Pull-up to 12V_PROT, 2N2222 Collector, 15V Zener (Cathode) |
| 2N2222 Emitter | GND |
| 2N2222 Base | GPIO 5 (via 1k Resistor), 10k Pull-down to GND |
| 15V Zener Anode | IRF4905 Gate (Cathode to Source/12V_PROT) |

### Sensors & Display
| Component | Connections |
| :--- | :--- |
| SHT40 | VCC -> 3.3V, GND -> GND, SDA -> GPIO 6, SCL -> GPIO 7 |
| MPU-6050 | VCC -> 5V (Switched), GND -> GND, SDA -> GPIO 6, SCL -> GPIO 7 |
| ST7735 TFT | VCC -> 3.3V, GND -> GND, CS -> GPIO 10, RESET -> GPIO 8, DC -> GPIO 9, MOSI -> GPIO 11, SCK -> GPIO 12, LED -> IRLZ44N Drain |

---

## Bill of Materials (BOM) - Corrected

### Active / Modules
| Qty | Component | Purpose |
| :--- | :--- | :--- |
| 1 | ESP32-S3 Super Mini (HW-747) | The Brain |
| 1 | 1.8" 128x160 TFT LCD (ST7735S, SPI) | The Face |
| 1 | SHT40 Temp/Humidity Sensor | Ambient Temp |
| 1 | MPU-6050 6-Axis Accelerometer/Gyro | Lean Angle / Motion |
| 1 | NEO-6M GPS Module (with external SMA antenna) | Location |
| 1 | MP1584EN Buck Converter (12V -> 5V) | Always-On Power |
| 1 | Mini 360 Buck Converter (12V -> 5V) | Switched Power (GPS/Fan) |
| 1 | IRF4905 P-Channel MOSFET | 12V High-Side Switch |
| 2 | 2N2222 NPN Transistor | MOSFET Driver & Buzzer Driver (SFM-27) |
| 1 | IRLZ44N N-Channel MOSFET | TFT Backlight Switch |
| 1 | 30x30x10mm 5V Fan (0.15A) | Parts Cooler |
| 1 | 16mm Panel-Mount Momentary Push Button | Wake Button |
| 1 | 12V 20A Circular Rocker Switch (20mm) | Kill Switch |

### Passive Components
| Qty | Component | Value / Spec |
| :--- | :--- | :--- |
| 1 | Resistor | 470k (Voltage Divider Top) |
| 1 | Resistor | 100k (Voltage Divider Bottom) |
| 1 | Resistor | 10k (MOSFET Gate Pull-up) |
| 1 | Resistor | 10k (Wake Button Pull-up) |
| 2 | Resistor | 10k (2N2222 Base Pull-down, IRLZ44N Gate Pull-down) |
| 3 | Resistor | 1k (MOSFET Driver, Buzzer, GPS TX) |
| 2 | Resistor | 330 ohm (Red LED, Green LED) |
| 1 | Capacitor | 1000uF 25V Electrolytic (12V Bulk) |
| 1 | Capacitor | 1uF Ceramic (ADC Filter) |
| 2 | Capacitor | 0.1uF Ceramic (Decoupling) |
| 1 | Diode | SA16CA TVS (12V Protection) |
| 1 | Diode | 1N5401 (Series Reverse Polarity, 3A) |
| 1 | Diode | 1N5819 Schottky (USB Backfeed Protection) |
| 1 | Diode | 15V Zener (IRF4905 Gate Protection) |
| 1 | Fuse | 3A PTC Resettable |
| 1 | Crystal | 32.768kHz (Optional) |
| 2 | Capacitor | 15pF (Crystal Load) |
| 1 | Kit | 300pcs Ceramic Capacitor Kit (2pF - 0.1uF) |
| 1 | LED | 5mm Red (Fake Alarm) |
| 1 | LED | 5mm Green (12V Switched Indicator) |

### Connectors & Hardware
| Qty | Component | Purpose |
| :--- | :--- | :--- |
| 1 | 2-pin 5mm Screw Terminal | 12V Battery Input |
| 1 | 4-pin 5mm Screw Terminal | GPS (5V, GND, TX, RX) |
| 1 | 2-pin 5mm Screw Terminal | Fan (5V, GND) |
| 1 | 24x55 Vero Board | Main PCB |
| 4 | M3 Heat-set Inserts & Screws | Case Mounting (for tube/caps) |
| 1 | U.FL to SMA Pigtail | GPS Antenna Connection |
