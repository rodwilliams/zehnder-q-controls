# Build, flash, recover

## Tools

- Linux/macOS with Python 3 and `git`.
- **ESPHome 2025.7.5**, exactly - everything here was built and tested with that version. Newer
  releases are untested; the display files are the most likely to need changes (this version has no
  built-in AtomS3R display preset and no `pad_width`/`pad_height` options, so they use `model: CUSTOM`).
- A USB-C data cable (not charge-only).

```bash
git clone --recurse-submodules https://github.com/rodwilliams/zehnder-q-controls.git
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

## Secrets and wireless (OTA) updates

Nothing secret is stored in this repository. The real values live in `secrets.yaml`, which each
maintainer keeps locally (it is git-ignored) and gets from whoever looks after the installations -
**record in your own records where the master copy is kept** (a company password manager is the
sensible place). `secrets.example.yaml` lists the five keys:

| Key | Needed for |
|---|---|
| `wifi_ssid`, `wifi_password` | the board joining the site's WiFi |
| `wifi_hotspot_password` | joining the board's own fallback hotspot to set up WiFi |
| `ota_password` | **updating a board over WiFi** |
| `api_encryption_key` | reading a board's logs / entities / recorder (`esphome logs`, Home Assistant) |

How the OTA password works:

- The board checks it against the value in the firmware **currently running on it**, so to update a board
  wirelessly you need the password that board was last flashed with. A different `ota_password` in your
  `secrets.yaml` is refused ("authentication failed").
- **If it is lost, nothing is bricked:** flash over USB (`esphome run <file> --device /dev/ttyACM0`),
  which needs no password, with the new password in `secrets.yaml`. Wireless updates then work again.
- To **change** the password on a board that already has one, flash that board once **over USB** with
  the new value in `secrets.yaml`. (A wireless update uses the password in the file being built to
  authenticate, so it only works while that equals the password the board currently has.) The one
  exception is a board with **no** password set: ESPHome's updater lets a first wireless update install
  one.
- An **empty** `ota_password` means no protection: anyone on the same WiFi can flash the board. Set a real
  one before deploying anywhere that is not a private network.
- The API key is likewise baked into the firmware; without it a new maintainer cannot read a deployed
  board's logs. Boards can still be reflashed over USB, which replaces the key.

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

## WiFi: off by default ("service mode")

Both projects are meant to run **without being on the site network** (nothing is reachable at the site
while the unit does its job). The WiFi radio is **off at boot**; the damper/humidity logic does not need it.

- **Switch WiFi on:** hold the screen button for about 2.5 seconds. WiFi stays on for **15 minutes**, then
  switches itself off. Hold again to switch it off sooner. The screen shows "WiFi off" / "WiFi joining..." /
  the IP address with the minutes left.
- It joins the network in `secrets.yaml` (`wifi_ssid`) - **bring that network with you** (a phone hotspot, a
  travel router, or a laptop access point set up with that name and password).
- If it cannot join it, after about a minute it offers its own **fallback hotspot** (exact name not
  recorded: look for a network named after the device; password = `wifi_hotspot_password`). Join it,
  and a setup page should open (otherwise browse to `192.168.4.1`); choose the new network and its password.
  It is only available during the 15-minute window.
- A WiFi update still running when the window ends fails safely: the old firmware stays in place. Start again.
- Neither WiFi nor the API reboots the board for lack of a network. ESPHome's default is to reboot after
  15 minutes without one; on the damper controller each reboot drops the output to 0 V, so this is disabled
  (`reboot_timeout: 0s` in `service_wifi_local.yml`).

Working rule: **in the enclosure, update over WiFi (service mode); out of it, over USB.** Never connect USB
while the Mini CAN Unit is on the Q's 12 V.

The development project (`ZehnderQAdaptor.yaml`) does not use service mode: its WiFi is always on.

Our bench network is a laptop access point (`hostapd`) bridged to the router. If boards join the radio network
but never get an IP address, the dongle's interface has dropped out of the bridge
(`ip -br link show master br0`); re-adding it (`sudo ip link set <iface> master br0`) or restarting `hostapd`
fixes it. This is specific to our bench, not to the firmware.

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
