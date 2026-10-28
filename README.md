# Vehicle Generator for C++

> **Requires C++23** (e.g., `-std=c++23` for GCC/Clang, `/std:c++latest` for MSVC).

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/vehicle-generator/main/LICENSE.MIT)
[![CI](https://github.com/dasmig/vehicle-generator/actions/workflows/ci.yml/badge.svg)](https://github.com/dasmig/vehicle-generator/actions/workflows/ci.yml)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/vehicle-generator.svg)](https://github.com/dasmig/vehicle-generator/releases)
[![C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/type-header--only-green.svg)](https://github.com/dasmig/vehicle-generator#integration)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue.svg)](https://dasmig.github.io/vehicle-generator/)

**[API Reference](https://dasmig.github.io/vehicle-generator/)** · **[Usage Guide](doc/usage.md)** · **[Data and Method](doc/data.md)** · **[Releases](https://github.com/dasmig/vehicle-generator/releases)**

Cars and motorcycles that fit their country and date: a make sold there and a model built in its model year, a fuel as common as it was among new vehicles of that kind that year, a trim, gearbox and drive the model was offered with, a colour of its era, an odometer reading that fits its age, a licence plate in the country's format of the registration date, and a VIN whose maker prefix, check digit and model-year code are right.

```
2019 Volkswagen Golf 1.5 TSI, petrol, silver, 54,300 km, B-KX 4821
2022 Toyota Prius 1.8 Hybrid, hybrid, white, 3,840 km, 福岡 331 な ・317
2013 Chevrolet Equinox 2.4, petrol, black, 92,800 mi, 7CBU948
2020 Honda BeAT, petrol, red, 25,480 km, 30-Y5 275.82
```

## Features

- **Every Country**. 195 countries in the `lite` tier and 242 in `full`, each with its own fleet age, fuel mix by model year, makes, models sold, automatic share, yearly mileage and plate format; no country falls back to an invented format.

- **Cars and Motorcycles**. Each country's motorcycles and scooters next to its cars, with their own fleet size, age, makes, fuel mix, colours and mileage: most vehicles drawn in Vietnam or Indonesia are scooters and underbones, a US motorcycle is most often a Harley-Davidson, an Indian one a Hero, a Honda or a TVS. Without a requested kind, the kind is drawn by the country's vehicles in use.

- **Coherent with the Date**. The model was built in the model year, the trim was offered with that fuel that year, the first registration falls between October of the year before and the end of the model year, the odometer grows with the years in use. A 2012 car in Norway is rarely electric, a 2024 one usually is.

- **Real Plates**. The format each country issued at the registration date: Spain's and Italy's sequential series move with the date, British age identifiers match the half-year, German plates carry their district with `E` for electric and `H` for historic cars, China's green NEV plates from 2017, Japanese plates in kanji and hiragana, the motorcycle series of Italy, Vietnam, Japan, Greece, Colombia and Argentina, all 50 US states and DC, Canadian provinces and Australian states.

- **Valid VINs**. ISO 3779: the WMI of the make and the country that built the vehicle for that market (a Golf sold in Mexico comes from Puebla, a Honda Wave sold in Vietnam from Vinh Phuc), model-consistent descriptor characters, the check digit, the model-year code and a plant code.

- **Safe Mode**. `safe = true` gives plates from series not yet issued, region codes no authority uses or letters the format never takes, and VINs whose WMI no manufacturer holds (checked against NHTSA vPIC).

- **Requests**. Country, kind (car or motorcycle), plate region, make, model, body, fuel, age range, owner's age, reference date (2000 to 2050), units; impossible combinations are rejected with their reason.

- **Deterministic**. A seed gives the same vehicle on every compiler and platform; batches, threads and streaming as in every dasmig library.

## Integration

[`vehiclegen.hpp`](https://github.com/dasmig/vehicle-generator/blob/main/dasmig/vehiclegen.hpp) is the library [released here](https://github.com/dasmig/vehicle-generator/releases). Keep [`core.hpp`](https://github.com/dasmig/vehicle-generator/blob/main/dasmig/core.hpp) and the [`core/`](https://github.com/dasmig/vehicle-generator/tree/main/dasmig/core) directory ([dasmig-core](https://github.com/dasmig/dasmig-core)) in the same directory. Add

```cpp
#include <dasmig/vehiclegen.hpp>
```

to the files you want to use it in, and supply the [`resources`](https://github.com/dasmig/vehicle-generator/tree/main/resources) folder (or define `DASMIG_VEHICLE_EMBED`).

## Usage

```cpp
#include <dasmig/vehiclegen.hpp>
#include <iostream>

auto& gen = dasmig::vhg::instance(); // loads the lite tier

auto v = gen.get_vehicle(dasmig::vehicle_request{});
std::cout << v << '\n';           // 2019 Volkswagen Golf 1.5 TSI, petrol, silver, 54,300 km, B-KX 4821
std::cout << v.vin << '\n';       // WVWZZZJY9KH761457
std::cout << v.to_json() << '\n'; // {"country_code":"DE","make":"Volkswagen",...}

dasmig::vehicle_request r;
r.country = "US";
r.region = "CA";
r.fuel = dasmig::vehicle_fuel::electric;
r.safe = true;                    // plate and VIN nobody holds
auto ev = gen.get_vehicle(r, 42); // the same seed gives the same result everywhere

dasmig::vehicle_request bike;
bike.country = "IT";
bike.kind = dasmig::vehicle_kind::motorcycle; // or a motorcycle body or model
auto m = gen.get_vehicle(bike);               // 2005 KTM 640 Duke, petrol, grey, 70,960 km, MW 62480
```

See the [usage guide](doc/usage.md) for everything else.

## Building

```bash
make            # example
make test       # tests
make bench      # benchmarks, results in build/bench.tsv
make test-embed # tests with the lite tier compiled in
make coverage   # code coverage
make lint       # clang-tidy
make docs       # API docs
make embed      # regenerate dasmig/vehiclegen_data.hpp from resources/lite
```

## Compiler Support

Tested with:
- Clang 18+ (`-std=c++23`)
- GCC 14+ (`-std=c++23`)
- MSVC 19.38+ (`/std:c++latest`)
- Emscripten 3.1.74

## Dependencies

| Dependency | Version | Bundled | Purpose |
|-----------|---------|---------|---------|
| [dasmig-core](https://github.com/dasmig/dasmig-core) | 1.3 | Yes (`core.hpp`, `core/`) | Platform-independent random engine and distributions, data layers, JSON |
| [Catch2](https://github.com/catchorg/Catch2) | 3.x | Yes (amalgamated) | Unit testing |

## Data

The data is curated for this library from public sources (national registration rules and statistics, motorcycle industry associations, makers' specifications, paint makers' colour reports) and derived from Eurostat, Our World in Data / IEA (CC BY 4.0), NHTSA vPIC and country-generator; see [doc/data.md](doc/data.md) and [LICENSE_DATA.txt](LICENSE_DATA.txt). Plates and VINs are realistic by design; use `safe` where a real one must never appear.

## License

This library is released under the [MIT License](https://opensource.org/licenses/MIT).

```
MIT License

Copyright (c) 2026 Diego Dasso Migotto
```
