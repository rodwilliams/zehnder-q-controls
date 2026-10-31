# Zehnder Q - ComfoNet CAN notes

Reverse-engineered from live captures, mostly on a **Q450** (2025-05 to 2025-08) and checked on a
**Q600** (2026-10). Where something was not re-checked on the Q600 it says so. The upstream project and
[aiocomfoconnect's PROTOCOL-PDO.md](https://github.com/michaelarnauts/aiocomfoconnect/blob/master/docs/PROTOCOL-PDO.md)
cover the basics; this file records what we found beyond them.

## Bus basics

- CAN, **50 kbit/s**, extended (29-bit) identifiers.
- PDO frame id: `(pdo << 14) + 0x40 + node_id`. Heartbeat frames: id `0x10000000 + node`, no data, about every
  2 s. Point-to-point commands: ids `0x1f......`, sent here from the ESP32's own node id **0x2a**.
- Q = **node 1**, Option Box = **node 2**. If an Option Box that the Q knows stops sending its heartbeat
  (node 2, about once a second in captures), the Q raises a "check cables" fault after ~30 s and its
  analogue values read 0 V; the fault takes ~30 s to clear after the Option Box (or our impersonation) returns.

## Useful PDOs

| PDO | Meaning |
|---|---|
| 65 | Fan level: 0 Away, 1 Low, 2 Medium, 3 High |
| 49 | Operating mode: 1 Manual (limited), 2 PRESETRF, 3 PRESETANALOG, 4 PRESETRFANALOG, 5 Manual (permanent), 6 Boost, 7 Boost (RF), 8 Bathroom switch, 11 Away, 255 Auto |
| 225 | Ventilation mode: 0 Disabled, 1 Active, 2 Overruling |
| 117-122 | fan duty, flow, speed |
| 290-294 | humidity sensors |
| 369-372 | analogue input 1-4 as rebroadcast by the Q, 1 byte, x0.1 V |
| 513-516 | Option Box raw analogue inputs V1-V4, 16-bit little-endian, **volts = raw / 4065** |

Commands available from the component: set fan level, away, and `set_boost(duration_secs)` (a timed
boost to level 3 which the Q ends itself) - used by UC2.

## Behaviours worth knowing

- **Boost** (from our node): Operating Mode -> Boost, Fan Level -> High, same second; ends by the Q's own
  timer. During a boost Ventilation Mode is reported inconsistently (see the UC2 document).
- **Humidity response**: Ventilation Mode goes Active -> Overruling at the onset (rate of rise), flow
  climbs to about the fan-3 flow and decays slowly afterwards, with the fan level staying Medium.
- Analogue inputs: the Q slew-limits the Option Box value (369-372 ramp over ~8-13 s) while the raw
  PDOs 513-516 change at once.

## Option Box impersonation (paused)

The ESP32 can act as node 2: join sequence (RTR requests for PDO 17, 385, 401, then an announcement
frame `04 01 00 00` on `0x10000002`), then a 2 s heartbeat and PDO 513-516. **Confirmed on the Q450
(2026-08-07):** the Q accepted it with the real Option Box removed, the fault cleared in ~30 s, and
volts = raw / 4065 was calibrated using the Q's own rebroadcast (three points, residuals <= 0.03 V).
Not yet tested on the Q600; whether V1-V4 must be enabled as an active control input in the installer
menu before they change fan speed has not been tested. Code: `start_impersonation` /
`set_analog_input_volts` in `components/zehnder_comfoair_q/zehnder_comfoair_q.cpp`,
exposed by `impersonation_local.yml` (development project only).

## Other

- The component also provides `bus_alive(timeout_ms)` / `note_frame_received()` (any frame resets the
  timer) for the control projects to detect a dead bus.
- ESPHome's `esp32_can` is hard-wired to normal (transmitting) mode, so there is no listen-only mode
  without patching it. Mind the adapter: see the faulty Mini CAN Unit in the hardware doc.
