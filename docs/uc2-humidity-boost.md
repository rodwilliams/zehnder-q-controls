# UC2 - humidity boost (`HumidityBoost.yaml`)

## Purpose

This Q is configured (installer menu) to speed up when humidity rises. The owner wants that response
to be a full **fan level 3** for a fixed period whenever the Q starts reacting to humidity, then to let
the unit carry on in Auto. If the Q is still reacting when the boost ends, boost again.

There are no dampers on this system: only the ESP32, the CAN adapter and the screen.

## What the Q does (evidence, kettle steam tests 2026-10-28, bench unit, no ducts)

- The Q's humidity response is **not visible in the fan level or operating mode** (both stay Medium / Auto
  throughout). The clean marker is **Ventilation Mode (PDO 225) = Overruling**, which appeared about
  12 s after the humidity peak (extract air RH ~53-54 %, exhaust RH ~58-60 % in these tests).
- Our boost command is accepted from this node: Operating Mode -> **Boost**, Fan Level -> **High** in the
  same second; supply flow rose ~150 -> ~196 m3/h, settling in ~30 s.
- The **Q's own timer** ends the boost (Boost -> Auto, High -> Medium), so a lost ESP32 cannot leave the
  unit stuck in a boost.
- During a boost the Ventilation Mode reading is **unreliable** (it showed Disabled in two boosts but stayed
  Overruling throughout a third). The logic therefore only reads it when the Q is back in Auto.
- **Known limit:** the Overruling flag catches the *onset* of a humidity event. After a boost the Q was
  seen ramping its own flow back up (~167 -> ~196 m3/h) with Ventilation Mode = **Active**, Fan Level Medium,
  Mode Auto, and the flow then decayed slowly for 8+ minutes after the humidity was back to normal. That
  later phase is **not** detected or boosted by this firmware. (A trigger combining supply flow and a rise in humidity was designed for this and then deliberately
  removed again at the owner's request, so it is not in the firmware.)
- Thresholds measured are for one bench unit and one test setup; a real installation may differ.

## Behaviour

States of the 1-second loop in `uc2_humidity_boost_local.yml`:

1. **Watching.** If the master switch is on, the CAN bus is alive (a frame within 30 s), the Q is in
   **Auto**, the fan level is **below 3**, and Ventilation Mode = **Overruling** -> send a timed boost
   of "Humidity Boost Duration" seconds (default 300). Also logs the supply flow and boost count.
2. **Boost running.** Wait for the Q to enter Boost and later leave it (the Q's own timer ends it).
   Timeout at duration + 30 s. If the Q is **not** in Boost 15 s after sending -> log "NOT accepted" and
   pause 60 s.
3. **After the boost ends,** wait 5 s ("settle") and look again; if the Q is still Overruling, boost
   again (a chain). The chain counter resets when the Q reports Ventilation Mode Active in Auto.

### Safeguards

- Only acts when the Q is in **Auto** - never overrides a manual, away or boost setting chosen by a person.
- Never acts if the Q is already at fan 3.
- At most **6 boosts in a row** without the Q reporting Active in between (runaway guard).
- Master switch **Humidity Boost Enabled** (restores its last state; default on).

## Entities and settings

| Entity | Meaning |
|---|---|
| `Humidity Boost Enabled` (switch) | master enable |
| `Humidity Boost Duration` (number, 30-1800 s, step 30, remembered) | length of each boost; default 300 |
| `Humidity Boost State` (text) | "Idle - watching", "Boost sent N s", "Boost N s left", "Settling...", "Not Auto - skip", "At fan 3 - skip", "Chain limit hit", "Refused - pausing", "Off (switch)", "No CAN bus" |
| `Documentation` (diagnostic) | repository address |

A changed duration is remembered across reboots, and a remembered value beats the default in the YAML:
after changing the default in the file you must also set the number on the device.

## Status screen (plain AtomS3)

5 s test card, then: WiFi dot + IP, CAN dot + signal, fan level and name, Q mode, Ventilation Mode
(amber while Overruling), supply flow and duty, extract/exhaust RH, and a state box at the bottom
(green/grey watching, **amber while boosting**, red when off / no CAN / refused / chain limit).

## Recording behaviour

`python3 tools/record_states.py --host <ip>` writes every entity change (plus a 60 s snapshot) to
`recordings/*.csv`, which is how all of the evidence above was collected. Use it for any new site
before trusting the trigger: the thresholds of the unit's own humidity control can differ.

## Verified / not verified

- Verified (bench, kettle): trigger, boost accepted, Q ends the boost itself, chained re-boost
  (a second boost 5 s after the first ended when the Q was Overruling again), chain stops when the Q
  reports Active.
- Not verified: behaviour on a unit with real ducts and real humidity events; very long events; what
  else can set Overruling (other overrides?).
- The intended working duration was originally 5-10 minutes; 30 s and 120 s were used for testing and
  300 s is the current default. Confirm the value with the owner for each installation.
