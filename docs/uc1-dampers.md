# UC1 - free-cooling dampers (`FreeCoolingDampers.yaml`)

## Purpose

On this installation the Q is configured (in its own installer menu) to auto-boost to **fan level 3**
when outdoor air can cool the building. Fan level 3 pushes about 30 % more air than level 2 (on the
bench unit ~197 vs ~150 m3/h), and that air is routed through dedicated open-plan ducts that have
**Lindab UltraLink FTCU dampers** which must be open for it and shut otherwise. This firmware
converts "the Q is at fan level 3" into a 0-10 V signal for the dampers.

Triggering on the fan level (and not on the bypass state) is deliberate: it also protects the standard
ducts from the full flow when someone selects fan 3 manually, whatever the reason the Q is at level 3.

## Behaviour (as agreed with the system owner)

| Q state | Output (DAC channel 0) |
|---|---|
| fan level 3 (High) | **10 V** (dampers open) |
| fan level 0, 1 or 2 | **0 V** (closed) |
| level unknown (just booted, not yet reported) | 0 V |
| no CAN frames for 30 s (Q off, cable out, bus fault) | 0 V |

No hysteresis, no run-on, binary only. The dampers' own travel time (tens of seconds) smooths any short
flicker between 2 and 3, and the Q's own boost logic provides the run-on. **Closed is the safe default.**

## How it works

`uc1_fan3_damper_output.yml`:

- once a second reads `fan_level_raw` (CAN PDO 65: 0 away, 1 low, 2 medium, 3 high) and
  `bus_alive(30000 ms)` (any CAN frame seen in the last 30 s);
- sets the DAC channel on every change and **re-asserts it every 5 s** even if nothing changed
  (self-heals a glitched I2C write);
- at boot sets both DAC channels to 0 V (the DAC keeps its last value across an ESP32 reset because it
  stays powered, so this covers a reboot while the dampers were open);
- requests PDO 65 once a minute, and immediately when the CAN bus comes back after an outage.

## Entities (device web page / API)

- `Dampers Open` (binary sensor) and `Damper Control State` (text, e.g. "Closed - fan level 2",
  "Open - fan level 3", "Closed - no CAN bus activity", "Closed - waiting for fan level").
- Generated Q entities: fan level, operating mode, supply/exhaust flow, duty, humidity, etc.
- `Documentation` (diagnostic): the repository address.

## Status screen (AtomS3R)

QR code to this documentation for 6 s (and for 60 s whenever the screen is pressed; a long press switches WiFi service mode on or off), then: WiFi dot (grey = off, shows the IP and minutes left when on), CAN dot, fan level
and name, Q mode, supply flow, and a large box **green "CLOSED 0V" / amber "OPEN 10V"** showing what the
DAC is commanded to. A lit screen with a wrong or missing picture usually means the wrong panel file
([hardware doc](hardware-and-pitfalls.md)).

## Verified so far

- 2026-10-28: Q forced to fan 3 -> output command within 1 s, **10.004 V** metered; fan 2 -> **0.0034 V**.
  Flow began rising ~5 s later and settled in ~35 s; back at fan 2 the output closed in the same second.
- Running on a second AtomS3R with screen: values shown matched the Q (2026-10-28).
- The DAC re-connected on that second board was not metered afterwards (2026-10-31): meter it.

## To check on site

1. Dampers physically move (the bench had no dampers).
2. What the FTCU does when the DAC loses power while the FTCU (24 V AC, separate supply) stays live - do the
   unplug test in the pitfalls doc; a pull-down resistor from AIN to GND is the fix if the dampers do not close.
3. The Q is left in **Auto** after any manual test - in a manual mode its automatic free-cooling boost
   may not engage.
4. FTCU configured as described in the hardware doc (analogue input enabled, voltage range, angle control).
