# Data and method

How vehicle-generator draws a vehicle, and where every table comes from. The tables are TSV files in `resources/lite` and `resources/full`, built by `scripts/prepare_data.py` from the curated tables in `scripts/curated/` and the downloads it fetches; each file's first line names its source.

## Tiers

| Tier | Countries | Car models (trims) | Motorcycle models (trims) | Plate formats | Use |
| --- | --- | --- | --- | --- | --- |
| `lite` | 195 (the sovereign states of country-generator's `lite`) | 154 (606), the best sellers | 104 (347) | 309 | compiled in with `DASMIG_VEHICLE_EMBED` |
| `full` | 242 (every inhabited territory) | 556 (1,517) | 187 (530) | 357 | files |

Every country of both tiers can draw a car and a motorcycle for every model year from 1990 to 2026; the build checks it.

## How a vehicle is drawn

Each step draws from its own stream split from the call seed, so a field fixed in the request leaves the other draws unchanged.

1. **Country** (`country`): by vehicles in use, `population × (cars_per_1000 + motorcycles_per_1000) / 1000` (or the requested kind's term alone), among the countries where the request is possible.
2. **Kind** (`kind`): car or motorcycle by the country's fleets in use, times the owner's factors, among the kinds the request allows. Every later step uses that kind's tables: the motorcycle fleet's ages, fuel mix, makes, colours and mileage.
3. **Plate jurisdiction** (`region`): in the United States, Canada and Australia, the state or province, by population.
4. **Age** (`age`): whole years since the model year, from the country's fleet by band (`ages.tsv`: uniform within the bands 0-1, 2-4, 5-9 and 10-19, a tail falling by 0.8 a year from 20 to 45), times the owner's factors, limited to model years from 1990 and to `min_age`/`max_age`, and weighted by the share of new cars of that model year the request can be (so a request for an electric car leans to recent years).
5. **First registration** (`registration`): a day from 1 October of the year before the model year to the end of the model year, and before the reference date.
6. **Fuel** (`fuel`): by the fuel mix of new cars of the model year in the country, among the fuels some make sold there offered that year.
7. **Make** (`make`): by `weight × (1 + trend)^(model year − 2025)` among the makes with a model offering that fuel that year.
8. **Model** (`model`): by its popularity in the country's market, halved in the first and last production year.
9. **Body** (`body`): by the model's body shares, times the owner's factors.
10. **Trim, gearbox, drive** (`trim`): a trim of that fuel offered in the model year and the market (LPG, CNG and flex-fuel cars may be converted petrol trims, labelled ` LPG`, ` CNG`, ` Flex`; motorcycles are never converted); automatic by the country's share for that model year where both are offered (hybrids and electric cars are automatic; a motorcycle offered with both, such as Honda's dual-clutch models, is automatic 40% of the time); the first drive listed 70% of the time.
11. **Colour** (`colour`): a family by the market's shares for the model year, then the maker's name for it.
12. **Odometer** (`odometer`): log-normal around `annual_km × fuel factor × years in use` (σ 0.35, 0.25 in the first year; 5 to 300 km under 30 days), in the country's unit unless the request says otherwise.
13. **Plate** (`plate`, `region`): the formats valid on the registration day for the country, the jurisdiction, the fuel and the age, by weight (see below).
14. **VIN** (`vin`): see below.

## Countries and fleets

| Table | Columns | Source |
| --- | --- | --- |
| `countries.tsv` | `country_code`, `population`, `cars_per_1000`, `mean_age`, `units`, `driving_side`, `markets`, `motorcycles_per_1000`, `motorcycle_mean_age` | population and driving side from country-generator; cars per 1,000 (OICA vehicles in use, the European Commission and national registers, the WHO road safety report), mean fleet age (ACEA, S&P Global Mobility, national registers) and markets curated; motorcycles per 1,000 from Eurostat `road_eqs_motorc` (the latest year since 2015, for the 39 countries it covers) and elsewhere curated from national registers, the WHO road safety report's two- and three-wheelers and the industry associations (ACEM, AISI, VAMM, ABRACICLO, SIAM); mean motorcycle age curated. Without the motorcycle columns a country has no motorcycles |
| `ages.tsv` | `country`, `band`, `share` | Eurostat `road_eqs_carage`, mean of the last three complete years, for the 32 countries it covers; elsewhere a yearly survival ratio fitted to `mean_age` |
| `fuel_shares.tsv` | `country`, `year`, one column per fuel | Eurostat `road_eqr_carpda` (new cars by motor energy, 2013 on) in Europe; Our World in Data / IEA Global EV Outlook (CC BY 4.0) for battery-electric and plug-in shares elsewhere; curated anchors (ACEA diesel shares, national statistics) before and between; petrol is the remainder. Years between rows interpolate |
| `transmission.tsv` | `country`, `year`, `automatic_share` | curated from national sales statistics |
| `motorcycle_fuel_shares.tsv` | as `fuel_shares.tsv` | curated (IEA Global EV Outlook two-wheelers, ACEM, India's VAHAN, ABRACICLO for Brazil's flex-fuel motorcycles); licensed electric motorcycles and scooters, not electric bicycles |
| `mileage.tsv` | `country`, `annual_km`, fuel factors, `motorcycle_km` | curated (Eurostat, ODYSSEE-MURE, national travel surveys); diesel 1.35, hybrid 1.1, plug-in 0.9, electric 0.85, LPG and CNG 1.3; `motorcycle_km` curated estimates (40% of `annual_km` when absent) |
| `owner_factors.tsv` | `owner_age`, `target`, `factor` | curated from household surveys (UK NTS, US NHTS, German MiD); `target` is `age:<band>`, `body:<body>` or `kind:<kind>` |

**Markets.** A country lists the model ranges sold there in order of preference: three-letter markets (`EUR` Europe, `CIS`, `NAM` North America, `LAM` Latin America, `MER` Mercosur, `CHN`, `JPN`, `JDM` used Japanese imports, `KOR`, `IND`, `SEA` South-East Asia, `MEA` Middle East, `AFR`, `OCE`) and two-letter countries where a model's sales differ (`TR`, `GB`, `MX`, `IR`, ...). A model's weight in a country is its entry for the country, else for the first of the country's markets it lists.

## Makes, models and trims

| Table | Columns | Source |
| --- | --- | --- |
| `make_shares.tsv` | `country`, `make`, `weight`, `trend` | curated from national registration statistics and public sales rankings; `weight` is the make's share of model year 2025, `trend` its yearly relative change |
| `motorcycle_make_shares.tsv` | as `make_shares.tsv` | curated from national registrations and the industry associations (ACEM, MCIA, SIAM, ABRACICLO, AISI, VAMM); countries without published figures share their region's mix |
| `models.tsv` | `key`, `kind`, `make`, `model`, `from_year`, `to_year`, `bodies`, `markets`, `seats` | curated (production years of the model name for its market, cross-checked with NHTSA vPIC for the US); `kind` is `car` (also when empty) or `motorcycle`, and the bodies must be of that kind |
| `trims.tsv` | `model`, `label`, `fuel`, `engine_cc`, `power_kw`, `from_year`, `to_year`, `transmissions`, `drives`, `weight`, `markets` | curated from the makers' specifications; gearboxes `m`, `a` and (motorcycles) `s` semi-automatic; drives `f`, `r`, `a` for cars, `c` chain, `b` belt, `s` shaft, `h` hub for motorcycles |

A model offers a fuel in a year when one of its trims of that fuel covers the year in the country's markets. Model and trim years are model years; a trim never outlives its model. A make may build both kinds (Honda, Suzuki, BMW, Peugeot); its models of each kind are drawn by the kind's make shares.

**Motorcycle bodies.** `scooter` (step-through with a CVT, from the Vespa to the Yamaha TMAX), `underbone` (step-through on a motorcycle frame with a semi-automatic gearbox: Honda Wave, Super Cub, Yamaha Jupiter), `standard` (commuters and naked bikes: Honda CG, Hero Splendor, Yamaha MT-07), `sport`, `touring`, `cruiser`, `adventure` and `offroad` (trail and enduro). Petrol mopeds of 50 cc and under are not drawn.

## Colours

`colours.tsv` gives the shares of eleven families (`white`, `black`, `grey`, `silver`, `blue`, `red`, `brown`, `green`, `yellow`, `orange`, `other`) by market at 1990, 2000, 2010, 2020 and 2025, interpolated between, from the paint makers' reports (Axalta, BASF, PPG; DuPont for the 1990s). A country uses its first market with rows, else `*`. `motorcycle_colours.tsv` has the same columns for motorcycles, curated from the makers' line-ups and dealer stock (the paint makers' reports cover cars only), for `*`, `SEA`, `IND` and `MER`. `colour_names.tsv` holds the makers' names with the model years they were offered and the `kind` they name (`car` when empty); makes without names use the generic ones of their kind (`*`).

## Plates

`plate_formats.tsv` has one row per format and era: `key`, `country`, `region` (a jurisdiction or `*`), `from`/`until` (registration dates), `kind` (`random`, `sequential`), `pattern`, `letters` (the alphabet, such as `A-Z!IOQ`), `regions` (the region set its `R` draws from), `fuels` (formats only for some fuels), `min_age`, `weight`, `exclude` (letter runs never issued), `max_chars` (characters without separators) `reissue` (yearly chance a car of an ended format got a new plate) and `kinds` (`car`, `motorcycle` or both separated by `;`; empty: every kind).

**Motorcycle plates.** A format restricted to a kind is only drawn for it, and where a kind's own general format (no `fuels`, no `min_age`) is valid on the registration day, the formats for every kind give way to it: Italian motorcycles registered from July 1999 get `LL DDDDD`, Vietnamese ones `29-B1 123.45`, Japanese ones their office and hiragana without a class number, Greek ones `RLL DDD`, Colombian ones `LLL DDL`, Argentine ones `DDD LLL` and from 2016 `LDDD LLL`. China's green NEV plates are for cars. Elsewhere motorcycles carry the country's general format.

**Pattern tokens.** `L` a letter of the alphabet, `D` a digit, `N` a digit 1-9, `X` a letter or digit, `R` the region code, `S` the region's office number (two digits), `C` its office letter, `A` the British age identifier, `Y` the British prefix-year letter, `V` the year of registration (two digits), `H` its half (1 or 2), `J` a Japanese serial (`12-34`, `・317`). `T^m` or `T^m-n` repeats a token; `[k:...]` is a sequential group of significance `k`; `{a|b}` is one of the literal options; `'text'` and `\c` are literal; any other character except `A`-`Z` is literal.

**Sequential systems** (Spain, Italy, France, the Netherlands, Belgium, Portugal, Argentina, Chile, California) map a plate to an index, the group characters as digits of a mixed radix. `plate_series.tsv` anchors the plate issued on dates, from public series trackers (to a few months); a car's plate is the index of its registration day, interpolated between anchors and continued at the last rate, ±7 days of issuance. The last anchor fixes the data's as-of date.

**Region codes.** `plate_regions.tsv` lists each set's codes with a label and a population weight, and codes no authority issues (`reserved`). German districts, Austrian districts, Swiss cantons, Polish, Czech, Russian, Ukrainian, Indian (with their RTO counts), Chinese (with their city letters) and Japanese registration offices, US states, Canadian provinces and Australian states, among others. `vehicle_request::region` takes a code of the country's sets; for formats without a region code it is ignored.

**Era changes.** France's FNI plates (until April 2009) are kept with a 15% yearly chance of a car moving to the SIV format, at a day drawn between then and the reference date.

**Validation.** `scripts/check_plates.py` (run by `prepare_data.py`) parses every pattern, checks the anchors fit their pattern and move forward, and that every country and jurisdiction has a format for every kind and registration date from October 1989. The tests check plates against regular expressions written from the national rules.

## VINs

`factories.tsv` (cars) and `motorcycle_factories.tsv` (motorcycles: NHTSA vPIC's motorcycle WMIs, with the Indian, Indonesian, Thai and Vietnamese plants from public VIN decoders) give, per make and market, the producing countries with weights, their WMIs (for cars, SUVs and pickups), the plant codes and whether the maker fills unused positions with `Z` (European makers). The VIN is the WMI; three characters from a hash of the model and trim (or `ZZZ` and two from the model); two random characters; the ISO 3779 check digit; the model-year code (`A`-`Y` for 1980-2000 and 2010-2030, `1`-`9` for 2001-2009); a plant code; a six-digit serial. VINs are structurally valid and model-consistent, not decodable to a trim.

`wmis.tsv` lists WMIs no maker holds; `scripts/prepare_wmis.py` checks each against NHTSA vPIC `DecodeWMI` and fails if one is held. Safe VINs use them.

## Safe mode

A safe plate is, in order: a reserved region code (unless the request names a region); a letter the format's alphabet never uses (`I`, `O` or `Q`, or when it takes all three, another capital it lacks); in a sequential series that takes every letter, a plate past the series' last one, where it ended or where it will be on 2050-12-31; a `0` where the format has a 1-9 digit; all digits zero. None of these depends on the reference date, so a safe plate stays unissued as the series move on. A safe VIN has a reserved WMI. `reserved` is set when both apply.

## Limits

- Model years from 1990; reference dates from 2000-01-01 to 2050-12-31. At early reference dates the fleet has no car older than 1990.
- No import modelling: a used Japanese import registered years later keeps a first registration near its model year.
- Plate series anchors age: dates after the last anchor extrapolate.
- Plate formats are each country's standard passenger plate; personalised, diplomatic and special plates are not drawn. Motorcycles have their own series only in the countries listed above: elsewhere (the US states among them) they carry the car format, though their real plates are often shorter. Italian motorcycles registered before July 1999 get car-style plates; Japanese motorcycles of 125 cc and under, which carry municipal plates, get the transport bureau's.
- Motorcycles: petrol mopeds of 50 cc and under, three-wheelers and quadricycles are not drawn. Makes without curated models (Italika, Gogoro, VinFast, Ola, Ather, AKT, Motomel, Modenas, the many Chinese assemblers beyond the largest) are left out, so their share goes to the makes listed. Japanese domestic motorcycles carry frame numbers, not VINs; they get VINs here. Motorcycle fleet sizes outside Europe come from registers that keep scrapped motorcycles for years, and are rounded.
- Curated tables are rounded; they reproduce the shape of each market, not its exact statistics.

## Rebuilding

```bash
python scripts/prepare_data.py   # downloads into raw_data/, writes resources/lite and resources/full (cars and motorcycles)
python scripts/prepare_wmis.py   # checks the reserved WMIs against NHTSA vPIC
make embed                       # regenerates dasmig/vehiclegen_data.hpp
```
