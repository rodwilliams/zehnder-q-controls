# Working on this repository (for AI assistants and new maintainers)

Read `README.md` first, then `docs/`. This file holds the working rules that are not obvious from the code.

## Hard rules
- **ESPHome is pinned to 2025.7.5.** Everything was built and tested with it. Check for "Configuration is valid!" in
  the output of `esphome config` - truncated output has been mistaken for success before.
- **Never edit `upstream/`** (git submodule of yoziru/esphome-zehnder-comfoair, GPL-3.0). Extend with `!extend`, or
  change the `*_local.yml` files and `components/`.
- **Never commit `secrets.yaml`** or print secret values. Before a commit, search the staged changes for the values
  (count matches only). The OTA password is blank **by the owner's choice**; do not "fix" it unasked.
- One top-level YAML per project; keep changes small so they could be offered upstream.

## Before touching hardware
- **Identify the exact board first**: `esptool flash_id`. `ESP32-S3-PICO-1` + 8 MB PSRAM = AtomS3**R**; `ESP32-S3 (QFN56)`
  with no PSRAM = plain AtomS3. They differ in display pins, backlight and panel (`docs/hardware-and-pitfalls.md`).
  Do not flash a board whose type you have not confirmed.
- Pin maps come from the vendor's **source code** (M5GFX / M5Unified), not from web summaries. A summary once
  swapped SDA/SCL and cost days.
- Debug order when something "does not work": (1) live I2C scan, (2) is the instrument right - check a meter against
  a known source or a second meter, (3) only then suspect wiring or modules. Measure the CAN adapter's H-L
  resistance unpowered before connecting it to a live bus (about 120 ohm good; 94 ohm was faulty).
- Never power the board from USB while the Mini CAN Unit is on the Q's 12 V.

## Behaviour decisions made by the owner (do not change silently)
- UC1: 10 V only at fan level 3; closed (0 V) on anything unknown; no hysteresis.
- UC2: act only when the Q is in **Auto**, never at fan 3, boost length 300 s, re-boost while the Q still reports
  Overruling. A flow/humidity trigger was tried and **removed on purpose**.
- The Option Box emulation is **paused** - do not resume it unprompted.
- If you add a safeguard that changes behaviour, say so plainly.

## Process
- Ask before flashing, pushing, or changing anything outward-facing. Pushes publish to a public repository.
- Prefer one batched shell command over many small ones; give plain-language status, especially during long steps
  (a full 8 MB flash read shows no progress when piped).
- Do not `pkill -f` a pattern that also appears in your own command line.
- Commits use the repo-local noreply identity; do not set a global git identity.
- Before any UC1 site visit, raise the two damper checks listed in `docs/hardware-and-pitfalls.md`
  ("Not yet checked on site").
