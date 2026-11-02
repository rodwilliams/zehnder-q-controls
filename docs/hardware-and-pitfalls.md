# Hardware and pitfalls

## Bill of materials (per system)

| Part | UC1 dampers | UC2 humidity |
|---|---|---|
| Controller | M5Stack **AtomS3R** | M5Stack **AtomS3** (plain) |
| CAN interface | M5Stack **Mini CAN Unit** (TJA1051T/3) on the top Grove port | same |
| DAC | DFRobot Gravity **GP8413** (2-channel, 15-bit, 0-10 V), I2C address **0x59** | none |
| Loads | Lindab UltraLink **FTCU** damper controller(s), 0-10 V analogue input, **powered separately from 24 V AC** (not from this firmware's board or the CAN bus) | none |

The Mini CAN Unit takes **12 V from the Q's CAN connector on its HV pin** and supplies 5 V to the
Atom over the Grove cable, so the whole assembly is bus-powered.

## Which board is this

Both are ESP32-S3 with 8 MB flash, so the USB name does not tell you. Run:

```bash
.venv/bin/python -m esptool --port /dev/ttyACM0 flash_id
```

- `ESP32-S3-PICO-1` with **Embedded PSRAM 8MB** = **AtomS3R**
- `ESP32-S3 (QFN56)` with **no PSRAM** = plain **AtomS3**

(Do not rely on the label alone: confusing the two cost a day of display debugging.)

## Pin maps

| Function | Plain AtomS3 | AtomS3R |
|---|---|---|
| CAN (Mini CAN Unit, top Grove) | TX GPIO2, RX GPIO1 | TX GPIO2, RX GPIO1 |
| Screen SPI | MOSI 21, SCLK 17, DC 33, CS 15, RST 34 | MOSI 21, SCLK 15, DC 42, CS 14, RST 48 |
| Screen backlight | plain PWM, **GPIO16** | LP5562 LED driver at I2C 0x30, **SDA 45 / SCL 0** (use 50 kHz) |
| Screen panel | GC9107, 128x128 window at row offset 32 | GC9107 (older units) or ST7735S (built after ~May 2026) |
| DAC I2C (bottom pin header "G39 / G38 / 5V / G") | SDA 38, SCL 39 | SDA **38**, SCL **39** (UC3: GPIO5 / GPIO6, for the ToUnit Base) |

Screen pins and panel details come from M5Stack's own library (`M5GFX`, `board_M5AtomS3` and
`board_M5AtomS3R`), not from web summaries. M5GFX detects the panel controller at boot by reading its
ID, which ESPHome cannot do (the screen is write-only). The two AtomS3R panel types need different
files: `display_atoms3r_gc9107_local.yml` (used) and `display_atoms3r_st7735_local.yml`
(fallback, **never tested**). If a screen is lit but blank, garbled or colour-inverted, suspect the
panel type first. The plain AtomS3 GC9107 and the AtomS3R GC9107 variants were verified on the
actual units on 2026-10-28 (S3R) and 2026-10-31 (S3).

## Wiring

**CAN:** Mini CAN Unit to the Q's CAN port. Wire colours used here: red = 12 V (to HV), black = GND,
white = CAN_L, **yellow = CAN_H**. The unit has a **fixed 120 ohm terminator** (no jumper).

**DAC (UC1):** Grove cable from the Atom's bottom pins (G38 = SDA, G39 = SCL, 5 V, G) to the GP8413.
Channel 0 is the damper output.
**FTCU side:** DAC output to the FTCU **AIN**, DAC GND to the FTCU **system neutral**. On the FTCU
cables used here: **grey = GND, pink = AIN** - swapping them presents -x V and the input silently
ignores it (it looks exactly like "the damper does not sense the signal"). The FTCU must be configured
in the UltraLink app: analogue input enabled (Modbus register 4x071 = 1), voltage range selected
(4x500: 0 = 0-10 V, 2 = 2-10 V, the factory default unless ordered otherwise), and angle control for
simple open/closed (the factory input is a *flow* setpoint). Useful FTCU diagnostics: register 3x013
"Unit Status" (0 normal, 1 locating flow, 2 override, 3 error, 4 regulating, 5 calibrating) and
4x151 "Override" (forces max/min/open/closed, overriding the analogue input).

