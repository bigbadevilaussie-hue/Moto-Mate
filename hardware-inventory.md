# Moto-Mate — Ordered Hardware and Enclosure Notes

Updated 2026-11-25. Owner-supplied order history, display specification image and physical stripboard confirmation. This is a procurement/reference inventory, not a final BOM or evidence that every item has arrived, meets its advertised rating or is installed. Product links identify ordered variants; listing claims are not independently verified. Private order IDs and account links are intentionally omitted. Quantities below are purchased quantities, not build requirements.

## Electronics

| Ordered part | Quantity | Intended / possible use |
| --- | ---: | --- |
| [ESP32-S3 Super Mini](https://www.aliexpress.com/item/1005007785809540.html) | 2 | MCU; verify actual flash, PSRAM and chip variant |
| [ST7735S 1.8-inch 128 x 160 TFT](https://www.aliexpress.com/item/1005005806627331.html) | 2 | Colour display, 3.3 V module |
| SHT40 HZWDONE module | 2 | Temperature/humidity |
| [MPU6050 module](https://www.aliexpress.com/item/1005010008819484.html) | 2 | Motion sensing, possible future lean graphics |
| [GY-GPS6MV2 NEO-6M with antenna](https://www.aliexpress.com/item/599500680.html) | 1 | GPS |
| [INA226 I2C current/power monitor](https://www.aliexpress.com/item/1005009890292157.html) | 2 | Optional own-consumption measurement; not in current schematic |
| [A3144 TO-92 Hall sensor](https://www.aliexpress.com/item/1005013034988041.html) | 10 | Optional rotation sensing; not in current schematic |
| [IRF4905PBF P-channel MOSFET](https://www.aliexpress.com/item/1005007078037006.html) | 10 | Switched high-side rail |
| [PN2222A TO-92 NPN](https://www.aliexpress.com/item/1005007022929855.html) | 20 | MOSFET/buzzer driver candidates; verify actual pinout |
| [1N5819 Schottky diode](https://www.aliexpress.com/item/1005002872292563.html) | 50 | D6 |
| [1N5822 Schottky diode](https://www.aliexpress.com/item/1005005707619946.html) | 20 | Candidate D5 replacement; not approved |
| [0.5 W DO-35 Zener kit, 14 values, 3.3–30 V](https://www.aliexpress.com/item/1005013110872522.html) | 140 | Gate clamp candidate; confirm 15 V included |
| [25 V 1000 uF electrolytic](https://www.aliexpress.com/item/32829549030.html) | 10 | C1 candidate; transient voltage margin open |
| Electrolytic assortment, 1–470 uF, 16–50 V | 225 | C6 and other filter candidates; check values/ratings |
| [Ceramic assortment, 2 pF–0.1 uF](https://www.aliexpress.com/item/1005007851933584.html) | 300 | C2/C5 100 nF, optional C3/C4 15 pF; verify actual contents/ratings |
| [10 uH radial inductor, 8 x 10 mm](https://www.aliexpress.com/item/1005005686937276.html) | 5 | L1 GPS filter; current rating unknown |
| [JK30 3 A 30 V resettable PTC](https://www.aliexpress.com/item/1005005699246438.html) | 5 | F1; verify hold/trip ratings and temperature derating |
| [32.768 kHz cylinder crystal, 3 x 8 mm](https://www.aliexpress.com/item/1005007552196498.html) | 10 | Optional RTC crystal; omission discussed, not locked |
| [QC3.0/QC2.0 single-channel USB buck module, advertised 6–32 V input](https://www.aliexpress.com/item/1005006176726264.html) | 2 | Switched USB; differs from drawn dual USB-A module |
| [12 V round two-pin rocker switch](https://www.aliexpress.com/item/1005010153247795.html) | 4 | Kill switch; verify DC current rating and cutout |
| 12 mm momentary SPST push button, assorted colours | 6 | Wake button; confirm normally open and mounting dimensions |
| 3010 30 x 30 x 10 mm 5 V two-wire brushless fan | 2 | Enclosure ventilation |

Other existing parts may already be on hand; this table is not a missing-parts list. No purchase is inferred for items not shown in the owner-provided orders.

## Mechanical and workshop supplies

| Ordered item | Quantity / specification |
| --- | --- |
| [Single-sided uncut stripboard](https://www.aliexpress.com/item/1005001908786259.html) | 5 boards, 65 x 145 mm, **owner confirms 24 x 56 holes** |
| [Stainless washer assortment](https://www.aliexpress.com/item/1005010396222213.html) | 180, M2–M10 assortment |
| [M2.5 x 8 mm stainless 304 flat-head hex socket self-tapping screws](https://www.aliexpress.com/item/4001317709890.html) | 50 |
| Rosin paste flux SFD H05 | 30 g |
| Bomurphy 5022 wire stripper | 24–14 AWG |
| Mechanic TY-V866 rosin-core solder | 40 g, 0.4 mm, advertised 183 C |

The board hole count exactly matches the preliminary editor design (14 jumper wires / 30 cuts / 35 nets). Verify usable board edges, thickness, copper track orientation and mounting fit before cutting. The case model includes a USB corner notch in its assembly board preview; reconcile with actual stripboard routing before cutting.

## TFT module and V4 case

Owner-supplied display specification: ST7735S, 128 x 160 portrait (160 x 128 landscape), RGB565 / 65,536 colours, SPI, active area 35.04 x 28.03 mm, module 56 x 35 x 3.45 mm, advertised operating range -20 to +70 C. Actual physical module dimensions, mounting-hole pitch and backlight current still require measurement.

Git stores the editable [motomate_case.scad](motomate_case.scad) V4 source, not exported STL files. Three printable components: main tube with rails/columns/intake slots, front cap (display, LEDs, wake button, USB and SHT40 louvres), rear cap (rocker, SAE cable, buzzer and fan). OpenSCAD part selector: assembly, tube, cap_front, cap_back, print_tube, print_caps. No physical print or render verification reported here.

Source-derived dimensions: 148.8 mm wide x 39.4 mm high x 110 mm tube depth; **114.8 mm overall depth including both 2.4 mm cap flanges**. Model board 145 x 65 x 1 mm; TFT allowance 56.6 x 35.6 x 4 mm; window 35.6 x 28.6 mm; TFT screw pitch estimated 52 x 31 mm. Verify with calipers.

- Wake button: current btn_d = 16, btn_cb_d = 22; purchased nominal 12 mm button. Owner intends parametric adjustment, not redesign. Measure mounting diameter, body, nut and skin clearance before changing both values. No SCAD change yet.
- LED review: current cap calls hole_y(led_x, led_dz, ...) and hole_y(led_x, -led_dz, ...) with led_dz = 8 and case Z starting at 0. Negative-Z hole appears outside the panel. Verify render and intended centre-relative positioning before printing; no patch applied.
- USB: model assumes one 38 x 19 mm module and one front port; schematic describes dual USB-A; orders show two single-port modules. Confirm actual module sizes, port count, board notch and current budget before changing layout.
- Case screw columns are configured for M3 heat-set inserts (pilot_d = 4.0, screw_d = 3.4); ordered M2.5 self-tappers are not automatically compatible with those holes. TFT post pilots are 2.1 mm. Confirm screws/insert plan, counterbore and head clearance.
- Confirm rocker 20.2 mm cutout, fan mount, buzzer ear spacing, SAE cable strain relief, front/rear clearances and material/thermal suitability before print. Existing SCAD comments recommend ASA.

## Open electrical review

The current schematic remains preliminary. In particular: D5 1N4007 versus 3 A PTC and USB load; evaluate 1N5822 as a candidate, not automatic substitution. Confirm fuse placement, PTC hold/trip temperature behaviour, diode reverse voltage/current/thermal margin, TVS clamp and 25 V C1 transient margin against buck ratings, L1 current rating, actual PN2222A pinout, GPS back-power, I2C pull-ups, TFT BLK current, ADC filter and sleep/wake consumption. INA226/A3144 are optional stock only; do not add them to the circuit without owner direction and battery-budget review. Final BOM, PCB cuts, case fit and hardware power-up remain subject to owner approval.

See [README.md](README.md) for display build evidence and [aihandover.md](aihandover.md) for project direction. No circuit, GPIO, firmware or enclosure changes are made by this inventory.
