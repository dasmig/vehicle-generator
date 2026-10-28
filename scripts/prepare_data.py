"""Build resources/lite and resources/full from the curated tables and the downloads.

Reads:
  scripts/curated/*.tsv          the curated tables (see doc/data.md)
  ../country-generator/resources population, driving side and the alpha-3 codes
  Eurostat road_eqs_carage       passenger cars by age (EU, EFTA, candidates)
  Eurostat road_eqr_carpda       new passenger cars by motor energy
  Eurostat road_eqs_motorc       motorcycles in use (EU, EFTA, candidates)
  Our World in Data              electric and plug-in shares of new cars (IEA, CC BY 4.0)

Writes, for each tier, every table vehiclegen.hpp loads, and fails (exit 1)
when a plate pattern does not parse, a series anchor is out of order, a
country lacks a plate format for some kind and registration date, or a
country cannot draw a car or a motorcycle for some model year.

Usage: python scripts/prepare_data.py [--refresh]
  Run from the repository root. Downloads go to raw_data/ and are reused
  unless --refresh is given. Needs ../country-generator.
"""

import argparse
import csv
import datetime
import json
import math
import sys
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import plates  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "raw_data"
CURATED = ROOT / "scripts" / "curated"
COUNTRY_GENERATOR = ROOT.parent / "country-generator"

EUROSTAT = "https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/{}?format=JSON&lang=EN"
OWID = "https://ourworldindata.org/grapher/{}.csv?v=1&csvType=full&useColumnShortNames=true"

FUELS = ["petrol", "diesel", "hybrid", "plug_in", "electric", "lpg", "cng", "flex"]
FAMILIES = ["white", "black", "grey", "silver", "blue", "red", "brown", "green", "yellow", "orange", "other"]
KINDS = ["car", "motorcycle"]
BODIES = {"car": ["hatchback", "sedan", "wagon", "suv", "pickup", "van", "coupe", "convertible"],
          "motorcycle": ["scooter", "underbone", "standard", "sport", "touring", "cruiser", "adventure", "offroad"]}
GEARBOXES = {"car": "ma", "motorcycle": "mas"}
DRIVES = {"car": "fra", "motorcycle": "cbsh"}
BANDS = [("0-1", 0, 1), ("2-4", 2, 4), ("5-9", 5, 9), ("10-19", 10, 19), ("20+", 20, 45)]
FIRST_YEAR, LAST_YEAR = 1990, 2026
MILES = {"US", "GB", "LR", "MM", "PR", "VI", "GU", "AS", "MP", "IM", "JE", "GG", "FK", "SH"}
MILEAGE_FACTORS = {"diesel": 1.35, "hybrid": 1.1, "plug_in": 0.9, "electric": 0.85, "lpg": 1.3, "cng": 1.3}
RESERVED_WMIS = ["EZ1", "EZ7", "FZ3", "FZ8", "GZ5", "GZ9"]
EUROSTAT_GEO = {"EL": "GR", "UK": "GB"}


def fetch(url, name, refresh):
    path = RAW / name
    if refresh or not path.exists():
        RAW.mkdir(exist_ok=True)
        request = urllib.request.Request(url, headers={"User-Agent": "dasmig-vehicle-generator data build"})
        with urllib.request.urlopen(request, timeout=300) as response:
            path.write_bytes(response.read())
    return path


def read_tsv(path):
    with open(path, encoding="utf-8", newline="") as f:
        return list(csv.DictReader((line for line in f if not line.startswith("#")), delimiter="\t"))


def comment(path):
    with open(path, encoding="utf-8") as f:
        return f.readline().rstrip("\n")


