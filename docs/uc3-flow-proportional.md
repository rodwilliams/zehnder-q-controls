# UC3 - flow-proportional dampers (`FlowProportionalDampers.yaml`)

## Purpose

Drive a 0-10 V damper signal that follows the Q's **airflow demand** continuously, instead of switching at
fan level 3 (UC1): **0 V at level 2 (the base rate), 10 V at level 3 (the top rate), linear in between.**
Below level 2 (Low, Away, level 0) the output is the "Below-Base Voltage", default **0 V** (the owner expects to
tune this once the system is running).

## The signal: PDO 226 "Fan Speed Modulated"

Measured on the bench unit (recordings of 2026-11-02, not committed):

| Q state | PDO 226 | Settled supply flow |
|---|---|---|
| Auto, Medium (level 2) | 200 | ~143-152 m3/h |
| Humidity modulation | 200 to 300 (continuous) | ~150 to ~205 m3/h |
| Manual High (level 3) | 300 | 205.3 m3/h |
| Boost | 300 | 205.4 m3/h |
| Manual Low | 100 | 92.7-94.9 m3/h |

- Over the whole 100-300 range the supply flow is a straight line of the demand:
  `flow = 41 + 0.55 x PDO 226` m3/h (residual under 1 m3/h). So the demand *is* "flow between level 2 and
  level 3" - no per-level flow rates are needed.
- Humidity modulation (two events recorded on 2026-11-02): at the onset (the same second Ventilation Mode becomes
  Overruling) the demand leaves 200 and goes to **300** within seconds - in the first event via 225 and 243
  (about 8 s), in the second straight to 281 then 300, so the jump size depends on how fast the humidity rises.
  It then decays **linearly**: about 10 counts a minute in the first event (still 233 seven minutes later) and
  about 14-18 a minute in the second. The Q runs on long after the humidity is back to normal. Do not rely on a
  fixed decay rate.
- The demand changes in the same second as the level; the **airflow lags by 5-30 s** (about 20 % after 10 s, 73 %
  after 20 s, 96 % after 30 s), so the signal leads the air.
- In manual modes the Q switches its humidity modulation off (Ventilation Mode = Disabled) and the value is
  simply 100 x level. Away (0) is expected to read 0 but was not recorded.
- The Q publishes the value when it changes; the firmware also asks for it every minute and whenever the CAN bus
  comes back.

## Behaviour

| Q demand | Output (DAC channel 0) |
|---|---|
| >= base (default 200) | 0 V at the base, rising linearly to **10 V at the top** (default 300), clamped at 10 V |
| < base (Low, Away, level 0) | **Below-Base Voltage** (default 0 V) |
| unknown: just booted, no demand received, **no CAN traffic for 30 s**, or top <= base | **0 V** |

The DAC is re-asserted every 5 s, both channels are set to 0 V at boot, and writes happen when the voltage moves
by 0.02 V or more. Note that a non-zero Below-Base Voltage makes the output **step** at the base (to 0 V),
because the line starts from 0 V there.

## Settings and entities

| Entity | Meaning |
|---|---|
| `Flow Base Value` (number, 0-400, default 200) | demand that gives 0 V |
| `Flow Top Value` (number, 0-400, default 300) | demand that gives 10 V |
| `Below-Base Voltage` (number, 0-10 V, default 0) | output while demand < base |
| `Damper Output Voltage` (sensor) | the commanded voltage |
| `Damper Control State` (text) | e.g. "3.3 V - demand 233 (200-300)", "0 V - no CAN bus activity" |
| `Fan Speed Modulated` (sensor) | the Q demand (PDO 226), unfiltered |

Settings are remembered across reboots. A changed default in the YAML does not override a remembered value: set
the number on the device as well.

## Status screen (AtomS3R)

QR code for 6 s (and for 60 s on a short press; a long press is the WiFi service window), then WiFi state, CAN,
fan level, the demand with its end points, supply flow, and the commanded voltage with a bar.

## Verified on the bench

- 2026-11-02, second humidity event with UC3 running on a board on the Q: the commanded voltage matched
  (demand - 200) / 10 to within one count at every sample, reaching 10.0 V three seconds after the onset (airflow
  needs about 30 s). A trusted meter on the open DAC output agreed with the displayed voltage to 0.06 V (the same
  +0.06 V seen at 10 V in the sweep test, i.e. DAC/meter tolerance).

## Not yet verified

- A live damper (the DAC output has so far been open): how an FTCU in flow mode behaves along 0-10 V (see the FTCU notes in
  `hardware-and-pitfalls.md`). In 0-10 V mode, 0 V is simply "minimum".
- Whether the demand behaves the same for other overrides (RF/analogue input presets, other timers).
- The output at Away (expected: demand 0).
