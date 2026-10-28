"""Check the curated plate tables: every pattern parses, series anchors fit
their pattern and move forward, and every country and jurisdiction has a plate
format for every registration date. prepare_data.py runs the same check.

Usage: python scripts/check_plates.py
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import prepare_data as data  # noqa: E402


def main():
    fleet = data.read_tsv(data.CURATED / "fleet.tsv")
    errors = data.check_plates(data.read_tsv(data.CURATED / "plate_formats.tsv"),
                               data.read_tsv(data.CURATED / "plate_series.tsv"),
                               data.load_region_sets(), sorted(r["country"] for r in fleet))
    print("\n".join(errors) or "plates: ok")
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