def write_tsv(path, header_comment, columns, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(header_comment + "\n")
        f.write("\t".join(columns) + "\n")
        for row in rows:
            f.write("\t".join("" if v is None else str(v) for v in row) + "\n")


def fmt(x, digits=4):
    text = f"{x:.{digits}f}".rstrip("0").rstrip(".")
    return text if text not in ("", "-0") else "0"


def anchors(text):
    """'1990:0.05,2010:0.2' or '1990=0.05,...' -> sorted [(year, value)]."""
    out = []
    for part in filter(None, text.split(",")):
        year, _, value = part.replace("=", ":").partition(":")
        out.append((int(year), float(value)))
    return sorted(out)


def interpolate(points, year):
    if not points:
        return 0.0
    if year <= points[0][0]:
        return points[0][1]
    for (y0, v0), (y1, v1) in zip(points, points[1:]):
        if year <= y1:
            return v0 + (v1 - v0) * (year - y0) / (y1 - y0)
    return points[-1][1]


# ---- downloads ----------------------------------------------------------

def jsonstat(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    ids, sizes = data["id"], data["size"]
    index = {d: data["dimension"][d]["category"]["index"] for d in ids}
    out = {}
    for flat, value in data["value"].items():
        flat = int(flat)
        key = []
        for d, size in reversed(list(zip(ids, sizes))):
            position = flat % size
            flat //= size
            key.append(next(k for k, v in index[d].items() if v == position))
        out[tuple(reversed(key))] = value
    return ids, out


def eurostat_ages(refresh):
    ids, values = jsonstat(fetch(EUROSTAT.format("road_eqs_carage"), "eu_carage.json", refresh))
    by_geo = {}
    for key, value in values.items():
        k = dict(zip(ids, key))
        geo = EUROSTAT_GEO.get(k["geo"], k["geo"])
        by_geo.setdefault(geo, {}).setdefault(int(k["time"]), {})[k["age"]] = value
    codes = ["Y_LT2", "Y2-5", "Y5-10", "Y10-20", "Y_GT20"]
    out = {}
    for geo, years in by_geo.items():
        if len(geo) != 2:
            continue
        complete = [years[y] for y in sorted(years) if all(years[y].get(c) for c in codes)]
        if not complete:
            continue
        recent = complete[-3:]
        shares = [sum(row[c] / sum(row[x] for x in codes) for row in recent) / len(recent) for c in codes]
        out[geo] = shares
    return out


def eurostat_fuels(refresh):
    ids, values = jsonstat(fetch(EUROSTAT.format("road_eqr_carpda"), "eu_road_eqr_carpda.json", refresh))
    rows = {}
    for key, value in values.items():
        k = dict(zip(ids, key))
        geo = EUROSTAT_GEO.get(k["geo"], k["geo"])
        rows.setdefault((geo, int(k["time"])), {})[k["mot_nrg"]] = value
    out = {}
    for (geo, year), r in rows.items():
        total = r.get("TOTAL")
        if len(geo) != 2 or not total or r.get("DIE") is None or r.get("ELC") is None:
            continue
        diesel = r.get("DIE_X_HYB", r["DIE"] - r.get("ELC_DIE_HYB", 0) - r.get("ELC_DIE_PI", 0))
        shares = {
            "diesel": diesel / total,
            "hybrid": (r.get("ELC_PET_HYB", 0) + r.get("ELC_DIE_HYB", 0)) / total,
            "plug_in": (r.get("ELC_PET_PI", 0) + r.get("ELC_DIE_PI", 0)) / total,
            "electric": r["ELC"] / total,
            "lpg": r.get("LPG", 0) / total,
            "cng": r.get("GAS", 0) / total,
        }
        if sum(shares.values()) <= 1.0:
            out.setdefault(geo, {})[year] = shares
    return out


def owid_ev(refresh, alpha3):
    path = fetch(OWID.format("share-car-sales-battery-plugin"), "owid_share-car-sales-battery-plugin.csv", refresh)
    out = {}
    with open(path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            code = alpha3.get(row["code"])
            if not code or not row["bev_share_car_sales"]:
                continue
            bev = float(row["bev_share_car_sales"]) / 100
            phev = float(row["phev_share_car_sales"] or 0) / 100
            out.setdefault(code, {})[int(row["year"])] = (bev, phev)
    return out


# ---- curated ------------------------------------------------------------

def load_models():
    models = []
    paths = [(path, "car") for path in sorted(CURATED.glob("models_*.tsv"))]
    for path, kind in paths + [(CURATED / "motorcycle_models.tsv", "motorcycle")]:
        for row in read_tsv(path):
            bodies = {}
            for part in row["bodies"].split(","):
                body, _, weight = part.partition(":")
                if body not in BODIES[kind]:
                    sys.exit(f"{path.name}: {row['key']}: unknown body {body}")
                bodies[body] = float(weight or 1)
            markets = {}
            for part in row["markets"].split(","):
                market, _, weight = part.partition(":")
                markets[market] = float(weight or 1)
            model = dict(key=row["key"], kind=kind, make=row["make"], model=row["model"], start=int(row["from"]),
                         end=int(row["to"]) if row["to"] else None, bodies=bodies, markets=markets,
                         seats=int(row["seats"]), lite=row["lite"] == "1", trims=[])
            seen = set()
            for spec in filter(None, row["trims"].split(";")):
                f = spec.split("|") + [""] * 9
                label, fuel = f[0], f[1]
                if fuel not in FUELS:
                    sys.exit(f"{path.name}: {row['key']}: unknown fuel {fuel} in {spec}")
                years = f[4] or f"{model['start']}-{row['to']}"
                start, _, end = years.partition("-")
                trim = dict(label=label, fuel=fuel, cc=int(f[2] or 0), kw=int(f[3] or 0),
                            start=max(int(start or model["start"]), model["start"]),
                            end=int(end) if end else model["end"],
                            trans=f[5] or GEARBOXES[kind][:2], drives=f[6] or DRIVES[kind][0],
                            weight=float(f[7] or 1),
                            markets=[m for m in f[8].split(",") if m])
                if model["end"] is not None and (trim["end"] is None or trim["end"] > model["end"]):
                    trim["end"] = model["end"]
                if fuel == "electric":
                    trim["cc"] = 0
                if (label, fuel, trim["start"]) in seen:
                    sys.exit(f"{path.name}: {row['key']}: trim {label} ({fuel}) from {trim['start']} twice")
                seen.add((label, fuel, trim["start"]))
                if trim["end"] is not None and trim["end"] < trim["start"]:
                    sys.exit(f"{path.name}: {row['key']}: trim {label} ends before it starts")
                if set(trim["trans"]) - set(GEARBOXES[kind]) or set(trim["drives"]) - set(DRIVES[kind]):
                    sys.exit(f"{path.name}: {row['key']}: bad gearbox or drive in {spec}")
                model["trims"].append(trim)
            if not model["trims"]:
                sys.exit(f"{path.name}: {row['key']}: no trims")
            models.append(model)
    keys = [m["key"] for m in models]
    if len(keys) != len(set(keys)):
        sys.exit("duplicate model keys: " + ", ".join(sorted({k for k in keys if keys.count(k) > 1})))
    return models


def model_weight(model, country, markets):
    if country in model["markets"]:
        return model["markets"][country]
    for market in markets:
        if market in model["markets"]:
            return model["markets"][market]
    return 0.0


def trim_sold(trim, country, markets):
    return not trim["markets"] or country in trim["markets"] or any(m in trim["markets"] for m in markets)


def offers(model, trim, fuel, year):
    if trim["start"] > year or (trim["end"] is not None and trim["end"] < year):
        return False
    converted = model["kind"] == "car" and fuel in ("lpg", "cng", "flex") and trim["fuel"] == "petrol"
    return trim["fuel"] == fuel or converted


def check_plates(formats, series, region_sets, countries):
    errors = []
    parsed = {}
    for f in formats:
        try:
            items = plates.parse(f["pattern"])
        except plates.PatternError as e:
            errors.append(f"plate format {f['key']}: {e}")
            continue
        parsed[f["key"]] = items
        uses_region = any(i[0] == "token" and i[1] in "RSC" for i in items)
        if uses_region and f["regions"] not in region_sets:
            errors.append(f"plate format {f['key']}: region set {f['regions'] or '(none)'} missing")
        if f["regions"] and not uses_region:
            errors.append(f"plate format {f['key']}: names regions but has no R")
        if f["kind"] == "sequential" and not any(i[4] for i in items):
            errors.append(f"plate format {f['key']}: sequential without a group")
        if f["region"] != "*" and f["country"] not in region_sets:
            errors.append(f"plate format {f['key']}: no jurisdiction set {f['country']}")
        if any(k not in KINDS for k in filter(None, f["kinds"].split(";"))):
            errors.append(f"plate format {f['key']}: unknown kind in {f['kinds']}")
    by_format = {}
    for s in series:
        by_format.setdefault(s["format"], []).append(s)
    keys = {f["key"]: f for f in formats}
    for key, rows in by_format.items():
        if key not in keys or key not in parsed:
            errors.append(f"plate series for unknown format {key}")
            continue
        alphabet = plates.letters(keys[key]["letters"])
        last = (None, -1)
        for row in sorted(rows, key=lambda r: r["date"]):
            index = plates.plate_index(row["plate"], parsed[key], alphabet)
            if index is None:
                errors.append(f"plate series {key} {row['date']}: {row['plate']} does not fit the pattern")
            elif index < last[1]:
                errors.append(f"plate series {key} {row['date']}: {row['plate']} is before {last[0]}")
            else:
                last = (row["plate"], index)
    for f in formats:
        if f["kind"] == "sequential" and f["key"] not in by_format:
            errors.append(f"plate format {f['key']}: sequential without anchors")
    # Every country (and jurisdiction) has a general format for every kind and registration date.
    start = datetime.date(FIRST_YEAR - 1, 10, 1)
    end = datetime.date(LAST_YEAR, 12, 31)
    for country, kind in ((c, k) for c in countries for k in KINDS):
        rows = [f for f in formats if f["country"] == country and not f["fuels"] and not f["min_age"]
                and (not f["kinds"] or kind in f["kinds"].split(";"))]
        if not rows:
            errors.append(f"{country}: no plate format for a {kind}")
            continue
        jurisdictions = sorted({f["region"] for f in rows})
        if jurisdictions != ["*"]:
            listed = {r["code"] for r in region_sets.get(country, []) if not r["reserved"]}
            if "*" in jurisdictions or set(jurisdictions) != listed:
                errors.append(f"{country}: formats' regions {jurisdictions} differ from set {sorted(listed)}")
        for jurisdiction in jurisdictions:
            spans = sorted((f["from"] or "0000-00-00", f["until"] or "9999-99-99")
                           for f in rows if f["region"] == jurisdiction)
            reached = start.isoformat()
            for low, high in spans:
                if low > reached:
                    break
                if high >= reached:
                    reached = (datetime.date.fromisoformat(high) + datetime.timedelta(days=1)).isoformat() \
                        if high != "9999-99-99" else "9999-99-99"
            if reached <= end.isoformat():
                errors.append(f"{country} {jurisdiction}: no {kind} plate format from {reached}")
    return errors


def load_region_sets():
    out = {}
    for row in read_tsv(CURATED / "plate_regions.tsv"):
        rows = []
        for part in row["codes"].split(";"):
            code, _, rest = part.partition("=")
            fields = rest.split(":")
            reserved = code.startswith("!")
            rows.append(dict(code=code.lstrip("!"), label=fields[0], weight=fields[1] if len(fields) > 1 else "1",
                             sub=fields[2] if len(fields) > 2 else "", reserved=reserved))
        codes = [r["code"] for r in rows]
        if len(codes) != len(set(codes)):
            sys.exit(f"plate_regions {row['set']}: duplicate codes "
                     + ", ".join(sorted({c for c in codes if codes.count(c) > 1})))
        out[row["set"]] = rows
    return out


# ---- build --------------------------------------------------------------

def geometric_shares(mean_age):
    """Fleet band shares for a country without a register: ages 0-45 with a
    yearly survival ratio chosen so the mean age is mean_age."""
    def mean(r):
        weights = [r ** a for a in range(46)]
        return sum(a * w for a, w in enumerate(weights)) / sum(weights)
    low, high = 0.01, 0.999
    for _ in range(60):
        mid = (low + high) / 2
        if mean(mid) < mean_age:
            low = mid
        else:
            high = mid
    weights = [low ** a for a in range(46)]
    total = sum(weights)
    return [sum(weights[a] for a in range(lo, hi + 1)) / total for _, lo, hi in BANDS]


def eurostat_motorcycles(refresh):
    """Latest measured motorcycle stock (year, count) per country, from 2015 on."""
    ids, values = jsonstat(fetch(EUROSTAT.format("road_eqs_motorc"), "eu_road_eqs_motorc.json", refresh))
    out = {}
    for key, value in values.items():
        k = dict(zip(ids, key))
        geo = EUROSTAT_GEO.get(k["geo"], k["geo"])
        year = int(k["time"])
        if k["engine"] != "TOTAL" or len(geo) != 2 or not value or year < 2015:
            continue
        if year > out.get(geo, (0, 0))[0]:
            out[geo] = (year, value)
    return out


def parse_makes(text, makes_with_models, label, errors):
    entries = []
    for part in text.split(";"):
        make, _, rest = part.partition(":")
        weight, _, trend = rest.partition("/")
        if make not in makes_with_models:
            errors.append(f"{label}: {make} has no models")
        entries.append((make, float(weight), float(trend or 0)))
    return entries


def fuel_table(points):
    """Fuel anchors {fuel: [(year, share)]} -> rows [year, petrol, diesel, ...]."""
    years = sorted({FIRST_YEAR, LAST_YEAR} | {y for p in points.values() for y, _ in p if FIRST_YEAR <= y <= LAST_YEAR})
    rows = []
    for year in years:
        shares = {fuel: max(0.0, interpolate(points[fuel], year)) for fuel in points}
        total = sum(shares.values())
        if total > 1:
            shares = {k: v / total for k, v in shares.items()}
            total = 1.0
        rows.append([year, 1 - total] + [shares[f] for f in FUELS[1:]])
    return rows


def fuel_points(text, label):
    points = {fuel: [] for fuel in FUELS[1:]}
    for part in filter(None, text.split(";")):
        fuel, _, values = part.partition(":")
        if fuel not in points:
            sys.exit(f"{label}: unknown fuel {fuel}")
        points[fuel] = anchors(values)
    return points


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--refresh", action="store_true")
    args = parser.parse_args()

    tiers = {}
    alpha3 = {}
    for tier in ("lite", "full"):
        rows = read_tsv(COUNTRY_GENERATOR / "resources" / tier / "countries.tsv")
        tiers[tier] = {r["cca2"]: r for r in rows}
        alpha3.update({r["cca3"]: r["cca2"] for r in rows})

    fleet = {r["country"]: r for r in read_tsv(CURATED / "fleet.tsv")}
    moto_fleet = {r["country"]: r for r in read_tsv(CURATED / "motorcycle_fleet.tsv")}
    make_texts = {"car": {r["country"]: r["makes"] for r in read_tsv(CURATED / "make_shares.tsv")},
                  "motorcycle": {r["country"]: r["makes"] for r in read_tsv(CURATED / "motorcycle_make_shares.tsv")}}
    fuel_anchors = {r["country"]: r["fuels"] for r in read_tsv(CURATED / "fuels.tsv")}
    moto_fuel_anchors = {r["country"]: r["fuels"] for r in read_tsv(CURATED / "motorcycle_fuels.tsv")}
    models = load_models()
    formats = read_tsv(CURATED / "plate_formats.tsv")
    series = read_tsv(CURATED / "plate_series.tsv")
    region_sets = load_region_sets()
    factories = {"car": read_tsv(CURATED / "factories.tsv"),
                 "motorcycle": read_tsv(CURATED / "motorcycle_factories.tsv")}
    colours = {"car": read_tsv(CURATED / "colours.tsv"), "motorcycle": read_tsv(CURATED / "motorcycle_colours.tsv")}
    colour_names = [dict(r, kind="car") for r in read_tsv(CURATED / "colour_names.tsv")] + \
        [dict(r, kind="motorcycle") for r in read_tsv(CURATED / "motorcycle_colour_names.tsv")]
    owner_factors = read_tsv(CURATED / "owner_factors.tsv")

    errors = []
    for country in fleet:
        for kind in KINDS:
            if country not in make_texts[kind]:
                errors.append(f"{country}: no {kind} make shares")
        if country not in fuel_anchors:
            errors.append(f"{country}: no fuel anchors")
        if country not in moto_fleet:
            errors.append(f"{country}: no motorcycle fleet")
    makes_with_models = {kind: {m["make"] for m in models if m["kind"] == kind} for kind in KINDS}
    parsed_makes = {kind: {country: parse_makes(text, makes_with_models[kind], f"{kind} make shares {country}", errors)
                           for country, text in make_texts[kind].items()} for kind in KINDS}
    for kind in KINDS:
        for f in factories[kind]:
            if f["make"] not in makes_with_models[kind]:
                errors.append(f"{kind} factories: {f['make']} has no models")
            for wmi in (f["wmi"], f["wmi_suv"], f["wmi_truck"]):
                if len(wmi) != 3 or any(c in "IOQ" for c in wmi) or wmi in RESERVED_WMIS:
                    errors.append(f"{kind} factories: {f['make']} {f['market']} {f['country']}: bad WMI {wmi}")
        for make in sorted(makes_with_models[kind] - {f["make"] for f in factories[kind]}):
            errors.append(f"{kind} factories: {make} has no factory")
    names = [(r["make"], r["family"], r["name"]) for r in colour_names]
    for key in sorted({k for k in names if names.count(k) > 1}):
        errors.append(f"colour names: {' / '.join(key)} twice")
    errors += check_plates(formats, series, region_sets, sorted(fleet))
    if errors:
        print("\n".join(errors))
        sys.exit(1)

    ages_eu = eurostat_ages(args.refresh)
    fuels_eu = eurostat_fuels(args.refresh)
    motorcycles_eu = eurostat_motorcycles(args.refresh)
    ev = owid_ev(args.refresh, alpha3)

    # Fuel shares per kind and country: curated anchors, then (cars) measured years override.
    fuel_rows = {"car": {}, "motorcycle": {}}
    for country in fleet:
        points = fuel_points(fuel_anchors[country], f"fuels {country}")
        measured = {}
        for year, shares in fuels_eu.get(country, {}).items():
            measured[year] = dict(shares)
        for year, (bev, phev) in ev.get(country, {}).items():
            if year >= 2010 and year not in measured:
                measured.setdefault(year, {})
                measured[year]["electric"] = bev
                measured[year]["plug_in"] = phev
        for fuel in points:
            years = {y: s[fuel] for y, s in measured.items() if fuel in s}
            if years:
                kept = [(y, v) for y, v in points[fuel] if y < min(years)]
                points[fuel] = sorted(kept + list(years.items()))
        fuel_rows["car"][country] = fuel_table(points)
        text = moto_fuel_anchors.get(country, moto_fuel_anchors["*"]) or ""
        fuel_rows["motorcycle"][country] = fuel_table(fuel_points(text, f"motorcycle fuels {country}"))

    def fuel_share(kind, country, fuel, year):
        rows = fuel_rows[kind][country]
        column = FUELS.index(fuel) + 1
        return interpolate([(r[0], r[column]) for r in rows], year)

    population = {code: float(row["population"]) for code, row in tiers["full"].items()}
    moto_per_1000 = {}
    for country, row in moto_fleet.items():
        measured = motorcycles_eu.get(country)
        moto_per_1000[country] = fmt(1000 * measured[1] / population[country], 1) if measured and \
            population.get(country) else row["motorcycles_per_1000"]

    for tier, country_rows in tiers.items():
        out = ROOT / "resources" / tier
        countries = [c for c in sorted(fleet) if c in country_rows]
        tier_models = [m for m in models if tier == "full" or m["lite"]]
        tier_makes = {kind: {m["make"] for m in tier_models if m["kind"] == kind} for kind in KINDS}

        # Feasibility: every model year has some car and some motorcycle in every country.
        for country, kind in ((c, k) for c in countries for k in KINDS):
            markets = fleet[country]["markets"].split(";")
            makes = {make for make, weight, _ in parsed_makes[kind][country] if weight > 0}
            kind_models = [m for m in tier_models if m["kind"] == kind and m["make"] in makes
                           and model_weight(m, country, markets) > 0]
            for year in range(FIRST_YEAR, LAST_YEAR + 1):
                if not any(fuel_share(kind, country, fuel, year) > 0 and any(
                        trim_sold(t, country, markets) and offers(m, t, fuel, year)
                        for m in kind_models for t in m["trims"]) for fuel in FUELS):
                    errors.append(f"{tier} {country}: no {kind} for model year {year}")
                    break
        if errors:
            print("\n".join(errors))
            sys.exit(1)

        write_tsv(out / "countries.tsv",
                  "# Countries: population (country-generator), passenger cars per 1,000 people, mean car fleet "
                  "age, distance unit, driving side, the markets whose model ranges are sold there, motorcycles "
                  "per 1,000 people (Eurostat road_eqs_motorc where measured since 2015, curated elsewhere) and "
                  "mean motorcycle fleet age (curated, see doc/data.md).",
                  ["country_code", "population", "cars_per_1000", "mean_age", "units", "driving_side", "markets",
                   "motorcycles_per_1000", "motorcycle_mean_age"],
                  [[c, country_rows[c]["population"], fleet[c]["cars_per_1000"], fleet[c]["mean_age"],
                    "mi" if c in MILES else "km", country_rows[c]["driving_side"] or "right", fleet[c]["markets"],
                    moto_per_1000[c], moto_fleet[c]["mean_age"]]
                   for c in countries])

        age_rows = []
        for c in countries:
            shares = ages_eu.get(c) or geometric_shares(float(fleet[c]["mean_age"]))
            age_rows += [[c, band, fmt(share)] for (band, _, _), share in zip(BANDS, shares)]
        write_tsv(out / "ages.tsv",
                  "# Share of the car fleet in each age band (completed years since the model year): Eurostat "
                  "road_eqs_carage (mean of the last three complete years) where measured; elsewhere a yearly "
                  "survival curve fitted to the curated mean age.",
                  ["country", "band", "share"], age_rows)

        write_tsv(out / "fuel_shares.tsv",
                  "# Fuel mix of new cars by model year (interpolated between rows): Eurostat road_eqr_carpda "
                  "(2013 on) in Europe, Our World in Data / IEA Global EV Outlook (CC BY 4.0) for electric and "
                  "plug-in elsewhere, curated anchors before and around them; petrol is the remainder.",
                  ["country", "year"] + FUELS,
                  [[c, r[0]] + [fmt(v) for v in r[1:]] for c in countries for r in fuel_rows["car"][c]])
        write_tsv(out / "motorcycle_fuel_shares.tsv", comment(CURATED / "motorcycle_fuels.tsv"),
                  ["country", "year"] + FUELS,
                  [[c, r[0]] + [fmt(v) for v in r[1:]] for c in countries for r in fuel_rows["motorcycle"][c]])

        for kind, name in (("car", "make_shares"), ("motorcycle", "motorcycle_make_shares")):
            write_tsv(out / f"{name}.tsv", comment(CURATED / f"{name}.tsv"),
                      ["country", "make", "weight", "trend"],
                      [[c, make, fmt(weight), fmt(trend)] for c in countries
                       for make, weight, trend in parsed_makes[kind][c] if make in tier_makes[kind]])

        write_tsv(out / "models.tsv",
                  comment(CURATED / "models_europe.tsv").replace("models of the European makers", "car and "
                                                                 "motorcycle models"),
                  ["key", "kind", "make", "model", "from_year", "to_year", "bodies", "markets", "seats"],
                  [[m["key"], m["kind"], m["make"], m["model"], m["start"], m["end"] or "",
                    ",".join(f"{b}:{fmt(w)}" for b, w in m["bodies"].items()),
                    ",".join(f"{k}:{fmt(w)}" for k, w in m["markets"].items()), m["seats"]]
                   for m in tier_models])

        write_tsv(out / "trims.tsv",
                  "# Trims of each model: label, fuel, engine (cc, 0 for electric), power (kW), model years, "
                  "gearboxes (m manual, a automatic, s semi-automatic), drives (cars f, r, a; motorcycles c chain, "
                  "b belt, s shaft, h hub; the first is the usual one), weight and the markets that got it (empty: "
                  "all). Curated from the makers' specifications.",
                  ["model", "label", "fuel", "engine_cc", "power_kw", "from_year", "to_year", "transmissions",
                   "drives", "weight", "markets"],
                  [[m["key"], t["label"], t["fuel"], t["cc"], t["kw"], t["start"], t["end"] or "", t["trans"],
                    t["drives"], fmt(t["weight"]), ",".join(t["markets"])]
                   for m in tier_models for t in m["trims"]])

        write_tsv(out / "transmission.tsv",
                  "# Share of new cars with an automatic gearbox by model year (interpolated between rows), "
                  "curated from national sales statistics and makers' figures.",
                  ["country", "year", "automatic_share"],
                  [[c, y, fmt(v)] for c in countries for y, v in anchors(fleet[c]["automatic"])])

        for kind, name in (("car", "colours"), ("motorcycle", "motorcycle_colours")):
            write_tsv(out / f"{name}.tsv", comment(CURATED / f"{name}.tsv"), ["market", "year"] + FAMILIES,
                      [[r["market"], r["year"]] + [r[f] for f in FAMILIES] for r in colours[kind]])

        write_tsv(out / "colour_names.tsv",
                  comment(CURATED / "colour_names.tsv") + " kind: whether the name is a car or a motorcycle colour.",
                  ["make", "family", "name", "weight", "from_year", "to_year", "kind"],
                  [[r["make"], r["family"], r["name"], r["weight"], r["from"], r["to"], r["kind"]]
                   for r in colour_names if r["make"] == "*" or r["make"] in tier_makes[r["kind"]]])

        write_tsv(out / "mileage.tsv",
                  "# Average distance a car is driven a year (km; curated from Eurostat, ODYSSEE-MURE and national "
                  "travel surveys), how much more or less each fuel's cars are driven than petrol cars, and the "
                  "average distance a motorcycle is ridden a year (km, curated estimates).",
                  ["country", "annual_km"] + list(MILEAGE_FACTORS) + ["motorcycle_km"],
                  [[c, fleet[c]["annual_km"]] + [fmt(v) for v in MILEAGE_FACTORS.values()]
                   + [moto_fleet[c]["annual_km"]] for c in countries])

        tier_formats = [f for f in formats if f["country"] in countries]
        format_keys = {f["key"] for f in tier_formats}
        columns = ["key", "country", "region", "from", "until", "kind", "pattern", "letters", "regions", "fuels",
                   "min_age", "weight", "exclude", "max_chars", "reissue", "kinds"]
        write_tsv(out / "plate_formats.tsv", comment(CURATED / "plate_formats.tsv"), columns,
                  [[f[k] for k in columns] for f in tier_formats])
        write_tsv(out / "plate_series.tsv", comment(CURATED / "plate_series.tsv"), ["format", "date", "plate"],
                  [[s["format"], s["date"], s["plate"]] for s in series if s["format"] in format_keys])
        used_sets = {f["regions"] for f in tier_formats if f["regions"]} | \
            {f["country"] for f in tier_formats if f["region"] != "*"}
        write_tsv(out / "plate_regions.tsv", comment(CURATED / "plate_regions.tsv"),
                  ["set", "code", "label", "weight", "sub", "reserved"],
                  [[s, r["code"], r["label"], r["weight"], r["sub"], 1 if r["reserved"] else 0]
                   for s in sorted(used_sets) for r in region_sets[s]])

        for kind, name in (("car", "factories"), ("motorcycle", "motorcycle_factories")):
            write_tsv(out / f"{name}.tsv", comment(CURATED / f"{name}.tsv"),
                      ["make", "market", "country", "weight", "wmi", "wmi_suv", "wmi_truck", "plants", "vds"],
                      [[f[k] for k in ("make", "market", "country", "weight", "wmi", "wmi_suv", "wmi_truck",
                                       "plants", "vds")] for f in factories[kind] if f["make"] in tier_makes[kind]])
        write_tsv(out / "wmis.tsv",
                  "# World manufacturer identifiers no maker holds (checked against NHTSA vPIC DecodeWMI by "
                  "scripts/prepare_wmis.py), for safe VINs.",
                  ["wmi", "reserved"], [[w, 1] for w in RESERVED_WMIS])
        write_tsv(out / "owner_factors.tsv", comment(CURATED / "owner_factors.tsv"),
                  ["owner_age", "target", "factor"],
                  [[r["owner_age"], r["target"], r["factor"]] for r in owner_factors])

        counts = ", ".join(f"{sum(m['kind'] == k for m in tier_models)} {k} models" for k in KINDS)
        print(f"{tier}: {len(countries)} countries, {counts}, "
              f"{sum(len(m['trims']) for m in tier_models)} trims, {len(tier_formats)} plate formats")
    missing = sorted(set(tiers["full"]) - set(fleet))
    print(f"countries without fleet data (not drawn): {' '.join(missing) or 'none'}")
    print(f"Eurostat ages: {len(ages_eu)} countries; Eurostat fuels: {len(fuels_eu)}; "
          f"Eurostat motorcycles: {len(motorcycles_eu)}; OWID EV: {len(ev)}")


if __name__ == "__main__":
    main()