## Atomic ToUnit Base (M5Stack A161) - replacing the soldered DAC pins

The base turns the S3R's bottom pins into a Grove (HY2.0-4P) socket, so the DAC can plug straight in. Read from
M5Stack's schematic (2025-08-28, image on the product page) on 2026-11-02:

- **Only GPIO5, 6, 7, 8 are routed**, through an 8-way DIP switch (switches 1-4 connect G5, G6, G7, G8 to one
  signal wire, 5-8 connect the same four to the other). **GPIO38 and GPIO39 - where the soldered DAC cable goes
  today - are not connected** on the base (they carry no-connect marks). 5 V and GND reach the plug. Enable
  **each GPIO in only one group.** Our DAC cable has **yellow = SDA, white = SCL** (it was wired G38 = yellow,
  G39 = white). M5Stack's text says the base's plug has yellow = IO1, white = IO2, so for SDA = G5 and SCL = G6
  the expected setting is **switch 5 on** (G5 into the IO1 group) and **switch 2 on** (G6 into the IO2 group), all
  others off - to be confirmed on the real plug with the I2C scan.
- So the firmware's DAC pins must move to two of G5-G8: `dac_sda_pin` / `dac_scl_pin` (defaults G38/G39 in
  `dac_local.yml`; `FlowProportionalDampers.yaml` sets **G5 = SDA, G6 = SCL**).
- **Unsettled:** which plug wire is IO1 and which is IO2. M5Stack's text says yellow = IO1, white = IO2, but the
  schematic's connector pin order suggests the opposite. Settle it with a **live I2C scan** (expect 0x59 for the
  DAC); if nothing is found, swap the two substitution values (or the two DIP switches).
- **Verified with flying leads (2026-11-02):** SDA = GPIO5 (yellow lead), SCL = GPIO6 (white lead): the boot scan
  finds the DAC at 0x59 and UC3 writes to it without errors. The S3R's internal bus (SDA 45 / SCL 0) holds the
  LP5562 backlight chip (0x30) and the IMU (0x68), so G5/G6 are free. **The 5-pin bottom bank is, in order,
  3V3, G5, G6, G7, G8 - the first pad is 3V3, not G5.** Putting the leads one pad out (yellow on 3V3, white on
  G5) gave "no devices" in either firmware orientation.
- Size 30.2 x 24.0 x 13.1 mm, one M2 screw. The base's pins need the S3R's bottom pads free, so the soldered pins
  on the 4-pin bank have to be removed first.

## Power warning

**Never connect USB power to the Atom while the Mini CAN Unit is connected and powered from the
Q's 12 V** - two 5 V sources meet on the Grove cable. Flash over USB with the CAN cable unplugged.
A USB power bank was observed to give a reboot loop and poor WiFi; use a proper supply or the bus power.

## Mistakes already made (so you do not repeat them)

1. **I2C SDA/SCL swapped.** A vendor doc's "respectively" was read backwards; the DAC "did not
   respond" for days, a cable and a second DAC were ordered, and the real fix was swapping two numbers in
   software. **Run a live I2C scan before blaming wiring** (`i2c: scan: true`; expect the DAC at 0x59
   and the IMU at 0x68).
2. **A faulty Mini CAN Unit.** One unit read **94 ohm** across CAN H-L (a single healthy unit reads
   ~120 ohm) and raised an immediate CAN error on the Q as soon as CAN_H was connected. A second unit
   of the same model read 119.4 ohm and worked at once. Measure H-L unpowered before connecting to a
   live bus; if it is below ~110 ohm, do not use it. (Also: a lone ~120 ohm terminator is acceptable on
   this Q; a healthy bus reads ~60 ohm.)
