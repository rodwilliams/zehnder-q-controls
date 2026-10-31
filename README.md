# Zehnder Q controls

ESP32 firmware (ESPHome) that listens to a **Zehnder ComfoAir Q450/Q600** ventilation unit on its
ComfoNet CAN bus and adds control behaviour the unit does not have on its own. Two systems are
maintained here, each on its own hardware and each built from its own top-level file:

| System | Top-level file | What it does | Hardware |
|---|---|---|---|
| **UC1 - free-cooling dampers** | `FreeCoolingDampers.yaml` | Output **10 V while the Q runs at fan level 3, 0 V otherwise**, to open/close Lindab UltraLink FTCU dampers. Default (boot, no data, no CAN) is closed. | AtomS3**R** + Mini CAN Unit + GP8413 DAC, status screen |
| **UC2 - humidity boost** | `HumidityBoost.yaml` | When the Q starts overruling for humidity, **force a timed boost to fan level 3** (default 300 s) and repeat while the Q is still overruling. The Q returns itself to Auto when the timer ends. | plain AtomS3 + Mini CAN Unit, status screen |
| Bench test | `dac_sweep_test.yaml` | Steps the DAC through 0 / 2.5 / 5 / 7.5 / 10 V (4 s each) to check the DAC and your meter. Replaces UC1 on the board. | AtomS3R + GP8413 |
| Development | `ZehnderQAdaptor.yaml` | Impersonates the Zehnder *Option Box* to inject 0-10 V analogue inputs. **Paused**, not for production. | AtomS3R + Mini CAN + DAC |

> **AtomS3 and AtomS3R are different boards** with different display pins and backlight. UC1 is built
> for the S3**R**, UC2 for the plain S3. Identify a board before flashing it:
> [docs/hardware-and-pitfalls.md](docs/hardware-and-pitfalls.md#which-board-is-this).

## Documentation

- [docs/build-flash-recover.md](docs/build-flash-recover.md) - **start here to rebuild**: tools, first flash, updates, WiFi recovery, restoring stock firmware
- [docs/uc1-dampers.md](docs/uc1-dampers.md) - UC1 behaviour, entities, what to check on site
- [docs/uc2-humidity-boost.md](docs/uc2-humidity-boost.md) - UC2 behaviour, the evidence behind it, safeguards, known limits
- [docs/hardware-and-pitfalls.md](docs/hardware-and-pitfalls.md) - wiring, pins, boards, and the mistakes already made (read before debugging hardware)
- [docs/q-protocol-notes.md](docs/q-protocol-notes.md) - CAN/PDO findings reverse-engineered from live captures

## Repository layout

```
FreeCoolingDampers.yaml, HumidityBoost.yaml, ZehnderQAdaptor.yaml   one top-level file per project
base_local.yml, canbus_local.yml                                    shared core (CAN decoding, requests)
analogue_inputs_local.yml, dac_local.yml, impersonation_local.yml   optional shared parts
uc1_*.yml, uc2_*.yml                                                the two behaviours + their status screens
display_atoms3_*.yml, display_atoms3r_*.yml                         screen drivers for each board type
documentation_local.yml                                             adds the "Documentation" entry on every device
components/zehnder_comfoair_q/                                      our modified copy of the upstream CAN component
components/gp8413/                                                  DAC driver (DFRobot GP8413, 15-bit, I2C 0x59)
tools/record_states.py                                              passive CSV recorder of a device's entities
upstream/                                                           git submodule: yoziru/esphome-zehnder-comfoair, pinned
fonts/                                                              DejaVu Sans Mono (screen text)
secrets.example.yaml                                                copy to secrets.yaml and fill in (never committed)
```

## Quick start

```bash
git clone --recurse-submodules https://github.com/rodwilliams/zehnder-q-controls.git
cd zehnder-q-controls
cp secrets.example.yaml secrets.yaml      # then edit it
python3 -m venv .venv && .venv/bin/pip install esphome==2025.7.5
.venv/bin/esphome compile HumidityBoost.yaml
```

ESPHome is pinned to **2025.7.5**; newer versions changed APIs these files use (see build doc).
Full procedure, including the first USB flash, in [docs/build-flash-recover.md](docs/build-flash-recover.md).

## Security notes

- `secrets.yaml` holds the WiFi login, fallback-hotspot password, API key and OTA password. It is
  excluded by `.gitignore` - **never commit it**.
- **The OTA password in the current deployment is empty** (as in the upstream example), so anyone on
  the same WiFi network can reflash a board. Set `ota_password` before a deployment on a shared network.
  How secrets, the OTA password and USB recovery work: [docs/build-flash-recover.md](docs/build-flash-recover.md#secrets-and-wireless-ota-updates).
- The device web page has no login.

## Credits and licence

Derived from [yoziru/esphome-zehnder-comfoair](https://github.com/yoziru/esphome-zehnder-comfoair)
(GPL-3.0), which in turn builds on the Zehnder ComfoNet protocol work in
[michaelarnauts/aiocomfoconnect](https://github.com/michaelarnauts/aiocomfoconnect). This repository is
therefore licensed **GPL-3.0** - see [LICENSE](LICENSE) and [NOTICE](NOTICE).
