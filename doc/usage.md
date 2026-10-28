# Usage Guide

This guide covers every feature of the vehicle-generator library. For a quick overview, see the [README](../README.md). For the full API reference, run `doxygen Doxyfile` from the repository root and open `doc/api/html/index.html`.

[TOC]

<!-- GitHub-rendered TOC (Doxygen uses [TOC] above instead) -->
<ul>
<li><a href="#quick-start">Quick Start</a></li>
<li><a href="#installation">Installation</a></li>
<li><a href="#loading-resources">Loading Resources</a></li>
<li><a href="#generating">Generating</a></li>
<li><a href="#seeding-and-deterministic-generation">Seeding and Deterministic Generation</a></li>
<li><a href="#extending-data">Extending Data</a></li>
<li><a href="#replay-and-json">Replay and JSON</a></li>
<li><a href="#compiled-in-data">Compiled-in Data</a></li>
<li><a href="#thread-safety">Thread Safety</a></li>
<li><a href="#error-reference">Error Reference</a></li>
</ul>

## Quick Start

```cpp
#include <dasmig/vehiclegen.hpp>
#include <iostream>

int main()
{
    auto& gen = dasmig::vhg::instance();
    dasmig::vehicle_request request;
    request.country = "DE";
    auto v = gen.get_vehicle(request);
    std::cout << v << '\n'; // 2019 Volkswagen Golf 1.5 TSI, petrol, silver, 54,300 km, B-KX 4821
    std::cout << v.vin << '\n';
}
```

## Installation

Copy `dasmig/vehiclegen.hpp`, `dasmig/core.hpp` and the `dasmig/core/` directory into your include path, keeping them together. Supply `resources/` next to the program, or define `DASMIG_VEHICLE_EMBED` to compile the `lite` tier in.

## Loading Resources

`instance()` loads the `lite` tier on first use (or `full` if there is no `lite`), from the compiled-in copy or from the first of `resources/`, `../resources`, `vehicle-generator/resources` and `../vehicle-generator/resources` that has it.

```cpp
dasmig::vhg gen;                       // an empty, independent instance
bool ok = gen.load(dasmig::dataset::lite); // false when the tier is not found
gen.load(std::filesystem::path{"path/to/resources/lite"}); // a directory
gen.has_data();
gen.countries();                       // where vehicles can be drawn
```

Each `load` replaces the data and every layer loaded before.

## Generating

`vehicle_request` says what to generate; every field is optional.