3. **Wrong board assumed (AtomS3 vs AtomS3R).** All display work was first aimed at the wrong model.
   Identify the board with `flash_id` before anything else.
4. **Power-bank supply** produced reboot loops (above).
5. **Truncated `esphome config` output** was once mistaken for success - look for "Configuration is valid!".
6. **Debug logging over serial** floods with `'Led' Setting` lines when no API client is connected;
   read logs over the network API.
7. **FTCU wires swapped** (see Wiring).
8. **A multimeter that read 12.6 % high.** On 2026-11-01 the DAC appeared to output 11.25 V at its 10 V
   setting on two different DAC modules, linearly and with a slow upward creep, and the DAC's 5 V supply
   read 5.25 V. A second, trusted meter read the same outputs as 0.02 / 2.51 / 5.04 / 7.53 / 10.06 V for
   0 / 2.5 / 5 / 7.5 / 10 V commanded (within 0.6 %), so the DAC and firmware were correct and the first
   meter was wrong. **Before suspecting the DAC, check the meter against a known source** (a fresh AA cell
   reads about 1.55-1.6 V) or against a second meter. The bench test `dac_sweep_test.yaml` steps both channels
   through 0 / 2.5 / 5 / 7.5 / 10 V, 4 s each, with the commanded voltage on the AtomS3R screen, for exactly
   this kind of check. It replaces the UC1 firmware on the board it is flashed to.

## Not yet checked on site (UC1)

- **What the dampers do when the DAC loses power while the FTCU stays powered.** The FTCUs run from their
  own 24 V AC supply, independent of the ESP32. The DAC is powered from the CAN bus through the ESP32, so
  losing the CAN cable, the Q, or the ESP32 kills the DAC output while the FTCU is still live. If the FTCU's
  AIN then reads 0 V, the dampers close (the intended fail-safe). If AIN **floats** instead, they could drift
  or stay open. Test it: with the system running and the dampers open, unplug the ESP32 and watch them. If
  they do not close, a pull-down resistor from AIN to GND at the FTCU is the fix (about 10 kohm is a
  starting point - confirm against the FTCU's input impedance in the Lindab documentation).
  **Record these three cases** (use a *verified* meter - see pitfall 8), one row each, with the Q running at
  fan 2 so the commanded output is 0 V, and note the date, the FTCU model/size and its settings
  (4x071, 4x070, 4x500, and 4x510/4x511 if 2-10 V):

  | Case | How | Damper: closes / holds / drifts / opens | Voltage at the FTCU AIN terminal |
  |---|---|---|---|
  | Baseline | DAC powered, commanding 0 V | | |
  | Open circuit | Disconnect the AIN wire at the FTCU, DAC still powered | | |
  | DAC unpowered | Power off the ESP32 and DAC (the real fault case: lost CAN, dead ESP32) | | |

  Also look at the FTCU's "Unit Status" (Modbus 3x013, in the UltraLink app) to see whether it enters an
  override state. If the damper does not close in the open-circuit or unpowered case, fit the pull-down
  resistor described above and repeat the test.
  Background (Lindab FTCU manual, 2021-02-19): in the **2-10 V** range with flow regulation (4x500 = 2 and
  4x070 = 2, the factory defaults) the FTCU forces the damper **fully closed** when the input is below
  register 4x511 (default read as 0.8 V); in the 0-10 V range this override does not exist. What happens to a
  truly floating input is **not documented**, which is why it is tested.
- **Common reference.** The DAC's GND must be connected to the FTCU's signal ground (system neutral) so
  the 0-10 V is measured against the right reference; with 24 V AC supplies that share a neutral, confirm
  with a meter that the DAC GND and FTCU neutral are at the same potential before connecting the signal.
- That the dampers physically move on the damper side (only the output voltages have been metered:
  10.004 V at fan 3, 0.0034 V at fan 2).
