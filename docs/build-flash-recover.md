# Build, flash, recover

## Tools

- Linux/macOS with Python 3 and `git`.
- **ESPHome 2025.7.5**, exactly - everything here was built and tested with that version. Newer
  releases are untested; the display files are the most likely to need changes (this version has no
  built-in AtomS3R display preset and no `pad_width`/`pad_height` options, so they use `model: CUSTOM`).
- A USB-C data cable (not charge-only).

```bash
git clone --recurse-submodules git@github.com:rodwilliams/zehnder-q-controls.git
cd zehnder-q-controls
cp secrets.example.yaml secrets.yaml        # fill in real values
python3 -m venv .venv
.venv/bin/pip install esphome==2025.7.5
```

`upstream/` is a git submodule pinned to `yoziru/esphome-zehnder-comfoair` commit `68ad3d1`
(2025-08-19). If it is empty, run `git submodule update --init`. Do not edit it; change behaviour
with `!extend` or in the `*_local.yml` files and `components/`.

## secrets.yaml

Five keys (see `secrets.example.yaml`): `wifi_ssid`, `wifi_password`, `wifi_hotspot_password`
(8+ characters, for the fallback hotspot), `ota_password`, `api_encryption_key` (generate with
`openssl rand -base64 32`). Keep a copy somewhere safe - the API key is needed to talk to a device later.

## Which file for which board

| Board | File |
|---|---|
| AtomS3**R** on the dampers system | `FreeCoolingDampers.yaml` |
| plain AtomS3 on the humidity system | `HumidityBoost.yaml` |

Check the board first ([hardware doc](hardware-and-pitfalls.md#which-board-is-this)). Flashing the
wrong one gives a blank or flashing screen but is otherwise harmless.

## First flash (USB)

A board that has never run this firmware has to be flashed over USB once.

1. Plug the board into the computer **by USB only** - disconnect the CAN cable (see the power warning
   in the hardware doc).
2. (Recommended) save the stock firmware so it can be restored:
   `.venv/bin/python -m esptool --port /dev/ttyACM0 read_flash 0 0x800000 stock_backup.bin`
   (about 3-10 minutes, no progress shown if piped).
3. Flash: `.venv/bin/esphome run HumidityBoost.yaml --device /dev/ttyACM0`

On Linux the user must be in the `dialout` group to open `/dev/ttyACM0`.

## Updates (over WiFi)

```bash
.venv/bin/esphome run HumidityBoost.yaml --device <ip-address>
```

The IP address is on the board's screen; the node name is `zehnder-q-humidity-<mac suffix>` /
`zehnder-q-dampers-<mac suffix>` (a MAC suffix is added automatically). A WiFi update takes about
5 seconds and the board restarts.

## Reading logs and state

- Logs: `.venv/bin/esphome logs HumidityBoost.yaml --device <ip>` (over USB: `--device /dev/ttyACM0`).
- Web page: `http://<ip>/`. The REST interface works with curl, but a POST needs a body header:
  `curl -X POST -H "Content-Length: 0" "http://<ip>/number/humidity_boost_duration/set?value=300"`
  (without it the server answers HTTP 411).
- Recording to CSV: `python3 tools/record_states.py --host <ip>` (needs `aioesphomeapi` and `pyyaml`
  in the venv; reads the API key from `secrets.yaml`). Output goes to `recordings/` (not committed).
- Device web page has a **Documentation** entry pointing at this repository.

## WiFi lost or moving to another network

The firmware has a fallback hotspot and captive portal:

1. If the saved network cannot be joined, the board starts its own access point after about a minute
   (its exact name was not recorded: look for a new network named after the device in your WiFi list; password = `wifi_hotspot_password`).
2. Join it from a laptop; a setup page should open (otherwise browse to `192.168.4.1`).
3. Choose the new network and enter its password. The board saves it and joins.
4. If the saved network disappears later, the board reboots after 15 minutes and offers the hotspot again.

The control logic runs on the board and does **not** need WiFi - only monitoring and updates do.

Our development network is a laptop access point (`hostapd`) bridged to the router. If boards join
the radio network but never get an IP address, the dongle's interface has dropped out of the bridge
(`ip -br link show master br0`); re-adding it (`sudo ip link set <iface> master br0`) or restarting
`hostapd` fixes it. This is specific to our bench, not to the firmware.

## Restoring the stock firmware

`.venv/bin/python -m esptool --port /dev/ttyACM0 write_flash 0 stock_backup.bin`
(the file saved in step 2 above). Stock images are **not** in this repository: they can contain the
previous owner's settings.

## Verifying a build without hardware

```bash
.venv/bin/esphome config HumidityBoost.yaml     # must end with "Configuration is valid!"
.venv/bin/esphome compile HumidityBoost.yaml
```

Always look for the "valid" line - truncated output can hide a failure.