| Field | Meaning |
| --- | --- |
| `country` | ISO 3166-1 alpha-2, any case. Unset: drawn by passenger cars in use. |
| `region` | A plate region of the country (`plate_regions(country)`): a US state, Canadian province or Australian state (which picks that jurisdiction's plates), or a district or province code where plates carry one (`"M"` for Munich). |
| `make`, `model` | A make as `makes()` lists it, or a model key (`models(make)`), which implies its make. |
| `body`, `fuel` | `vehicle_body` and `vehicle_fuel` values. |
| `min_age`, `max_age` | Completed years since the model year at the reference date. |
| `owner_age` | The owner's age; younger owners have older cars and more hatchbacks, older owners newer cars and more sedans. |
| `reference_date` | The day the vehicle is seen, from 2000-01-01 to 2050-12-31 (default 2023-07-01). |
| `units` | `distance_units::country` (miles in the US, UK, Liberia and Myanmar), `metric` or `imperial`. |
| `safe` | Plate and VIN from series nobody holds; `reserved` is then set. |
| `weighted` | `false` draws uniformly among what is possible instead of by the data's shares. |

```cpp
dasmig::vehicle_request r;
r.country = "US";
r.region = "CA";
r.fuel = dasmig::vehicle_fuel::electric;
r.max_age = 3;
r.reference_date = std::chrono::year_month_day{std::chrono::year{2025}, std::chrono::month{1}, std::chrono::day{1}};
auto v = gen.get_vehicle(r);
```

The result:

| Field | Example |
| --- | --- |
| `country_code`, `make`, `model`, `model_key`, `trim` | `"DE"`, `"Volkswagen"`, `"Golf"`, `"vw-golf"`, `"1.5 TSI"` |
| `body`, `fuel` | `vehicle_body::hatchback`, `vehicle_fuel::petrol` (`to_string()` gives their names) |
| `model_year`, `first_registration` | `2019`, `2019-04-11` |
| `engine_cc`, `power_kw` | `1498`, `96` (`engine_cc` is 0 for electric cars) |
| `transmission`, `drive` | `"manual"` or `"automatic"`; `"fwd"`, `"rwd"` or `"awd"` |
| `doors`, `seats` | `5`, `5` |
| `colour`, `colour_family` | `"Reflex Silver"`, `"silver"` |
| `odometer`, `odometer_unit` | `54300`, `"km"` |
| `plate`, `plate_region`, `plate_format` | `"B-KX 4821"`, `"B"`, `"de"` |
| `vin`, `wmi` | `"WVWZZZCDZKW123456"`, `"WVW"` |
| `reserved` | `true` for safe plates and VINs |

Queries: `countries()`, `makes(country)` (sold there; every make without a country), `models(make)` (keys), `plate_regions(country)`.

Japanese and Chinese plates are UTF-8 in their scripts (`福岡 331 な ・317`, `京A·12345`). The [data guide](data.md) explains every draw.

## Seeding and Deterministic Generation

- `get_vehicle(request, seed)` gives the same result for the same request, seed and data, on every platform, without advancing the instance engine.
- `get_vehicle(request)` draws a fresh seed from the instance engine, initially seeded from system entropy. There are no generator-level `seed()` or `unseed()` controls; for deterministic sequences, keep a `dasmig::core::rng` in the caller and pass its next value as the seed.
- `v.seed()` is the seed a result was generated from: `get_vehicle(dasmig::vehicle_request{}, v.seed())` replays it with the same request and data.

## Extending Data

The shipped data is a base layer. Files with the columns of one of the tables (see `vehicle_table` and the [data guide](data.md)) can be loaded on top of it to add rows, change them or remove them, without editing the shipped files. Each table is keyed by its key columns (`key` for models and plate formats, `country` and `make` for make shares, ...).

```cpp
// A file is matched to its table by its name; a directory may hold several.
gen.extend("mods/models.tsv");
gen.extend("mods/");

// From text, for example supplied by a WebAssembly host.
gen.extend_from_string(dasmig::vehicle_table::make_shares,
                       "country\tmake\tweight\nDE\tTesla\t8\n", "scenario");

// Remove rows: a layer needs only the key and a disabled column.
gen.extend_from_string(dasmig::vehicle_table::models, "key\tdisabled\nvw-golf\t1\n", "cleanup");
```

- Every value in a layer is checked before anything changes, and the tables are rebuilt together; a bad layer throws `dasmig::core::tsv_error` (a malformed row, with file, line and column) or `std::invalid_argument` (tables that no longer fit, such as a trim of an unknown model), and the data stays as it was.
- `layers(table)` lists what is loaded for a table.
- `load(...)` replaces the base and drops every layer.

## Replay and JSON

Every result records the seed and the data it came from:

```cpp
auto v = gen.get_vehicle(dasmig::vehicle_request{});
v.signature().seed; // the call seed
v.signature().data; // gen.data_hash() when it was generated
```

`to_json()` gives the result as JSON, the form the WebAssembly layer and the game consume. Seeds are strings because JavaScript numbers cannot hold every 64-bit value. `to_json(writer)` writes into an existing `dasmig::core::json_writer`, for composing larger documents.

## Compiled-in Data

Define `DASMIG_VEHICLE_EMBED` before including the header to compile the `lite` tier into the program. `load(dataset::lite)` and `instance()` then use it and need no files. The compiled-in data has the same `data_hash()` as the file, so seeds replay the same either way.

`dasmig/vehiclegen_data.hpp` is generated from `resources/lite` by `make embed`; CI checks it is up to date.

## Thread Safety

Each `vhg` instance is independent. The seeded `get_vehicle(request, seed)` and the batch calls are const and may run concurrently. `get_vehicle(request)` advances the instance engine; concurrent calls to it on the **same** instance require external synchronisation (e.g. a `std::mutex`).

## Batch Generation

Every seeded getter has batch forms: `get_vehicle_batch` return a `std::vector`, and `for_each_vehicle` streams the items to a sink in index order without holding the batch.

```cpp
auto& gen = dasmig::vhg::instance();
const auto request = dasmig::vehicle_request{};

// 1,000 results from one batch seed, on four threads.
auto items = gen.get_vehicle_batch(request, {.count = 1000, .threads = 4}, 42);

// Item 1 replays on its own: the batch seed and the index give its seed.
auto again = gen.get_vehicle(request, dasmig::core::item_seed(42, 1));

// A million results to a file as JSON Lines, never all in memory.
std::ofstream out{"items.jsonl"};
dasmig::core::jsonl_writer lines{out};
gen.for_each_vehicle(request, {.count = 1'000'000, .threads = 0}, 42, lines);
```

- **Item seeds.** Item `i` is exactly `get_vehicle(request, core::item_seed(batch_seed, first + i))`. A range can be generated directly (`first = 1000` starts at item 1000), two batches that follow each other equal one, and the output is the same for any `threads` (0 means the hardware count). Without a `batch_seed` one is drawn like the single getter draws its own.
- **Options.** `core::batch_options` has `count`, `first`, `threads` and `unique`. The request describes one item; the options describe the batch.
- **Uniqueness.** `.unique = core::unique_rule::by<vehicle>(key)` regenerates an item whose key already appeared in the batch, with a deterministic retry seed, and throws `std::runtime_error` naming the item when the attempts run out.
- **Sinks.** `core::jsonl_writer` writes one `to_json()` per line, `core::csv_writer` flattens each result's JSON into dotted columns. Any callable `(std::uint64_t index, vehicle&)` works.
- Batch calls use the same const getters from several threads, which the thread-safety rules above allow; the CI runs the batch tests under ThreadSanitizer.

## Error Reference

| Exception | When |
|-----------|------|
| `std::runtime_error` | Any generation call before data is loaded |
| `std::invalid_argument` | A reference date outside 2000-01-01 to 2050-12-31 or invalid; an unknown country (`Unknown country code: XX`), make, model or plate region; `min_age` above `max_age`; a combination nobody sells in the country and age range (the message names it); a layer that leaves the tables inconsistent |
| `dasmig::core::tsv_error` | A malformed table or layer (message gives file, line and column); the data is unchanged |
