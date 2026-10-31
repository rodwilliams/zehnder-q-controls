#!/usr/bin/env python3
"""Passive recorder for the ESPHome Zehnder devices: logs every state change to a CSV.

    python3 tools/record_states.py [--host 192.168.1.53] [--out recordings/uc2.csv]

Rows: iso_timestamp,entity,value. Only changes are written, plus a full snapshot every 60 s
(so a quiet stretch can be told apart from a recorder that was not running). Reconnects by
itself if the device drops off WiFi or reboots. Never sends a command - it only listens.

Needs aioesphomeapi (pip install aioesphomeapi; the esphome package brings it) and the API
encryption key, read from secrets.yaml (key: api_encryption_key) next to the project.
Records sensors, text sensors, binary sensors, selects, switches and numbers.
"""
import argparse, asyncio, csv, datetime as dt, pathlib, sys

import yaml
from aioesphomeapi import APIClient

KINDS = ("SensorInfo", "TextSensorInfo", "BinarySensorInfo", "SelectInfo", "SwitchInfo", "NumberInfo")


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.1.53")
    ap.add_argument("--secrets", default=str(root / "secrets.yaml"))
    ap.add_argument("--out", default=str(root / "recordings" / f"states_{dt.date.today()}.csv"))
    ap.add_argument("--snapshot-every", type=int, default=60, help="seconds between full snapshots")
    args = ap.parse_args()
    key = yaml.safe_load(open(args.secrets))["api_encryption_key"]
    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    new_file = not out.exists()
    fh = open(out, "a", newline="")
    writer = csv.writer(fh)
    if new_file:
        writer.writerow(["timestamp", "entity", "value"])

    async def session():
        client = APIClient(args.host, 6053, "", noise_psk=key)
        gone = asyncio.Event()

        async def on_stop(expected):
            gone.set()

        await client.connect(on_stop=on_stop, login=True)
        ents, _ = await client.list_entities_services()
        names = {e.key: e.name for e in ents if type(e).__name__ in KINDS}
        last, latest = {}, {}

        def stamp():
            return dt.datetime.now().isoformat(timespec="milliseconds")

        def on_state(s):
            if s.key not in names or not hasattr(s, "state"):
                return
            v = s.state
            if isinstance(v, float):
                v = round(v, 3)
            latest[s.key] = v
            if last.get(s.key) != v:
                last[s.key] = v
                writer.writerow([stamp(), names[s.key], v])
                fh.flush()

        client.subscribe_states(on_state)
        print(f"[{stamp()}] recording {len(names)} entities from {args.host} -> {out}", flush=True)
        while not gone.is_set():
            try:
                await asyncio.wait_for(gone.wait(), timeout=args.snapshot_every)
            except asyncio.TimeoutError:
                for k, v in latest.items():
                    writer.writerow([stamp(), names[k], v])
                writer.writerow([stamp(), "_snapshot", "end"])
                fh.flush()
        print(f"[{stamp()}] connection lost, will retry", flush=True)

    async def forever():
        while True:
            try:
                await session()
            except Exception as e:  # device rebooting / WiFi gap / handshake failure
                print(f"[{dt.datetime.now():%H:%M:%S}] {type(e).__name__}: {e} - retrying in 10 s", flush=True)
            await asyncio.sleep(10)

    try:
        asyncio.run(forever())
    except KeyboardInterrupt:
        print("stopped", file=sys.stderr)


if __name__ == "__main__":
    main()
