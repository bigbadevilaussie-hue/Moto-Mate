# Moto-Mate MkII - Veroboard Layout Guide

This document contains the ASCII block diagrams for the Moto-Mate MkII circuit.

## 1. Power Input & Switching
Takes raw 12V AGM, protects it, and creates the switched 12V and 5V rails.

```text
       [ 12V AGM Battery ]
               |
           (Kill Switch)
               |
           [ 3A Fuse ]
               |
          +----+----+
          |         |
      [ SA16CA ]  [ 1N4007 ] (Series Diode)
      (TVS Diode)    |
          |         |
         GND    [ 1000uF Cap ]
                    |
                    +-----------------+-----------------+
                    |                 |                 |
              [ MP1584EN ]      [ IRF4905 ]       [ Voltage Divider ]
             (Always-on Buck)     (P-Chan FET)      (470k / 100k)
                    |                 |                 |
                    |             (Switched 12V)    (To GPIO 1)
                    |                 |                 |
                    |          +------+------+          |
                    |          |             |          |
                    |      [ Mini 360 ]  [ 30mm Fan ]  |
                    |       (12V->5V)    (5V Output)   |
                    |          |             |          |
                    |      [ GPS Module ]    |          |
                    |      (NEO-6M)          |          |
                    |                        |          |
                 (5V Rail)                (GND)        |
                    |                                   |
               [ Schottky ]                             |
              (1N5819 Diode)                            |
                    |                                   |
                 (5V Pin)                            (GND)
                 (ESP32)
