"""Check the world manufacturer identifiers against NHTSA vPIC.

Every reserved WMI (wmis.tsv, used for safe VINs) must be unknown to vPIC's
DecodeWMI; the script fails otherwise. It also lists the factories' WMIs that
vPIC decodes to no manufacturer, for review (vPIC lists makers that have sold
in the United States, so many non-US WMIs are expected there).

Usage: python scripts/prepare_wmis.py [--refresh]
  Run from the repository root after prepare_data.py. Answers are cached in
  raw_data/vpic_wmis.json and reused unless --refresh is given.
"""

import argparse
import csv
import json
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "raw_data"
VPIC = "https://vpic.nhtsa.dot.gov/api/vehicles/DecodeWMI/{}?format=json"


def read_tsv(path):
    with open(path, encoding="utf-8", newline="") as f:
        return list(csv.DictReader((line for line in f if not line.startswith("#")), delimiter="\t"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--refresh", action="store_true")
    args = parser.parse_args()
    cache_path = RAW / "vpic_wmis.json"
    cache = {} if args.refresh or not cache_path.exists() else json.loads(cache_path.read_text(encoding="utf-8"))

    def lookup(wmi):
        if wmi not in cache:
            request = urllib.request.Request(VPIC.format(wmi), headers={"User-Agent": "dasmig-vehicle-generator data build"})
            with urllib.request.urlopen(request, timeout=60) as response:
                results = json.loads(response.read())["Results"]
            cache[wmi] = sorted({r.get("ManufacturerName") or "" for r in results} - {""})
            time.sleep(0.2)
        return cache[wmi]

    full = ROOT / "resources" / "full"
    reserved = [r["wmi"] for r in read_tsv(full / "wmis.tsv") if r["reserved"] == "1"]
    factories = read_tsv(full / "factories.tsv")
    held = sorted({f[k] for f in factories for k in ("wmi", "wmi_suv", "wmi_truck") if f[k]})

    failures = [w for w in reserved if lookup(w)]
    unknown = [w for w in held if not lookup(w)]
    RAW.mkdir(exist_ok=True)
    cache_path.write_text(json.dumps(cache, indent=1, sort_keys=True), encoding="utf-8")

    print(f"reserved WMIs: {len(reserved)}, held by a maker in vPIC: {', '.join(failures) or 'none'}")
    print(f"factory WMIs: {len(held)}, unknown to vPIC: {len(unknown)} ({' '.join(unknown)})")
    if failures:
        sys.exit(1)


if __name__ == "__main__":
    main()
