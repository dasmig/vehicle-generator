// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Diego Dasso Migotto

#ifndef DASMIG_VEHICLEGEN_HPP
#define DASMIG_VEHICLEGEN_HPP

#include "core.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef DASMIG_VEHICLE_EMBED
#include "vehiclegen_data.hpp"
#endif

/// @file vehiclegen.hpp
/// @brief Vehicle Generator library — cars and motorcycles with make, model, fuel, colour, mileage, licence plate
///        and VIN that fit their country and date for C++23.
/// @author Diego Dasso Migotto (diegomigotto at hotmail dot com)
/// @see See doc/usage.md for the narrative tutorial.

DASMIG_CORE_REQUIRE(1, 1);

namespace dasmig
{

/// @brief What kind of vehicle it is.
enum class vehicle_kind : std::uint8_t
{
    car,       ///< Passenger car (with SUVs, pickups and vans used as cars).
    motorcycle ///< Motorcycle or scooter on two wheels (petrol mopeds of 50 cc and under are not drawn).
};

/// @brief Body style of a vehicle. The first eight are cars' (see kind_of()),
///        the rest motorcycles'.
enum class vehicle_body : std::uint8_t
{
    hatchback,   ///< Hatchback.
    sedan,       ///< Sedan (saloon).
    wagon,       ///< Station wagon (estate).
    suv,         ///< Sport utility vehicle or crossover.
    pickup,      ///< Pickup truck used as a passenger car.
    van,         ///< Passenger van or MPV.
    coupe,       ///< Coupe.
    convertible, ///< Convertible.
    scooter,     ///< Scooter, with a step-through floor and an automatic gearbox.
    underbone,   ///< Step-through underbone (Super Cub, Wave), usually semi-automatic.
    standard,    ///< Standard or naked motorcycle, from commuters to roadsters.
    sport,       ///< Sport bike with a fairing.
    touring,     ///< Touring or sport-touring motorcycle.
    cruiser,     ///< Cruiser.
    adventure,   ///< Adventure or dual-sport motorcycle.
    offroad      ///< Trail, enduro or off-road motorcycle with road registration.
};

/// @brief The kind of vehicle a body style belongs to.
/// @param b The body.
/// @return vehicle_kind::motorcycle for scooter and the bodies after it.
[[nodiscard]] constexpr vehicle_kind kind_of(vehicle_body b) noexcept
{
    return b >= vehicle_body::scooter ? vehicle_kind::motorcycle : vehicle_kind::car;
}

/// @brief What a vehicle runs on.
enum class vehicle_fuel : std::uint8_t
{
    petrol,   ///< Petrol (gasoline).
    diesel,   ///< Diesel.
    hybrid,   ///< Hybrid that does not plug in.
    plug_in,  ///< Plug-in hybrid.
    electric, ///< Battery electric.
    lpg,      ///< Liquefied petroleum gas (usually a converted petrol car).
    cng,      ///< Compressed natural gas.
    flex      ///< Flexible fuel (ethanol and petrol).
};

/// @brief Unit of the odometer reading.
enum class distance_units : std::uint8_t
{
    country, ///< The unit the country uses (miles in the US, UK, Liberia, Myanmar and a few territories).
    metric,  ///< Kilometres.
    imperial ///< Miles.
};

/// @brief Name of a kind ("car", "motorcycle").
/// @param k The kind.
/// @return Its name, as in the data.
[[nodiscard]] constexpr std::string_view to_string(vehicle_kind k) noexcept
{
    constexpr std::array<std::string_view, 2> names{"car", "motorcycle"};
    return names.at(static_cast<std::size_t>(k));
}

/// @brief Name of a body style ("hatchback", "suv", "scooter", ...).
/// @param b The body.
/// @return Its name, as in the data.
[[nodiscard]] constexpr std::string_view to_string(vehicle_body b) noexcept
{
    constexpr std::array<std::string_view, 16> names{
        "hatchback", "sedan",    "wagon", "suv",     "pickup",  "van",     "coupe",     "convertible",
        "scooter",   "underbone", "standard", "sport", "touring", "cruiser", "adventure", "offroad"};
    return names.at(static_cast<std::size_t>(b));
}

/// @brief Name of a fuel ("petrol", "plug_in", ...).
/// @param f The fuel.
/// @return Its name, as in the data.
[[nodiscard]] constexpr std::string_view to_string(vehicle_fuel f) noexcept
{
    constexpr std::array<std::string_view, 8> names{"petrol",   "diesel", "hybrid", "plug_in",
                                                    "electric", "lpg",    "cng",    "flex"};
    return names.at(static_cast<std::size_t>(f));
}

/// @brief What a vehicle call asks for.
///
/// Every generation call takes one; `vehicle_request{}` asks for the
/// defaults. Choices are never stored on the generator, so one call cannot
/// change the next.
struct vehicle_request
{
    /// The reference date when none is given.
    static constexpr std::chrono::year_month_day default_reference_date{std::chrono::year{2023}, std::chrono::month{7},
                                                                        std::chrono::day{1}};
    /// The earliest reference date the data covers.
    static constexpr std::chrono::year_month_day earliest_reference_date{std::chrono::year{2000},
                                                                         std::chrono::month{1}, std::chrono::day{1}};
    /// The latest reference date the data covers.
    static constexpr std::chrono::year_month_day latest_reference_date{std::chrono::year{2050},
                                                                       std::chrono::month{12}, std::chrono::day{31}};

    std::optional<std::string> country{};  ///< ISO 3166-1 alpha-2, any case; unset: drawn by vehicles in use.
    std::optional<vehicle_kind> kind{};    ///< Car or motorcycle; unset: drawn by the country's vehicles in use.
    std::optional<std::string> region{};   ///< Plate region: a US state, German district, ... (see plate_regions()).
    std::optional<std::string> make{};     ///< A make, as makes() lists it ("Volkswagen").
    std::optional<std::string> model{};    ///< A model key ("vw-golf"); implies its make and kind.
    std::optional<vehicle_body> body{};    ///< Body style; implies its kind.
    std::optional<vehicle_fuel> fuel{};    ///< Fuel.
    std::optional<unsigned> min_age{};     ///< Youngest age, in completed years since the model year.
    std::optional<unsigned> max_age{};     ///< Oldest age.
    std::optional<unsigned> owner_age{};   ///< The owner's age (from a persona); shifts age and body mildly.
    std::chrono::year_month_day reference_date{default_reference_date}; ///< The day the vehicle is seen.
    distance_units units{distance_units::country}; ///< Odometer unit.
    bool safe{false};     ///< Plate and VIN from series nobody holds.
    bool weighted{true};  ///< Draw by the data's shares (true) or uniformly among what is possible.
};

/// @brief Return type for vehicle generation.
///
/// Supports implicit conversion to std::string (the summary), streaming via
/// operator<< and JSON output via to_json().
class vehicle
{
  public:
    std::string country_code; ///< ISO 3166-1 alpha-2.
    vehicle_kind kind{vehicle_kind::car}; ///< Car or motorcycle.
    std::string make;         ///< "Volkswagen".
    std::string model;        ///< "Golf".
    std::string model_key;    ///< "vw-golf".
    std::string trim;         ///< "1.5 TSI".
    vehicle_body body{vehicle_body::hatchback}; ///< Body style.
    vehicle_fuel fuel{vehicle_fuel::petrol};    ///< Fuel.
    int model_year{0};        ///< Model year.
    std::chrono::year_month_day first_registration{}; ///< First registration.
    unsigned engine_cc{0};    ///< Engine displacement; 0 for electric.
    unsigned power_kw{0};     ///< Power.
    std::string transmission; ///< "manual", "automatic" or (motorcycles) "semi-automatic".
    std::string drive;        ///< Cars: "fwd", "rwd" or "awd"; motorcycles' final drive: "chain", "belt", "shaft" or "hub".
    unsigned doors{0};        ///< Doors; 0 for motorcycles.
    unsigned seats{0};        ///< Seats (riders on a motorcycle).
    std::string colour;       ///< The maker's colour name ("Reflex Silver").
    std::string colour_family; ///< "silver".
    unsigned odometer{0};     ///< Odometer reading, rounded to 10.
    std::string odometer_unit; ///< "km" or "mi".
    std::string plate;        ///< Licence plate.
    std::string plate_region; ///< Region code on or behind the plate (US state, German district); may be empty.
    std::string plate_format; ///< Key of the plate format used.
    std::string vin;          ///< Vehicle identification number (ISO 3779).
    std::string wmi;          ///< World manufacturer identifier (VIN positions 1 to 3).
    bool reserved{false};     ///< Plate and VIN from series nobody holds (safe requests).

    /// @brief Retrieve the random seed used to generate this vehicle.
    /// @return The call seed; pass it to get_vehicle(request, seed) with
    ///         the same request and data to replay this vehicle.
    [[nodiscard]] std::uint64_t seed() const { return _signature.seed; }

    /// @brief The seed and the data this vehicle was generated from.
    /// @return The replay signature; `data` is vhg::data_hash() at the
    ///         time of generation.
    [[nodiscard]] const core::seed_signature& signature() const { return _signature; }

    /// @brief Human-readable summary string.
    /// @return For example
    ///   `2019 Volkswagen Golf 1.5 TSI, petrol, silver, 54,300 km, B-KX 4821`.
    ///   A motorcycle's trim usually names the model (`CBR600RR` of the
    ///   `CBR`); the summary then gives the trim alone:
    ///   `2019 Honda CBR600RR, petrol, red, 12,400 km, AB 12345`.
    [[nodiscard]] std::string to_string() const
    {
        std::string s = std::to_string(model_year) + " " + make;
        if (kind == vehicle_kind::car || !trim.contains(model)) s += " " + model;
        if (!trim.empty()) s += " " + trim;
        s += ", ";
        s += dasmig::to_string(fuel);
        if (!colour_family.empty()) s += ", " + colour_family;
        s += ", " + grouped_(odometer) + " " + odometer_unit;
        if (!plate.empty()) s += ", " + plate;
        return s;
    }

    /// @brief Write this vehicle as a JSON object.
    /// @param w Writer positioned where a value is allowed.
    void to_json(core::json_writer& w) const
    {
        w.begin_object()
            .field("country_code", country_code)
            .field("kind", dasmig::to_string(kind))
            .field("make", make)
            .field("model", model)
            .field("model_key", model_key)
            .field("trim", trim)
            .field("body", dasmig::to_string(body))
            .field("fuel", dasmig::to_string(fuel))
            .field("model_year", model_year)
            .field("first_registration", iso_date_(first_registration))
            .field("engine_cc", engine_cc)
            .field("power_kw", power_kw)
            .field("transmission", transmission)
            .field("drive", drive)
            .field("doors", doors)
            .field("seats", seats)
            .field("colour", colour)
            .field("colour_family", colour_family)
            .field("odometer", odometer)
            .field("odometer_unit", odometer_unit)
            .field("plate", plate)
            .field("plate_region", plate_region)
            .field("plate_format", plate_format)
            .field("vin", vin)
            .field("wmi", wmi)
            .field("reserved", reserved)
            .key("signature");
        _signature.to_json(w);
        w.end_object();
    }

    /// @brief This vehicle as a JSON document.
    /// @return Compact JSON, one object.
    [[nodiscard]] std::string to_json() const
    {
        core::json_writer w;
        to_json(w);
        return w.str();
    }

    /// @brief Implicit conversion to std::string.
    /// @return Summary string.
    operator std::string() const // NOLINT(hicpp-explicit-conversions)
    {
        return to_string();
    }

    /// @brief Stream the summary to an output stream.
    /// @param os Output stream.
    /// @param v The vehicle to stream.
    /// @return The stream.
    friend std::ostream& operator<<(std::ostream& os, const vehicle& v)
    {
        os << v.to_string();
        return os;
    }

  private:
    core::seed_signature _signature;
    friend class vhg;

    static std::string grouped_(unsigned n)
    {
        std::string digits = std::to_string(n);
        std::string out;
        for (std::size_t i = 0; i < digits.size(); ++i)
        {
            if (i > 0 && (digits.size() - i) % 3 == 0) out += ',';
            out += digits[i];
        }
        return out;
    }

    static std::string iso_date_(const std::chrono::year_month_day& d)
    {
        const auto two = [](unsigned v) { return std::string(v < 10 ? "0" : "") + std::to_string(v); };
        return std::to_string(static_cast<int>(d.year())) + "-" + two(static_cast<unsigned>(d.month())) + "-" +
               two(static_cast<unsigned>(d.day()));
    }
};

/// @brief The data tables of vehicle-generator. Each is a TSV file in a tier
///        directory and can be extended with layers (see doc/data.md).
enum class vehicle_table : std::uint8_t
{
    countries,     ///< `countries.tsv`: population, cars and motorcycles per 1,000, units, markets.
    ages,          ///< `ages.tsv`: car fleet share by age band.
    fuel_shares,   ///< `fuel_shares.tsv`: fuel mix of new cars by model year.
    make_shares,   ///< `make_shares.tsv`: car makes by country, with a yearly trend.
    models,        ///< `models.tsv`: car and motorcycle models, production years, bodies, markets.
    trims,         ///< `trims.tsv`: engines, power, gearboxes and drives of each model.
    transmission,  ///< `transmission.tsv`: automatic share of new cars by country and model year.
    colours,       ///< `colours.tsv`: car colour families by market and model year.
    colour_names,  ///< `colour_names.tsv`: the makers' colour names.
    mileage,       ///< `mileage.tsv`: yearly distance and fuel factors.
    plate_formats, ///< `plate_formats.tsv`: licence plate formats by country, date and kind.
    plate_series,  ///< `plate_series.tsv`: anchors of sequential plate systems.
    plate_regions, ///< `plate_regions.tsv`: region codes plates carry.
    wmis,          ///< `wmis.tsv`: world manufacturer identifiers nobody holds (for safe VINs).
    factories,     ///< `factories.tsv`: where each car make builds for each market, with WMIs.
    owner_factors, ///< `owner_factors.tsv`: how the owner's age shifts kind, age and body.
    motorcycle_fuel_shares, ///< `motorcycle_fuel_shares.tsv`: fuel mix of new motorcycles (columns of fuel_shares).
    motorcycle_make_shares, ///< `motorcycle_make_shares.tsv`: motorcycle makes by country (columns of make_shares).
    motorcycle_colours,     ///< `motorcycle_colours.tsv`: motorcycle colour families (columns of colours).
    motorcycle_factories    ///< `motorcycle_factories.tsv`: where motorcycle makes build (columns of factories).
};

/// @brief Vehicle Generator: cars and motorcycles whose make, model, fuel,
///        colour, mileage, licence plate and VIN fit their country and date.
///
/// A call draws, from streams split from the call seed: the country
/// (`"country"`), the kind (`"kind"`), the plate jurisdiction or region (`"region"`), the age
/// (`"age"`), the first registration (`"registration"`), the fuel
/// (`"fuel"`), the make (`"make"`), the model (`"model"`), the body
/// (`"body"`), the trim, gearbox and drive (`"trim"`), the colour
/// (`"colour"`), the odometer (`"odometer"`), the plate (`"plate"`) and the
/// VIN (`"vin"`). Fixing one in the request leaves the others' draws
/// unchanged. Output is identical on every compiler and platform
/// (dasmig-core).
///
/// Can be used as a singleton via instance() or constructed independently.
///
/// @par Thread safety
/// Each instance is independent. The seeded overloads are const and may run
/// concurrently; the unseeded ones advance the instance engine and need
/// external synchronisation.
class vhg
{
  public:
    /// @brief Default constructor — creates an empty generator with no data.
    vhg() = default;

    vhg(const vhg&) = delete;                 ///< Not copyable.
    vhg& operator=(const vhg&) = delete;      ///< Not copyable.
    vhg(vhg&&) noexcept = default;            ///< Movable.
    vhg& operator=(vhg&&) noexcept = default; ///< Movable.
    ~vhg() = default;                         ///< Destructor.

    /// @brief Access the global singleton instance.
    ///
    /// Loads the `lite` tier (or `full` if `lite` is missing) on first
    /// access, from compiled-in data or an auto-probed `resources/`.
    /// @return The singleton.
    static vhg& instance()
    {
        static vhg inst{auto_probe_tag{}};
        return inst;
    }

    // -- Generation -------------------------------------------------------

    /// @brief Generate a random vehicle. Use vehicle_request{} for the
    ///        defaults.
    /// @param request What to generate.
    /// @return The vehicle.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If the request is invalid or asks for
    ///         a combination nobody sells.
    [[nodiscard]] vehicle get_vehicle(const vehicle_request& request) { return get_vehicle(request, draw_seed_()); }

    /// @brief Generate a deterministic vehicle.
    /// @param request What to generate.
    /// @param call_seed Seed; the same request, seed and data give the same
    ///        result. Does not touch the instance engine.
    /// @return The vehicle.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If the request is invalid or asks for
    ///         a combination nobody sells.
    [[nodiscard]] vehicle get_vehicle(const vehicle_request& request, std::uint64_t call_seed) const
    {
        require_data_();
        const auto ctx = resolve_(request);
        const core::rng call{call_seed};

        vehicle v;
        v._signature = {.seed = call_seed, .data = data_hash()};

        const country_rec& c = ctx.country ? *ctx.country : draw_country_(ctx, request, call);
        v.country_code = c.code;
        if (request.region && !has_region_(c, *request.region))
            throw std::invalid_argument("vehicle-generator: " + *request.region + " is not a plate region of " + c.code);
        v.kind = draw_kind_(c, ctx, request, call);
        const fleet_rec& fl = c.fleet(v.kind);
        const std::string jurisdiction = draw_jurisdiction_(c, request, call);

        const auto supply = supply_(fl, ctx);
        const int age = draw_age_(c, fl, ctx, supply, request, call);
        const int model_year = ctx.year - age;
        v.model_year = model_year;
        const int registered = draw_registration_(model_year, ctx.day, call);
        v.first_registration = ymd_(registered);

        const vehicle_fuel fuel = draw_fuel_(fl, ctx, supply, model_year, request, call);
        v.fuel = fuel;
        const make_entry& mk = draw_make_(fl, ctx, fuel, model_year, request, call);
        v.make = mk.make;
        const model_rec& mdl = _model.models[draw_model_(mk, ctx, fuel, model_year, request, call)];
        v.model = mdl.name;
        v.model_key = mdl.key;
        v.body = draw_body_(mdl, ctx, request, call);
        v.doors = doors_(v.body);
        v.seats = mdl.seats;
        apply_trim_(v, c, mdl, fuel, model_year, request, call);
        apply_colour_(v, fl, model_year, request, call);
        apply_odometer_(v, c, fl, registered, ctx.day, request, call);
        apply_plate_(v, c, jurisdiction, age, registered, ctx.day, request, call);
        apply_vin_(v, c, mdl, request, call);
        return v;
    }

    // -- Batch generation -------------------------------------------------

    /// @brief Generate a batch of vehicles from a seed drawn like
    ///        get_vehicle() draws its own.
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @return The results, in index order.
    [[nodiscard]] std::vector<vehicle> get_vehicle_batch(const vehicle_request& request,
                                                         const core::batch_options& options)
    {
        return get_vehicle_batch(request, options, draw_seed_());
    }

    /// @brief Generate a batch of vehicles, deterministically.
    ///
    /// Item `i` is exactly `get_vehicle(request, core::item_seed(batch_seed,
    /// options.first + i))`, so an item replays on its own, a range can be
    /// generated directly, two batches that follow each other equal one, and
    /// the output does not depend on `options.threads`.
    ///
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness (see
    ///        core::unique_rule; the rule must be made for `vehicle`).
    /// @param batch_seed Seed of the whole batch.
    /// @return The results, in index order.
    /// @throws std::invalid_argument If the request or options are invalid.
    /// @throws std::runtime_error If no data has been loaded, or a unique
    ///         batch runs out of attempts.
    [[nodiscard]] std::vector<vehicle> get_vehicle_batch(const vehicle_request& request,
                                                         const core::batch_options& options,
                                                         std::uint64_t batch_seed) const
    {
        return core::run_batch(options, batch_seed,
                               [&](std::uint64_t item_seed) { return get_vehicle(request, item_seed); });
    }

    /// @brief Stream a batch of vehicles to a sink without holding the whole
    ///        batch.
    ///
    /// Generates the same items as get_vehicle_batch() and calls
    /// `sink(index, item)` in index order on the calling thread; `index`
    /// includes `options.first`.
    ///
    /// @tparam Sink Callable taking `(std::uint64_t, vehicle&)`; for example
    ///         core::jsonl_writer or core::csv_writer.
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @param batch_seed Seed of the whole batch.
    /// @param sink Receives the items.
    template <typename Sink>
    void for_each_vehicle(const vehicle_request& request, const core::batch_options& options,
                          std::uint64_t batch_seed, Sink&& sink) const
    {
        core::for_each_batch(
            options, batch_seed, [&](std::uint64_t item_seed) { return get_vehicle(request, item_seed); },
            std::forward<Sink>(sink));
    }

    /// @brief Stream a batch of vehicles with a batch seed drawn like
    ///        get_vehicle() draws its own.
    /// @tparam Sink Callable taking `(std::uint64_t, vehicle&)`.
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @param sink Receives the items.
    template <typename Sink>
    void for_each_vehicle(const vehicle_request& request, const core::batch_options& options, Sink&& sink)
    {
        for_each_vehicle(request, options, draw_seed_(), std::forward<Sink>(sink));
    }

    // -- Queries ----------------------------------------------------------

    /// @brief The countries vehicles can be drawn in.
    /// @param kind Only countries with vehicles of this kind; unset for any.
    /// @return Alpha-2 codes, in data order.
    [[nodiscard]] std::vector<std::string> countries(std::optional<vehicle_kind> kind = {}) const
    {
        std::vector<std::string> out;
        for (const auto& c : _model.countries)
            if (kind ? c.fleet(*kind).drawable : c.drawable) out.push_back(c.code);
        return out;
    }

    /// @brief The makes sold in a country, or every make.
    /// @param country Alpha-2 code (any case); empty for every make.
    /// @param kind Only makes of this kind; unset for both.
    /// @return Make names: in the country's data order (cars first), or sorted for all.
    /// @throws std::invalid_argument If the country is unknown.
    [[nodiscard]] std::vector<std::string> makes(std::string_view country = {},
                                                 std::optional<vehicle_kind> kind = {}) const
    {
        std::vector<std::string> out;
        if (country.empty())
        {
            for (const auto& name : _model.make_names)
                if (!kind || std::ranges::any_of(_model.models, [&](const model_rec& m) {
                        return m.make == name && m.kind == *kind;
                    }))
                    out.push_back(name);
            return out;
        }
        const auto& c = country_or_throw_(country);
        for (const auto k : kinds_)
        {
            if (kind && *kind != k) continue;
            for (const auto& m : c.fleet(k).makes)
                if (m.weight > 0.0 && !m.models.empty() && std::ranges::find(out, m.make) == out.end())
                    out.push_back(m.make);
        }
        return out;
    }

    /// @brief The model keys of a make.
    /// @param make Make name, as makes() lists it.
    /// @param kind Only models of this kind; unset for both.
    /// @return Model keys in data order; empty for an unknown make.
    [[nodiscard]] std::vector<std::string> models(std::string_view make, std::optional<vehicle_kind> kind = {}) const
    {
        std::vector<std::string> out;
        for (const auto& m : _model.models)
            if (m.make == make && (!kind || m.kind == *kind)) out.push_back(m.key);
        return out;
    }

    /// @brief The region codes a country's plates carry (US states, German
    ///        districts, ...), which vehicle_request::region accepts.
    /// @param country Alpha-2 code (any case).
    /// @return Codes in data order; empty where plates carry none.
    /// @throws std::invalid_argument If the country is unknown.
    [[nodiscard]] std::vector<std::string> plate_regions(std::string_view country) const
    {
        const auto& c = country_or_throw_(country);
        std::vector<std::string> out;
        const auto add_set = [&](const std::string& name) {
            const auto it = _model.region_sets.find(name);
            if (it == _model.region_sets.end()) return;
            for (const auto& r : it->second.codes)
                if (!r.reserved && std::ranges::find(out, r.code) == out.end()) out.push_back(r.code);
        };
        if (!c.jurisdictions.empty())
        {
            add_set(c.jurisdictions);
            return out;
        }
        for (const auto i : c.formats) add_set(_model.formats[i].regions);
        return out;
    }

    // -- Data management --------------------------------------------------

    /// @brief Check whether any data has been loaded.
    /// @return True when countries, models and plate formats are loaded.
    [[nodiscard]] bool has_data() const
    {
        return !_model.countries.empty() && !_model.models.empty() && !_model.formats.empty();
    }

    /// @brief Load data from a resource directory, replacing all data and
    ///        layers loaded before.
    ///
    /// The directory must contain `countries.tsv` and `models.tsv`; the
    /// other tables are read when present. A path without them is ignored.
    ///
    /// @param dir Directory such as `resources/lite`.
    /// @throws core::tsv_error If a file is malformed; the data is then
    ///         unchanged.
    void load(const std::filesystem::path& dir)
    {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec) ||
            !std::filesystem::is_regular_file(dir / file_name_(table::countries), ec) ||
            !std::filesystem::is_regular_file(dir / file_name_(table::models), ec))
        {
            return;
        }
        apply_(read_directory_(dir), core::layer_mode::replace, true);
    }

    /// @brief Load a dataset tier, replacing all data and layers loaded
    ///        before.
    ///
    /// Uses the compiled-in copy when the library is built with
    /// `DASMIG_VEHICLE_EMBED`, otherwise searches `resources/`,
    /// `../resources`, `vehicle-generator/resources` and
    /// `../vehicle-generator/resources`.
    ///
    /// @param tier The tier.
    /// @return False when the tier was not found.
    /// @throws core::tsv_error If a file is malformed.
    [[nodiscard]] bool load(dataset tier)
    {
        const std::string tier_dir{core::tier_name(tier)};
        const auto marker = tier_dir + "/" + std::string{file_name_(table::countries)};
        const auto root = core::find_resources(library_name_, marker);
        documents docs;
        for (const auto t : all_tables_)
        {
            docs.at(index_(t)) =
                core::load_resource(tier_dir + "/" + std::string{file_name_(t)}, embedded_files_(), root);
        }
        if (!docs.at(index_(table::countries)) || !docs.at(index_(table::models))) return false;
        apply_(std::move(docs), core::layer_mode::replace, true);
        return true;
    }

    /// @brief Add data layers from a directory or a file.
    ///
    /// A directory may hold any of the table files (see vehicle_table); all
    /// are applied together or, if any fails, none. A file must have one of
    /// those names.
    ///
    /// @param path Directory or file.
    /// @param mode How the layers combine with the data loaded before.
    /// @throws core::tsv_error If a file cannot be read or a row is
    ///         invalid; the data is then unchanged.
    /// @throws std::invalid_argument If @p path is a file with another name,
    ///         or the tables do not fit together.
    void extend(const std::filesystem::path& path, core::layer_mode mode = core::layer_mode::override)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec))
        {
            apply_(read_directory_(path), mode, false);
            return;
        }
        for (const auto t : all_tables_)
        {
            if (path.filename() == file_name_(t))
            {
                (void)extend(t, core::tsv_document::load(path), mode);
                return;
            }
        }
        throw std::invalid_argument("extend: expected one of the vehicle tables (countries.tsv, models.tsv, "
                                    "trims.tsv, plate_formats.tsv, ...), got " +
                                    path.filename().string());
    }

    /// @brief Add a data layer to one table.
    ///
    /// Rows are keyed by the table's key columns (see doc/data.md). In the
    /// default override mode a row replaces the loaded row with the same key
    /// and new keys are added; a row with `disabled` set to 1 removes its key.
    ///
    /// @param which The table.
    /// @param doc TSV with that table's columns.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did (rows added, replaced, removed, warnings).
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    /// @throws std::invalid_argument If the tables no longer fit together.
    const core::layer_info& extend(vehicle_table which, core::tsv_document doc,
                                   core::layer_mode mode = core::layer_mode::override)
    {
        documents docs;
        docs.at(index_(which)).emplace(std::move(doc));
        apply_(std::move(docs), mode, false);
        return _tables.at(index_(which)).layers().back();
    }

    /// @brief Add a data layer to one table from TSV text (for example from
    ///        a WebAssembly host).
    /// @param which The table.
    /// @param text TSV with that table's columns.
    /// @param source Name used in errors.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did.
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    const core::layer_info& extend_from_string(vehicle_table which, std::string_view text, std::string source,
                                               core::layer_mode mode = core::layer_mode::override)
    {
        return extend(which, core::tsv_document::parse(text, std::move(source)), mode);
    }

    /// @brief The layers in effect for one table, in order.
    /// @param which The table.
    /// @return The layer records.
    [[nodiscard]] const std::vector<core::layer_info>& layers(vehicle_table which) const
    {
        return _tables.at(index_(which)).layers();
    }

    /// @brief Hash of the loaded data, recorded in each result's signature().
    /// @return A hash that changes when any table does; 0 when nothing is
    ///         loaded.
    [[nodiscard]] std::uint64_t data_hash() const
    {
        std::uint64_t h = 0;
        bool any = false;
        for (const auto& t : _tables)
        {
            any = any || !t.layers().empty();
            h = core::hash_combine(h, t.data_hash());
        }
        return any ? h : 0;
    }

  private:
    using table = vehicle_table;

    // -- Tables -----------------------------------------------------------

    static constexpr std::string_view library_name_ = "vehicle-generator";
    static constexpr std::size_t table_count_ = 20;
    static constexpr std::array<table, table_count_> all_tables_{
        table::countries,    table::ages,          table::fuel_shares,  table::make_shares,
        table::models,       table::trims,         table::transmission, table::colours,
        table::colour_names, table::mileage,       table::plate_formats, table::plate_series,
        table::plate_regions, table::wmis,         table::factories,    table::owner_factors,
        table::motorcycle_fuel_shares, table::motorcycle_make_shares, table::motorcycle_colours,
        table::motorcycle_factories};

    using documents = std::array<std::optional<core::tsv_document>, table_count_>;
    using tables = std::array<core::layered_table, table_count_>;

    static constexpr std::size_t index_(table t) { return static_cast<std::size_t>(t); }

    static constexpr std::string_view file_name_(table t)
    {
        constexpr std::array<std::string_view, table_count_> names{
            "countries.tsv",    "ages.tsv",          "fuel_shares.tsv",  "make_shares.tsv",
            "models.tsv",       "trims.tsv",         "transmission.tsv", "colours.tsv",
            "colour_names.tsv", "mileage.tsv",       "plate_formats.tsv", "plate_series.tsv",
            "plate_regions.tsv", "wmis.tsv",         "factories.tsv",    "owner_factors.tsv",
            "motorcycle_fuel_shares.tsv", "motorcycle_make_shares.tsv", "motorcycle_colours.tsv",
            "motorcycle_factories.tsv"};
        return names.at(index_(t));
    }

    // The car table a motorcycle table has the columns of; any other table itself.
    static constexpr table columns_of_(table t)
    {
        switch (t)
        {
        case table::motorcycle_fuel_shares: return table::fuel_shares;
        case table::motorcycle_make_shares: return table::make_shares;
        case table::motorcycle_colours: return table::colours;
        case table::motorcycle_factories: return table::factories;
        default: return t;
        }
    }

    static core::table_schema schema_(table t)
    {
        switch (columns_of_(t))
        {
        case table::countries:
            return {.key = {"country_code"},
                    .required = {"population", "cars_per_1000", "mean_age", "units", "driving_side", "markets"},
                    .optional = {"motorcycles_per_1000", "motorcycle_mean_age"}};
        case table::ages: return {.key = {"country", "band"}, .required = {"share"}, .optional = {}};
        case table::fuel_shares:
            return {.key = {"country", "year"},
                    .required = {"petrol", "diesel", "hybrid", "plug_in", "electric", "lpg", "cng", "flex"},
                    .optional = {}};
        case table::make_shares: return {.key = {"country", "make"}, .required = {"weight"}, .optional = {"trend"}};
        case table::models:
            return {.key = {"key"},
                    .required = {"make", "model", "from_year", "bodies", "markets", "seats"},
                    .optional = {"to_year", "kind"}};
        case table::trims:
            return {.key = {"model", "label", "fuel", "from_year"},
                    .required = {"engine_cc", "power_kw", "transmissions", "drives", "weight"},
                    .optional = {"to_year", "markets"}};
        case table::transmission:
            return {.key = {"country", "year"}, .required = {"automatic_share"}, .optional = {}};
        case table::colours:
            return {.key = {"market", "year"},
                    .required = {"white", "black", "grey", "silver", "blue", "red", "brown", "green", "yellow",
                                 "orange", "other"},
                    .optional = {}};
        case table::colour_names:
            return {.key = {"make", "family", "name"},
                    .required = {"weight"},
                    .optional = {"from_year", "to_year", "kind"}};
        case table::mileage:
            return {.key = {"country"},
                    .required = {"annual_km"},
                    .optional = {"diesel", "hybrid", "plug_in", "electric", "lpg", "cng", "flex", "motorcycle_km"}};
        case table::plate_formats:
            return {.key = {"key"},
                    .required = {"country", "region", "kind", "pattern", "letters"},
                    .optional = {"from", "until", "regions", "fuels", "min_age", "weight", "exclude", "max_chars",
                                 "reissue", "kinds"}};
        case table::plate_series: return {.key = {"format", "date"}, .required = {"plate"}, .optional = {}};
        case table::plate_regions:
            return {.key = {"set", "code"}, .required = {"label", "weight"}, .optional = {"sub", "reserved"}};
        case table::wmis: return {.key = {"wmi"}, .required = {"reserved"}, .optional = {}};
        case table::factories:
            return {.key = {"make", "market", "country"},
                    .required = {"weight", "wmi", "plants", "vds"},
                    .optional = {"wmi_suv", "wmi_truck"}};
        case table::owner_factors: return {.key = {"owner_age", "target"}, .required = {"factor"}, .optional = {}};
        default: break;
        }
        return {};
    }

    static tables make_tables_()
    {
        return {core::layered_table{schema_(table::countries)},    core::layered_table{schema_(table::ages)},
                core::layered_table{schema_(table::fuel_shares)},  core::layered_table{schema_(table::make_shares)},
                core::layered_table{schema_(table::models)},       core::layered_table{schema_(table::trims)},
                core::layered_table{schema_(table::transmission)}, core::layered_table{schema_(table::colours)},
                core::layered_table{schema_(table::colour_names)}, core::layered_table{schema_(table::mileage)},
                core::layered_table{schema_(table::plate_formats)}, core::layered_table{schema_(table::plate_series)},
                core::layered_table{schema_(table::plate_regions)}, core::layered_table{schema_(table::wmis)},
                core::layered_table{schema_(table::factories)},    core::layered_table{schema_(table::owner_factors)},
                core::layered_table{schema_(table::motorcycle_fuel_shares)},
                core::layered_table{schema_(table::motorcycle_make_shares)},
                core::layered_table{schema_(table::motorcycle_colours)},
                core::layered_table{schema_(table::motorcycle_factories)}};
    }

    // The kind-specific tables of a kind.
    struct kind_tables
    {
        table fuel_shares, make_shares, colours, factories;
    };

    static constexpr kind_tables tables_of_(vehicle_kind k)
    {
        if (k == vehicle_kind::motorcycle)
            return {.fuel_shares = table::motorcycle_fuel_shares,
                    .make_shares = table::motorcycle_make_shares,
                    .colours = table::motorcycle_colours,
                    .factories = table::motorcycle_factories};
        return {.fuel_shares = table::fuel_shares,
                .make_shares = table::make_shares,
                .colours = table::colours,
                .factories = table::factories};
    }

    // -- Constants ----------------------------------------------------------

    static constexpr int first_year_ = 1990;   // the first model year
    static constexpr int year_slots_ = 64;     // model years 1990..2053 in masks and per-year tables
    static constexpr int max_age_ = 45;        // oldest age drawn (the 20+ band's tail)
    static constexpr int trend_year_ = 2025;   // make weights are those of model year 2025
    static constexpr std::size_t fuel_count_ = 8;
    static constexpr std::size_t kind_count_ = 2;
    static constexpr std::size_t body_count_ = 16;
    static constexpr std::size_t family_count_ = 11;
    static constexpr std::size_t band_count_ = 5;
    static constexpr double motorcycle_km_share_ = 0.4;  // a motorcycle's yearly distance where mileage.tsv has none
    static constexpr double dual_clutch_share_ = 0.4;    // motorcycles bought with the automatic where both are offered
    static constexpr std::array<vehicle_kind, kind_count_> kinds_{vehicle_kind::car, vehicle_kind::motorcycle};
    static constexpr std::array<std::string_view, family_count_> families_{
        "white", "black", "grey", "silver", "blue", "red", "brown", "green", "yellow", "orange", "other"};
    static constexpr std::array<std::string_view, band_count_> bands_{"0-1", "2-4", "5-9", "10-19", "20+"};
    static constexpr std::string_view vin_chars_ = "0123456789ABCDEFGHJKLMNPRSTUVWXYZ";

    using year_mask = std::uint64_t;
    using fuel_masks = std::array<year_mask, fuel_count_>;

    static constexpr int slot_(int year) { return std::clamp(year - first_year_, 0, year_slots_ - 1); }
    static constexpr year_mask bit_(int year) { return year_mask{1} << static_cast<unsigned>(slot_(year)); }

    static year_mask span_mask_(int from, int to)
    {
        year_mask m = 0;
        for (int y = std::max(from, first_year_); y <= std::min(to, first_year_ + year_slots_ - 1); ++y) m |= bit_(y);
        return m;
    }

    static constexpr std::size_t band_of_(int age)
    {
        if (age <= 1) return 0;
        if (age <= 4) return 1;
        if (age <= 9) return 2;
        if (age <= 19) return 3;
        return 4;
    }

    // -- Model ----------------------------------------------------------------

    struct trim_rec
    {
        std::string label;
        vehicle_fuel fuel{};
        unsigned cc{0};
        unsigned kw{0};
        int from{0};
        int to{std::numeric_limits<int>::max()};
        bool manual{false};
        bool automatic{false};
        bool semi{false};  // semi-automatic (motorcycles)
        std::string drives;
        double weight{1.0};
        std::vector<std::string> markets;
    };

    struct model_rec
    {
        std::string key, make, name;
        vehicle_kind kind{vehicle_kind::car};
        int from{0};
        int to{std::numeric_limits<int>::max()};
        std::vector<std::pair<vehicle_body, double>> bodies;
        std::vector<std::pair<std::string, double>> markets;
        unsigned seats{5};
        std::vector<trim_rec> trims;
    };

    struct model_entry
    {
        std::size_t model{0};
        double weight{0.0};
        fuel_masks years{};
    };

    struct make_entry
    {
        std::string make;
        double weight{0.0};
        double growth{0.0};
        std::vector<model_entry> models;
    };

    struct region_rec
    {
        std::string code, label;
        double weight{0.0};
        unsigned sub{0};
        bool reserved{false};
    };

    struct region_set
    {
        std::vector<region_rec> codes;
        std::vector<double> weights;  // zero for reserved codes
        std::vector<std::size_t> reserved;
    };

    struct plate_item
    {
        char kind{'l'};   // 't' token, 'l' literal, 'c' choice
        char token{0};
        std::string text;
        std::vector<std::string> options;
        unsigned lo{1}, hi{1};
        unsigned group{0};
    };

    struct plate_format
    {
        std::string key, country, region, regions;
        int from{std::numeric_limits<int>::min()};
        int until{std::numeric_limits<int>::max()};
        bool sequential{false};
        std::vector<plate_item> items;
        std::vector<std::string> letters;
        unsigned fuels{0};  // bit per fuel; 0: every fuel
        int min_age{0};
        double weight{1.0};
        std::vector<std::string> exclude;
        std::size_t max_chars{0};
        double reissue{0.0};
        unsigned kinds{0};  // bit per kind; 0: every kind
        std::vector<std::pair<std::size_t, std::uint64_t>> layout;  // item, radix; most significant first
        double capacity{0.0};
        std::vector<std::pair<int, double>> anchors;  // day, index
    };

    struct factory_rec
    {
        std::string market, country;
        double weight{0.0};
        std::string wmi, wmi_suv, wmi_truck, plants;
        bool zzz{false};
    };

    struct colour_name_rec
    {
        std::string name;
        double weight{0.0};
        int from{0};
        int to{std::numeric_limits<int>::max()};
    };

    using colour_names_by_family = std::array<std::vector<colour_name_rec>, family_count_>;
    using family_weights = std::array<double, family_count_>;

    struct owner_band
    {
        unsigned lo{0}, hi{0};
        std::array<double, kind_count_> kind{1.0, 1.0};
        std::array<double, band_count_> age{1.0, 1.0, 1.0, 1.0, 1.0};
        std::array<double, body_count_> body{1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
                                             1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    };

    // One kind's vehicles in a country.
    struct fleet_rec
    {
        double size{0.0};  // vehicles in use
        std::array<double, max_age_ + 1> ages{};
        std::vector<std::array<double, fuel_count_>> fuels;  // by year slot
        std::vector<double> automatic;                       // by year slot (cars)
        double annual_km{12000.0};
        std::vector<make_entry> makes;
        fuel_masks supply{};    // model years some make offers each fuel in
        fuel_masks positive{};  // model years each fuel has a positive share
        std::size_t colours{npos_};
        bool drawable{false};
    };

    struct country_rec
    {
        std::string code;
        bool miles{false};
        std::vector<std::string> markets;
        std::array<double, fuel_count_> mileage{1.0, 1.35, 1.1, 0.9, 0.85, 1.3, 1.3, 1.0};
        std::array<fleet_rec, kind_count_> fleets;
        std::vector<std::size_t> formats;
        std::string jurisdictions;  // region set of the formats' regions (US states), or empty
        bool drawable{false};       // some kind is

        [[nodiscard]] const fleet_rec& fleet(vehicle_kind k) const { return fleets.at(static_cast<std::size_t>(k)); }
    };

    static constexpr std::size_t npos_ = std::numeric_limits<std::size_t>::max();

    struct colour_table
    {
        std::vector<std::string> markets;
        std::vector<std::vector<family_weights>> weights;  // by market, by year slot
    };

    struct model
    {
        std::vector<country_rec> countries;
        std::unordered_map<std::string, std::size_t> country_index;
        std::vector<model_rec> models;
        std::unordered_map<std::string, std::size_t> model_index;
        std::vector<std::string> make_names;  // sorted
        std::vector<plate_format> formats;
        std::unordered_map<std::string, region_set> region_sets;
        std::array<std::unordered_map<std::string, std::vector<factory_rec>>, kind_count_> factories;  // by make
        std::vector<std::string> reserved_wmis;
        std::array<colour_table, kind_count_> colours;
        std::array<std::unordered_map<std::string, colour_names_by_family>, kind_count_> colour_names;
        std::vector<owner_band> owners;
    };

    // -- Parsing ----------------------------------------------------------

    static std::string text_(const core::tsv_row& row, std::string_view column)
    {
        return row.has(column) ? std::string{row[column]} : std::string{};
    }

    static std::string nonempty_(const core::tsv_row& row, std::string_view column)
    {
        auto text = row.get<std::string>(column);
        if (text.empty()) throw row.error(column, "must not be empty");
        return text;
    }

    template <typename T>
    static std::optional<T> optional_(const core::tsv_row& row, std::string_view column)
    {
        if (text_(row, column).empty()) return std::nullopt;
        return row.get<T>(column);
    }

    static double non_negative_(const core::tsv_row& row, std::string_view column)
    {
        const double v = row.get<double>(column);
        if (!(v >= 0.0)) throw row.error(column, "must not be negative");
        return v;
    }

    static std::string alpha2_(const core::tsv_row& row, std::string_view column)
    {
        auto c = row.get<std::string>(column);
        if (!core::is_alpha2(c)) throw row.error(column, "must be an upper case ISO 3166-1 alpha-2 code");
        return c;
    }

    static std::vector<std::string> split_(std::string_view text, char separator)
    {
        std::vector<std::string> out;
        if (text.empty()) return out;
        for (;;)
        {
            const auto pos = text.find(separator);
            out.emplace_back(text.substr(0, pos));
            if (pos == std::string_view::npos) break;
            text.remove_prefix(pos + 1);
        }
        return out;
    }

    static std::optional<double> number_(std::string_view text)
    {
        if (text.empty()) return std::nullopt;
        double sign = 1.0;
        if (text.front() == '-')
        {
            sign = -1.0;
            text.remove_prefix(1);
        }
        double v = 0.0;
        double scale = 0.0;
        bool digit = false;
        for (const char c : text)
        {
            if (c == '.' && scale == 0.0)
            {
                scale = 1.0;
                continue;
            }
            if (c < '0' || c > '9') return std::nullopt;
            digit = true;
            if (scale > 0.0)
            {
                scale /= 10.0;
                v += (c - '0') * scale;
            }
            else
            {
                v = (v * 10.0) + (c - '0');
            }
        }
        if (!digit) return std::nullopt;
        return sign * v;
    }

    // "hatchback:85,wagon:15" -> pairs; nullopt when malformed.
    static std::optional<std::vector<std::pair<std::string, double>>> weighted_list_(std::string_view text)
    {
        std::vector<std::pair<std::string, double>> out;
        for (const auto& part : split_(text, ','))
        {
            const auto colon = part.find(':');
            const std::string name = part.substr(0, colon);
            double w = 1.0;
            if (colon != std::string::npos)
            {
                const auto v = number_(std::string_view{part}.substr(colon + 1));
                if (!v || *v < 0.0) return std::nullopt;
                w = *v;
            }
            if (name.empty()) return std::nullopt;
            out.emplace_back(name, w);
        }
        if (out.empty()) return std::nullopt;
        return out;
    }

    static std::optional<vehicle_body> body_of_(std::string_view text)
    {
        for (std::size_t i = 0; i < body_count_; ++i)
            if (dasmig::to_string(static_cast<vehicle_body>(i)) == text) return static_cast<vehicle_body>(i);
        return std::nullopt;
    }

    static std::optional<vehicle_kind> kind_named_(std::string_view text)
    {
        for (const auto k : kinds_)
            if (dasmig::to_string(k) == text) return k;
        return std::nullopt;
    }

    // An optional kind column: the default kind when empty.
    static vehicle_kind kind_column_(const core::tsv_row& row)
    {
        const auto text = text_(row, "kind");
        return text.empty() ? vehicle_kind::car : *kind_named_(text);
    }

    static constexpr unsigned kind_bit_(vehicle_kind k) { return 1U << static_cast<unsigned>(k); }

    static std::optional<vehicle_fuel> fuel_of_(std::string_view text)
    {
        for (std::size_t i = 0; i < fuel_count_; ++i)
            if (dasmig::to_string(static_cast<vehicle_fuel>(i)) == text) return static_cast<vehicle_fuel>(i);
        return std::nullopt;
    }

    static std::optional<std::size_t> family_of_(std::string_view text)
    {
        for (std::size_t i = 0; i < family_count_; ++i)
            if (families_.at(i) == text) return i;
        return std::nullopt;
    }

    static std::optional<std::size_t> band_index_(std::string_view text)
    {
        for (std::size_t i = 0; i < band_count_; ++i)
            if (bands_.at(i) == text) return i;
        return std::nullopt;
    }

    // Days since 1970-01-01 of an ISO 8601 date, or nullopt.
    static std::optional<int> parse_day_(std::string_view text)
    {
        if (text.size() != 10 || text[4] != '-' || text[7] != '-') return std::nullopt;
        const auto num = [&](std::size_t at, std::size_t n) -> std::optional<int> {
            int v = 0;
            for (std::size_t i = at; i < at + n; ++i)
            {
                if (text[i] < '0' || text[i] > '9') return std::nullopt;
                v = (v * 10) + (text[i] - '0');
            }
            return v;
        };
        const auto y = num(0, 4);
        const auto mo = num(5, 2);
        const auto da = num(8, 2);
        if (!y || !mo || !da) return std::nullopt;
        const std::chrono::year_month_day ymd{std::chrono::year{*y}, std::chrono::month{static_cast<unsigned>(*mo)},
                                              std::chrono::day{static_cast<unsigned>(*da)}};
        if (!ymd.ok()) return std::nullopt;
        return static_cast<int>(std::chrono::sys_days{ymd}.time_since_epoch().count());
    }

    static int day_of_(int y, unsigned m, unsigned d)
    {
        return static_cast<int>(
            std::chrono::sys_days{std::chrono::year_month_day{std::chrono::year{y}, std::chrono::month{m},
                                                              std::chrono::day{d}}}
                .time_since_epoch()
                .count());
    }

    static std::chrono::year_month_day ymd_(int day)
    {
        return std::chrono::year_month_day{std::chrono::sys_days{std::chrono::days{day}}};
    }

    static bool vin_char_(char c) { return vin_chars_.find(c) != std::string_view::npos; }

    // UTF-8 code points of a string.
    static std::vector<std::string> code_points_(std::string_view text)
    {
        std::vector<std::string> out;
        std::size_t i = 0;
        while (i < text.size())
        {
            const auto b = static_cast<unsigned char>(text[i]);
            std::size_t len = 1;
            if (b >= 0xF0U) len = 4;
            else if (b >= 0xE0U) len = 3;
            else if (b >= 0xC0U) len = 2;
            len = std::min(len, text.size() - i);
            out.emplace_back(text.substr(i, len));
            i += len;
        }
        return out;
    }

    // An alphabet such as "A-Z!IOQ": ASCII ranges and single code points, then the ones to drop.
    static std::vector<std::string> letters_(std::string_view spec)
    {
        const auto bang = spec.find('!');
        const auto keep = code_points_(spec.substr(0, bang));
        const auto drop = bang == std::string_view::npos ? std::vector<std::string>{}
                                                         : code_points_(spec.substr(bang + 1));
        std::vector<std::string> out;
        const auto add = [&](const std::string& s) {
            if (std::ranges::find(out, s) == out.end() && std::ranges::find(drop, s) == drop.end()) out.push_back(s);
        };
        for (std::size_t i = 0; i < keep.size(); ++i)
        {
            if (i + 2 < keep.size() && keep[i + 1] == "-" && keep[i].size() == 1 && keep[i + 2].size() == 1)
            {
                for (char c = keep[i][0]; c <= keep[i + 2][0]; ++c) add(std::string(1, c));
                i += 2;
            }
            else
            {
                add(keep[i]);
            }
        }
        return out;
    }

    // Parse a plate pattern (doc/data.md); throws std::invalid_argument with the reason.
    static std::vector<plate_item> parse_pattern_(std::string_view p, unsigned group = 0)
    {
        constexpr std::string_view tokens = "LDNXRSCAYVHJ";
        std::vector<plate_item> items;
        std::size_t i = 0;
        const auto fail = [](const std::string& why) { return std::invalid_argument("pattern: " + why); };
        const auto read_int = [&](std::size_t& at) {
            unsigned v = 0;
            const std::size_t start = at;
            while (at < p.size() && p[at] >= '0' && p[at] <= '9') v = (v * 10) + static_cast<unsigned>(p[at++] - '0');
            if (at == start) throw fail("expected a number");
            return v;
        };
        while (i < p.size())
        {
            const char c = p[i];
            plate_item item;
            item.group = group;
            if (tokens.find(c) != std::string_view::npos)
            {
                item.kind = 't';
                item.token = c;
                ++i;
            }
            else if (c == '[')
            {
                const auto close = p.find(']', i);
                const auto colon = p.find(':', i);
                if (group != 0 || close == std::string_view::npos || colon == std::string_view::npos || colon > close)
                    throw fail("bad sequential group");
                std::size_t at = i + 1;
                const unsigned significance = read_int(at);
                if (at != colon || significance == 0) throw fail("bad group significance");
                for (auto& inner : parse_pattern_(p.substr(colon + 1, close - colon - 1), significance))
                {
                    if (inner.kind != 't' || std::string_view{"LDN"}.find(inner.token) == std::string_view::npos ||
                        inner.lo != 1 || inner.hi != 1)
                        throw fail("a sequential group holds only single L, D and N");
                    items.push_back(std::move(inner));
                }
                i = close + 1;
                continue;
            }
            else if (c == '{')
            {
                const auto close = p.find('}', i);
                if (close == std::string_view::npos) throw fail("unclosed {");
                item.kind = 'c';
                item.options = split_(p.substr(i + 1, close - i - 1), '|');
                i = close + 1;
            }
            else if (c == '\'')
            {
                const auto close = p.find('\'', i + 1);
                if (close == std::string_view::npos) throw fail("unclosed quote");
                item.text = std::string{p.substr(i + 1, close - i - 1)};
                i = close + 1;
            }
            else if (c == '\\')
            {
                if (i + 1 >= p.size()) throw fail("dangling escape");
                item.text = std::string(1, p[i + 1]);
                i += 2;
            }
            else if (c == '^')
            {
                if (items.empty() || items.back().kind != 't' || items.back().group != group)
                    throw fail("^ must follow a token");
                ++i;
                const unsigned lo = read_int(i);
                unsigned hi = lo;
                if (i + 1 < p.size() && p[i] == '-' && p[i + 1] >= '0' && p[i + 1] <= '9')
                {
                    ++i;
                    hi = read_int(i);
                }
                if (hi < lo) throw fail("repeat range reversed");
                items.back().lo = lo;
                items.back().hi = hi;
                continue;
            }
            else if (c >= 'A' && c <= 'Z')
            {
                throw fail(std::string{"letter "} + c + " is not a token; quote it");
            }
            else
            {
                const auto cp = code_points_(p.substr(i, 4)).front();
                item.text = cp;
                i += cp.size();
            }
            items.push_back(std::move(item));
        }
        return items;
    }

    // -- Row validation -----------------------------------------------------

    static void check_years_(const core::tsv_row& row, std::string_view from, std::string_view to)
    {
        const int a = row.get<int>(from);
        if (a < 1900 || a > 2100) throw row.error(from, "must be a year from 1900 to 2100");
        if (const auto b = optional_<int>(row, to); b && *b < a) throw row.error(to, "must not be before " +
                                                                                       std::string{from});
    }

    static void check_kind_(const core::tsv_row& row)
    {
        if (const auto k = text_(row, "kind"); !k.empty() && !kind_named_(k))
            throw row.error("kind", "must be car or motorcycle");
    }

    static void validate_(table t, const core::tsv_row& row)
    {
        switch (columns_of_(t))
        {
        case table::countries:
        {
            (void)alpha2_(row, "country_code");
            (void)non_negative_(row, "population");
            (void)non_negative_(row, "cars_per_1000");
            if (!(row.get<double>("mean_age") > 0.0)) throw row.error("mean_age", "must be positive");
            if (const auto m = optional_<double>(row, "motorcycles_per_1000"); m && !(*m >= 0.0))
                throw row.error("motorcycles_per_1000", "must not be negative");
            if (const auto a = optional_<double>(row, "motorcycle_mean_age"); a && !(*a > 0.0))
                throw row.error("motorcycle_mean_age", "must be positive");
            const auto units = row.get<std::string>("units");
            if (units != "km" && units != "mi") throw row.error("units", "must be km or mi");
            const auto side = row.get<std::string>("driving_side");
            if (side != "left" && side != "right") throw row.error("driving_side", "must be left or right");
            (void)nonempty_(row, "markets");
            break;
        }
        case table::ages:
            (void)alpha2_(row, "country");
            if (!band_index_(row.get<std::string>("band"))) throw row.error("band", "must be 0-1, 2-4, 5-9, 10-19 or 20+");
            (void)non_negative_(row, "share");
            break;
        case table::fuel_shares:
            (void)alpha2_(row, "country");
            check_years_(row, "year", "year");
            for (std::size_t f = 0; f < fuel_count_; ++f) (void)non_negative_(row, dasmig::to_string(vehicle_fuel(f)));
            break;
        case table::make_shares:
            (void)alpha2_(row, "country");
            (void)nonempty_(row, "make");
            (void)non_negative_(row, "weight");
            if (const auto trend = optional_<double>(row, "trend"); trend && !(*trend > -1.0))
                throw row.error("trend", "must be above -1");
            break;
        case table::models:
        {
            (void)nonempty_(row, "key");
            (void)nonempty_(row, "make");
            (void)nonempty_(row, "model");
            check_years_(row, "from_year", "to_year");
            check_kind_(row);
            const auto bodies = weighted_list_(row.get<std::string>("bodies"));
            if (!bodies || std::ranges::any_of(*bodies, [](const auto& b) { return !body_of_(b.first); }))
                throw row.error("bodies", "must list bodies as body:weight, separated by commas");
            const auto kind = kind_column_(row);
            if (std::ranges::any_of(*bodies, [&](const auto& b) { return kind_of(*body_of_(b.first)) != kind; }))
                throw row.error("bodies",
                                "must be bodies of the model's kind (" + std::string{dasmig::to_string(kind)} + ")");
            if (!weighted_list_(row.get<std::string>("markets")))
                throw row.error("markets", "must list markets as market:weight, separated by commas");
            if (row.get<int>("seats") <= 0) throw row.error("seats", "must be positive");
            break;
        }
        case table::trims:
        {
            (void)nonempty_(row, "model");
            (void)nonempty_(row, "label");
            if (!fuel_of_(row.get<std::string>("fuel"))) throw row.error("fuel", "unknown fuel");
            check_years_(row, "from_year", "to_year");
            if (row.get<int>("engine_cc") < 0) throw row.error("engine_cc", "must not be negative");
            if (row.get<int>("power_kw") < 0) throw row.error("power_kw", "must not be negative");
            const auto trans = row.get<std::string>("transmissions");
            if (trans.find_first_not_of("mas") != std::string::npos)
                throw row.error("transmissions", "must be letters of m, a and s");
            const auto drives = row.get<std::string>("drives");
            if (drives.find_first_not_of("fra") != std::string::npos && drives.find_first_not_of("cbsh") != std::string::npos)
                throw row.error("drives", "must be letters of f, r and a (cars) or of c, b, s and h (motorcycles)");
            (void)non_negative_(row, "weight");
            break;
        }
        case table::transmission:
        {
            (void)alpha2_(row, "country");
            check_years_(row, "year", "year");
            const double s = row.get<double>("automatic_share");
            if (!(s >= 0.0 && s <= 1.0)) throw row.error("automatic_share", "must be from 0 to 1");
            break;
        }
        case table::colours:
            (void)nonempty_(row, "market");
            check_years_(row, "year", "year");
            for (const auto f : families_) (void)non_negative_(row, f);
            break;
        case table::colour_names:
            (void)nonempty_(row, "make");
            if (!family_of_(row.get<std::string>("family"))) throw row.error("family", "unknown colour family");
            (void)nonempty_(row, "name");
            (void)non_negative_(row, "weight");
            check_kind_(row);
            if (const auto from = optional_<int>(row, "from_year"))
                if (const auto to = optional_<int>(row, "to_year"); to && *to < *from)
                    throw row.error("to_year", "must not be before from_year");
            break;
        case table::mileage:
            (void)alpha2_(row, "country");
            if (!(row.get<double>("annual_km") > 0.0)) throw row.error("annual_km", "must be positive");
            for (const auto f : {"diesel", "hybrid", "plug_in", "electric", "lpg", "cng", "flex", "motorcycle_km"})
                if (const auto v = optional_<double>(row, f); v && !(*v > 0.0)) throw row.error(f, "must be positive");
            break;
        case table::plate_formats:
        {
            (void)nonempty_(row, "key");
            (void)alpha2_(row, "country");
            (void)nonempty_(row, "region");
            const auto kind = row.get<std::string>("kind");
            if (kind != "random" && kind != "sequential") throw row.error("kind", "must be random or sequential");
            try
            {
                (void)parse_pattern_(row.get<std::string>("pattern"));
            }
            catch (const std::invalid_argument& e)
            {
                throw row.error("pattern", e.what());
            }
            if (letters_(row.get<std::string>("letters")).empty()) throw row.error("letters", "must not be empty");
            for (const auto col : {"from", "until"})
                if (const auto t2 = text_(row, col); !t2.empty() && !parse_day_(t2))
                    throw row.error(col, "must be a date as YYYY-MM-DD");
            for (const auto& f : split_(text_(row, "fuels"), ';'))
                if (!fuel_of_(f)) throw row.error("fuels", "unknown fuel " + f);
            for (const auto& k : split_(text_(row, "kinds"), ';'))
                if (!kind_named_(k)) throw row.error("kinds", "unknown kind " + k);
            if (const auto w = optional_<double>(row, "weight"); w && !(*w >= 0.0))
                throw row.error("weight", "must not be negative");
            if (const auto r = optional_<double>(row, "reissue"); r && !(*r >= 0.0 && *r <= 1.0))
                throw row.error("reissue", "must be from 0 to 1");
            if (const auto a = optional_<int>(row, "min_age"); a && *a < 0) throw row.error("min_age", "must not be negative");
            if (const auto m = optional_<int>(row, "max_chars"); m && *m <= 0) throw row.error("max_chars", "must be positive");
            break;
        }
        case table::plate_series:
            (void)nonempty_(row, "format");
            if (!parse_day_(row.get<std::string>("date"))) throw row.error("date", "must be a date as YYYY-MM-DD");
            (void)nonempty_(row, "plate");
            break;
        case table::plate_regions:
            (void)nonempty_(row, "set");
            (void)nonempty_(row, "code");
            (void)non_negative_(row, "weight");
            if (const auto s = optional_<int>(row, "sub"); s && *s < 0) throw row.error("sub", "must not be negative");
            (void)row.get_or<bool>("reserved", false);
            break;
        case table::wmis:
        {
            const auto w = row.get<std::string>("wmi");
            if (w.size() != 3 || !std::ranges::all_of(w, vin_char_))
                throw row.error("wmi", "must be three VIN characters (0-9, A-Z without I, O, Q)");
            (void)row.get<bool>("reserved");
            break;
        }
        case table::factories:
        {
            (void)nonempty_(row, "make");
            (void)nonempty_(row, "market");
            (void)alpha2_(row, "country");
            (void)non_negative_(row, "weight");
            for (const auto col : {"wmi", "wmi_suv", "wmi_truck"})
            {
                const auto w = text_(row, col);
                if ((!w.empty() || std::string_view{col} == "wmi") &&
                    (w.size() != 3 || !std::ranges::all_of(w, vin_char_)))
                    throw row.error(col, "must be three VIN characters (0-9, A-Z without I, O, Q)");
            }
            const auto plants = nonempty_(row, "plants");
            if (!std::ranges::all_of(plants, vin_char_)) throw row.error("plants", "must be VIN characters");
            const auto vds = row.get<std::string>("vds");
            if (vds != "zzz" && vds != "random") throw row.error("vds", "must be zzz or random");
            break;
        }
        case table::owner_factors:
        {
            if (!owner_band_of_(row.get<std::string>("owner_age")))
                throw row.error("owner_age", "must be an age range such as 18-24 or 65-");
            const auto target = row.get<std::string>("target");
            const bool ok = (target.starts_with("age:") && band_index_(std::string_view{target}.substr(4))) ||
                            (target.starts_with("body:") && body_of_(std::string_view{target}.substr(5))) ||
                            (target.starts_with("kind:") && kind_named_(std::string_view{target}.substr(5)));
            if (!ok) throw row.error("target", "must be age:<band>, body:<body> or kind:<kind>");
            (void)non_negative_(row, "factor");
            break;
        }
        default: break;
        }
    }

    static std::optional<std::pair<unsigned, unsigned>> owner_band_of_(std::string_view text)
    {
        const auto dash = text.find('-');
        if (dash == std::string_view::npos || dash == 0) return std::nullopt;
        const auto lo = number_(text.substr(0, dash));
        const auto rest = text.substr(dash + 1);
        const auto hi = rest.empty() ? std::optional<double>{1000.0} : number_(rest);
        if (!lo || !hi || *lo < 0.0 || *hi < *lo) return std::nullopt;
        return std::pair{static_cast<unsigned>(*lo), static_cast<unsigned>(*hi)};
    }

    // -- Building -----------------------------------------------------------

    static std::invalid_argument build_error_(const std::string& what)
    {
        return std::invalid_argument("vehicle-generator: " + what);
    }

    // Linear interpolation over (year, value) points, held flat outside them.
    template <typename T>
    static T interpolate_(const std::vector<std::pair<int, T>>& points, int year)
    {
        if (points.empty()) return T{};
        if (year <= points.front().first) return points.front().second;
        for (std::size_t i = 1; i < points.size(); ++i)
        {
            if (year <= points[i].first)
            {
                const auto& [y0, v0] = points[i - 1];
                const auto& [y1, v1] = points[i];
                const double t = static_cast<double>(year - y0) / static_cast<double>(y1 - y0);
                if constexpr (std::is_same_v<T, double>)
                {
                    return v0 + core::math::fenced(t * (v1 - v0));
                }
                else
                {
                    T out{};
                    for (std::size_t k = 0; k < out.size(); ++k) out[k] = v0[k] + core::math::fenced(t * (v1[k] - v0[k]));
                    return out;
                }
            }
        }
        return points.back().second;
    }

    // Per-age fleet weights from band shares: uniform in a band, a geometric tail (ratio 0.8) from 20.
    static std::array<double, max_age_ + 1> age_weights_(const std::array<double, band_count_>& shares)
    {
        std::array<double, max_age_ + 1> out{};
        constexpr std::array<std::pair<int, int>, 4> spans{{{0, 1}, {2, 4}, {5, 9}, {10, 19}}};
        for (std::size_t b = 0; b < spans.size(); ++b)
        {
            const auto [lo, hi] = spans.at(b);
            for (int a = lo; a <= hi; ++a)
                out.at(static_cast<std::size_t>(a)) = shares.at(b) / static_cast<double>(hi - lo + 1);
        }
        double tail = 0.0;
        double w = 1.0;
        for (int a = 20; a <= max_age_; ++a, w *= 0.8) tail += w;
        w = 1.0;
        for (int a = 20; a <= max_age_; ++a, w *= 0.8)
            out.at(static_cast<std::size_t>(a)) = core::math::fenced(shares.at(4) * w) / tail;
        return out;
    }

    // Band shares of a fleet whose cars survive a year with a fixed ratio, fitted to a mean age.
    static std::array<double, band_count_> survival_shares_(double mean_age)
    {
        const auto mean_of = [](double r) {
            double sum = 0.0;
            double weighted = 0.0;
            double w = 1.0;
            for (int a = 0; a <= max_age_; ++a, w *= r)
            {
                sum += w;
                weighted += w * a;
            }
            return weighted / sum;
        };
        double lo = 0.01;
        double hi = 0.999;
        for (int i = 0; i < 60; ++i)
        {
            const double mid = (lo + hi) / 2.0;
            if (mean_of(mid) < mean_age) lo = mid;
            else hi = mid;
        }
        std::array<double, band_count_> shares{};
        double w = 1.0;
        double total = 0.0;
        for (int a = 0; a <= max_age_; ++a, w *= lo)
        {
            shares.at(band_of_(a)) += w;
            total += w;
        }
        for (auto& s : shares) s /= total;
        return shares;
    }

    static std::optional<model> build_model_(const tables& t)
    {
        const auto& rows = [&](table which) -> const auto& { return t.at(index_(which)).rows(); };
        model m;

        // Models and trims.
        for (const auto& row : rows(table::models))
        {
            model_rec r;
            r.key = row.get<std::string>("key");
            r.make = row.get<std::string>("make");
            r.name = row.get<std::string>("model");
            r.kind = kind_column_(row);
            r.from = row.get<int>("from_year");
            r.to = optional_<int>(row, "to_year").value_or(std::numeric_limits<int>::max());
            const auto bodies = *weighted_list_(row.get<std::string>("bodies"));
            for (const auto& [b, w] : bodies) r.bodies.emplace_back(*body_of_(b), w);
            r.markets = *weighted_list_(row.get<std::string>("markets"));
            r.seats = static_cast<unsigned>(row.get<int>("seats"));
            m.model_index.emplace(r.key, m.models.size());
            m.models.push_back(std::move(r));
        }
        for (const auto& row : rows(table::trims))
        {
            const auto key = row.get<std::string>("model");
            const auto it = m.model_index.find(key);
            if (it == m.model_index.end()) throw build_error_("trims.tsv names model " + key + ", which models.tsv lacks");
            auto& mdl = m.models[it->second];
            trim_rec tr;
            tr.label = row.get<std::string>("label");
            tr.fuel = *fuel_of_(row.get<std::string>("fuel"));
            tr.cc = tr.fuel == vehicle_fuel::electric ? 0U : static_cast<unsigned>(row.get<int>("engine_cc"));
            tr.kw = static_cast<unsigned>(row.get<int>("power_kw"));
            tr.from = std::max(row.get<int>("from_year"), mdl.from);
            tr.to = std::min(optional_<int>(row, "to_year").value_or(std::numeric_limits<int>::max()), mdl.to);
            const auto trans = row.get<std::string>("transmissions");
            tr.manual = trans.contains('m');
            tr.automatic = trans.contains('a');
            tr.semi = trans.contains('s');
            tr.drives = row.get<std::string>("drives");
            tr.weight = row.get<double>("weight");
            tr.markets = split_(text_(row, "markets"), ',');
            const bool motorcycle = mdl.kind == vehicle_kind::motorcycle;
            if (!tr.drives.empty() && tr.drives.find_first_not_of(motorcycle ? "cbsh" : "fra") != std::string::npos)
                throw build_error_("trim " + tr.label + " of " + key + " has drives " + tr.drives + ", which a " +
                                   std::string{dasmig::to_string(mdl.kind)} + " does not have");
            if (tr.semi && !motorcycle)
                throw build_error_("trim " + tr.label + " of " + key + " is semi-automatic, which only motorcycles are");
            if (tr.from <= tr.to && tr.weight > 0.0 && (tr.manual || tr.automatic || tr.semi) && !tr.drives.empty())
                mdl.trims.push_back(std::move(tr));
        }
        for (const auto& mdl : m.models)
            if (std::ranges::find(m.make_names, mdl.make) == m.make_names.end()) m.make_names.push_back(mdl.make);
        std::ranges::sort(m.make_names);

        // Countries.
        std::unordered_map<std::string, std::array<double, band_count_>> ages;
        for (const auto& row : rows(table::ages))
            ages[row.get<std::string>("country")].at(*band_index_(row.get<std::string>("band"))) =
                row.get<double>("share");
        using fuel_points = std::unordered_map<std::string, std::vector<std::pair<int, std::array<double, fuel_count_>>>>;
        std::array<fuel_points, kind_count_> fuels;
        std::array<std::unordered_map<std::string, std::vector<const core::tsv_row*>>, kind_count_> make_rows;
        std::array<std::unordered_map<std::string, std::vector<std::size_t>>, kind_count_> models_of_make;
        for (const auto k : kinds_)
        {
            const auto kt = tables_of_(k);
            const auto ki = static_cast<std::size_t>(k);
            for (const auto& row : rows(kt.fuel_shares))
            {
                std::array<double, fuel_count_> s{};
                for (std::size_t f = 0; f < fuel_count_; ++f)
                    s.at(f) = row.get<double>(dasmig::to_string(vehicle_fuel(f)));
                fuels.at(ki)[row.get<std::string>("country")].emplace_back(row.get<int>("year"), s);
            }
            for (const auto& row : rows(kt.make_shares))
                make_rows.at(ki)[row.get<std::string>("country")].push_back(&row);
            for (std::size_t i = 0; i < m.models.size(); ++i)
                if (m.models[i].kind == k) models_of_make.at(ki)[m.models[i].make].push_back(i);

            // Colours by market.
            auto& colours = m.colours.at(ki);
            std::unordered_map<std::string, std::vector<std::pair<int, family_weights>>> colour_points;
            for (const auto& row : rows(kt.colours))
            {
                family_weights w{};
                for (std::size_t f = 0; f < family_count_; ++f) w.at(f) = row.get<double>(families_.at(f));
                const auto market = row.get<std::string>("market");
                if (!colour_points.contains(market)) colours.markets.push_back(market);
                colour_points[market].emplace_back(row.get<int>("year"), w);
            }
            for (const auto& market : colours.markets)
            {
                auto& points = colour_points[market];
                std::ranges::sort(points, {}, &std::pair<int, family_weights>::first);
                std::vector<family_weights> by_year;
                for (int s = 0; s < year_slots_; ++s) by_year.push_back(interpolate_(points, first_year_ + s));
                colours.weights.push_back(std::move(by_year));
            }

            for (const auto& row : rows(kt.factories))
            {
                const auto wmi = row.get<std::string>("wmi");
                const auto suv = text_(row, "wmi_suv");
                const auto truck = text_(row, "wmi_truck");
                m.factories.at(ki)[row.get<std::string>("make")].push_back(
                    {.market = row.get<std::string>("market"),
                     .country = row.get<std::string>("country"),
                     .weight = row.get<double>("weight"),
                     .wmi = wmi,
                     .wmi_suv = suv.empty() ? wmi : suv,
                     .wmi_truck = truck.empty() ? wmi : truck,
                     .plants = row.get<std::string>("plants"),
                     .zzz = row.get<std::string>("vds") == "zzz"});
            }
        }
        std::unordered_map<std::string, std::vector<std::pair<int, double>>> automatic;
        for (const auto& row : rows(table::transmission))
            automatic[row.get<std::string>("country")].emplace_back(row.get<int>("year"),
                                                                    row.get<double>("automatic_share"));
        std::unordered_map<std::string, const core::tsv_row*> mileage;
        for (const auto& row : rows(table::mileage)) mileage[row.get<std::string>("country")] = &row;

        for (const auto& row : rows(table::colour_names))
        {
            m.colour_names.at(static_cast<std::size_t>(kind_column_(row)))[row.get<std::string>("make")]
                .at(*family_of_(row.get<std::string>("family")))
                .push_back({.name = row.get<std::string>("name"),
                            .weight = row.get<double>("weight"),
                            .from = optional_<int>(row, "from_year").value_or(0),
                            .to = optional_<int>(row, "to_year").value_or(std::numeric_limits<int>::max())});
        }

        // Plates.
        for (const auto& row : rows(table::plate_regions))
        {
            auto& set = m.region_sets[row.get<std::string>("set")];
            const bool reserved = row.get_or<bool>("reserved", false);
            set.codes.push_back({.code = row.get<std::string>("code"),
                                 .label = row.get<std::string>("label"),
                                 .weight = row.get<double>("weight"),
                                 .sub = static_cast<unsigned>(optional_<int>(row, "sub").value_or(0)),
                                 .reserved = reserved});
            set.weights.push_back(reserved ? 0.0 : row.get<double>("weight"));
            if (reserved) set.reserved.push_back(set.codes.size() - 1);
        }
        std::unordered_map<std::string, std::vector<std::pair<int, std::string>>> anchors;
        for (const auto& row : rows(table::plate_series))
            anchors[row.get<std::string>("format")].emplace_back(*parse_day_(row.get<std::string>("date")),
                                                                 row.get<std::string>("plate"));
        for (const auto& row : rows(table::plate_formats))
        {
            plate_format f;
            f.key = row.get<std::string>("key");
            f.country = row.get<std::string>("country");
            f.region = row.get<std::string>("region");
            f.regions = text_(row, "regions");
            if (const auto from = text_(row, "from"); !from.empty()) f.from = *parse_day_(from);
            if (const auto until = text_(row, "until"); !until.empty()) f.until = *parse_day_(until);
            f.sequential = row.get<std::string>("kind") == "sequential";
            f.items = parse_pattern_(row.get<std::string>("pattern"));
            f.letters = letters_(row.get<std::string>("letters"));
            for (const auto& fuel : split_(text_(row, "fuels"), ';')) f.fuels |= 1U << static_cast<unsigned>(*fuel_of_(fuel));
            f.min_age = optional_<int>(row, "min_age").value_or(0);
            f.weight = optional_<double>(row, "weight").value_or(1.0);
            f.exclude = split_(text_(row, "exclude"), ';');
            f.max_chars = static_cast<std::size_t>(optional_<int>(row, "max_chars").value_or(0));
            f.reissue = optional_<double>(row, "reissue").value_or(0.0);
            for (const auto& k : split_(text_(row, "kinds"), ';')) f.kinds |= kind_bit_(*kind_named_(k));
            const bool uses_region = std::ranges::any_of(f.items, [](const plate_item& it) {
                return it.kind == 't' && (it.token == 'R' || it.token == 'S' || it.token == 'C');
            });
            if (uses_region && !m.region_sets.contains(f.regions))
                throw build_error_("plate format " + f.key + " uses region codes but names no region set in "
                                   "plate_regions.tsv");
            if (f.region != "*" && !m.region_sets.contains(f.country))
                throw build_error_("plate format " + f.key + " belongs to region " + f.region +
                                   " but plate_regions.tsv has no set " + f.country);
            if (f.sequential)
            {
                std::vector<std::tuple<unsigned, std::size_t, std::uint64_t>> slots;
                for (std::size_t i = 0; i < f.items.size(); ++i)
                {
                    const auto& it = f.items[i];
                    if (it.group == 0) continue;
                    const std::uint64_t radix = it.token == 'L' ? f.letters.size() : it.token == 'D' ? 10U : 9U;
                    slots.emplace_back(it.group, i, radix);
                }
                if (slots.empty()) throw build_error_("plate format " + f.key + " is sequential without a group");
                std::ranges::sort(slots);
                f.capacity = 1.0;
                for (const auto& [g, i, radix] : slots)
                {
                    f.layout.emplace_back(i, radix);
                    f.capacity *= static_cast<double>(radix);
                }
                auto list = anchors[f.key];
                std::ranges::sort(list);
                if (list.size() < 2) throw build_error_("plate format " + f.key + " needs two plate_series anchors");
                double last = -1.0;
                for (const auto& [day, plate] : list)
                {
                    const auto index = plate_index_(f, plate);
                    if (!index) throw build_error_("plate series " + f.key + ": " + plate + " does not fit the pattern");
                    if (*index < last) throw build_error_("plate series " + f.key + ": " + plate + " goes backwards");
                    last = *index;
                    f.anchors.emplace_back(day, *index);
                }
            }
            m.formats.push_back(std::move(f));
        }

        // VINs.
        for (const auto& row : rows(table::wmis))
            if (row.get<bool>("reserved")) m.reserved_wmis.push_back(row.get<std::string>("wmi"));

        // Owner factors.
        for (const auto& row : rows(table::owner_factors))
        {
            const auto [lo, hi] = *owner_band_of_(row.get<std::string>("owner_age"));
            auto it = std::ranges::find_if(m.owners, [&](const owner_band& b) { return b.lo == lo && b.hi == hi; });
            if (it == m.owners.end())
            {
                m.owners.push_back({.lo = lo, .hi = hi});
                it = m.owners.end() - 1;
            }
            const auto target = row.get<std::string>("target");
            const double factor = row.get<double>("factor");
            if (target.starts_with("age:")) it->age.at(*band_index_(std::string_view{target}.substr(4))) = factor;
            else if (target.starts_with("kind:"))
                it->kind.at(static_cast<std::size_t>(*kind_named_(std::string_view{target}.substr(5)))) = factor;
            else it->body.at(static_cast<std::size_t>(*body_of_(std::string_view{target}.substr(5)))) = factor;
        }

        for (const auto& row : rows(table::countries))
        {
            country_rec c;
            c.code = row.get<std::string>("country_code");
            c.miles = row.get<std::string>("units") == "mi";
            c.markets = split_(row.get<std::string>("markets"), ';');
            const auto* mileage_row = [&]() -> const core::tsv_row* {
                const auto mi = mileage.find(c.code);
                return mi == mileage.end() ? nullptr : mi->second;
            }();
            if (mileage_row)
                for (std::size_t f = 1; f < fuel_count_; ++f)
                    if (const auto v = optional_<double>(*mileage_row, dasmig::to_string(vehicle_fuel(f))))
                        c.mileage.at(f) = *v;
            for (std::size_t i = 0; i < m.formats.size(); ++i)
            {
                if (m.formats[i].country != c.code) continue;
                c.formats.push_back(i);
                if (m.formats[i].region != "*") c.jurisdictions = c.code;
            }

            for (const auto k : kinds_)
            {
                const auto ki = static_cast<std::size_t>(k);
                const bool car = k == vehicle_kind::car;
                auto& fl = c.fleets.at(ki);
                const double per_1000 = car ? row.get<double>("cars_per_1000")
                                            : optional_<double>(row, "motorcycles_per_1000").value_or(0.0);
                fl.size = core::math::fenced(row.get<double>("population") * per_1000) / 1000.0;
                const auto age_it = ages.find(c.code);
                if (car && age_it != ages.end()) fl.ages = age_weights_(age_it->second);
                else
                    fl.ages = age_weights_(survival_shares_(
                        car ? row.get<double>("mean_age")
                            : optional_<double>(row, "motorcycle_mean_age").value_or(row.get<double>("mean_age"))));

                auto fuel_rows = fuels.at(ki)[c.code];
                std::ranges::sort(fuel_rows, {}, &std::pair<int, std::array<double, fuel_count_>>::first);
                auto auto_points = automatic[c.code];
                std::ranges::sort(auto_points);
                for (int s = 0; s < year_slots_; ++s)
                {
                    auto shares = fuel_rows.empty() ? std::array<double, fuel_count_>{1.0}
                                                    : interpolate_(fuel_rows, first_year_ + s);
                    fl.fuels.push_back(shares);
                    if (car) fl.automatic.push_back(auto_points.empty() ? 0.3 : interpolate_(auto_points, first_year_ + s));
                    for (std::size_t f = 0; f < fuel_count_; ++f)
                        if (shares.at(f) > 0.0) fl.positive.at(f) |= year_mask{1} << static_cast<unsigned>(s);
                }
                const double car_km = mileage_row ? mileage_row->get<double>("annual_km") : fl.annual_km;
                fl.annual_km = car ? car_km
                                   : (mileage_row ? optional_<double>(*mileage_row, "motorcycle_km") : std::nullopt)
                                         .value_or(core::math::fenced(car_km * motorcycle_km_share_));

                for (const auto* mrow : make_rows.at(ki)[c.code])
                {
                    make_entry e;
                    e.make = mrow->get<std::string>("make");
                    e.weight = mrow->get<double>("weight");
                    const double trend = optional_<double>(*mrow, "trend").value_or(0.0);
                    e.growth = trend == 0.0 ? 0.0 : core::math::log(1.0 + trend);
                    for (const auto i : models_of_make.at(ki)[e.make])
                    {
                        const auto& mdl = m.models[i];
                        const double w = market_weight_(mdl, c);
                        if (!(w > 0.0)) continue;
                        model_entry me{.model = i, .weight = w};
                        for (const auto& tr : mdl.trims)
                        {
                            if (!trim_sold_(tr, c)) continue;
                            const auto years = span_mask_(tr.from, tr.to);
                            me.years.at(static_cast<std::size_t>(tr.fuel)) |= years;
                            if (car && tr.fuel == vehicle_fuel::petrol)
                                for (const auto f : {vehicle_fuel::lpg, vehicle_fuel::cng, vehicle_fuel::flex})
                                    me.years.at(static_cast<std::size_t>(f)) |= years;
                        }
                        if (std::ranges::any_of(me.years, [](year_mask y) { return y != 0; })) e.models.push_back(me);
                    }
                    if (e.weight > 0.0)
                        for (const auto& me : e.models)
                            for (std::size_t f = 0; f < fuel_count_; ++f) fl.supply.at(f) |= me.years.at(f);
                    fl.makes.push_back(std::move(e));
                }

                const auto& colour_markets = m.colours.at(ki).markets;
                for (const auto& market : c.markets)
                {
                    const auto it = std::ranges::find(colour_markets, market);
                    if (it != colour_markets.end())
                    {
                        fl.colours = static_cast<std::size_t>(it - colour_markets.begin());
                        break;
                    }
                }
                if (fl.colours == npos_)
                    if (const auto it = std::ranges::find(colour_markets, "*"); it != colour_markets.end())
                        fl.colours = static_cast<std::size_t>(it - colour_markets.begin());

                const bool plated = std::ranges::any_of(c.formats, [&](std::size_t i) {
                    return m.formats[i].kinds == 0 || (m.formats[i].kinds & kind_bit_(k)) != 0;
                });
                for (std::size_t f = 0; f < fuel_count_; ++f)
                    if ((fl.supply.at(f) & fl.positive.at(f)) != 0) fl.drawable = fl.size > 0.0 && plated;
                c.drawable = c.drawable || fl.drawable;
            }
            m.country_index.emplace(c.code, m.countries.size());
            m.countries.push_back(std::move(c));
        }
        return m;
    }

    static double market_weight_(const model_rec& mdl, const country_rec& c)
    {
        for (const auto& [k, w] : mdl.markets)
            if (k == c.code) return w;
        for (const auto& market : c.markets)
            for (const auto& [k, w] : mdl.markets)
                if (k == market) return w;
        return 0.0;
    }

    static bool trim_sold_(const trim_rec& tr, const country_rec& c)
    {
        if (tr.markets.empty()) return true;
        return std::ranges::any_of(tr.markets, [&](const std::string& k) {
            return k == c.code || std::ranges::find(c.markets, k) != c.markets.end();
        });
    }

    // Index of a plate in a sequential format, or nullopt when it does not fit.
    static std::optional<double> plate_index_(const plate_format& f, std::string_view plate)
    {
        std::vector<std::uint64_t> digits(f.items.size(), 0);
        std::size_t pos = 0;
        for (std::size_t i = 0; i < f.items.size(); ++i)
        {
            const auto& it = f.items[i];
            if (it.kind == 'l')
            {
                if (plate.substr(pos, it.text.size()) != it.text) return std::nullopt;
                pos += it.text.size();
                continue;
            }
            if (it.kind != 't' || it.group == 0 || pos >= plate.size()) return std::nullopt;
            const auto cp = code_points_(plate.substr(pos, 4)).front();
            if (it.token == 'L')
            {
                const auto at = std::ranges::find(f.letters, cp);
                if (at == f.letters.end()) return std::nullopt;
                digits[i] = static_cast<std::uint64_t>(at - f.letters.begin());
            }
            else
            {
                const char d = cp.size() == 1 ? cp[0] : '?';
                if (d < (it.token == 'N' ? '1' : '0') || d > '9') return std::nullopt;
                digits[i] = static_cast<std::uint64_t>(d - (it.token == 'N' ? '1' : '0'));
            }
            pos += cp.size();
        }
        if (pos != plate.size()) return std::nullopt;
        double index = 0.0;
        for (const auto& [i, radix] : f.layout) index = (index * static_cast<double>(radix)) + static_cast<double>(digits[i]);
        return index;
    }

    static documents read_directory_(const std::filesystem::path& dir)
    {
        documents docs;
        for (const auto t : all_tables_)
        {
            const auto file = dir / file_name_(t);
            std::error_code ec;
            if (std::filesystem::is_regular_file(file, ec)) docs.at(index_(t)) = core::tsv_document::load(file);
        }
        return docs;
    }

    void apply_(documents docs, core::layer_mode mode, bool reset)
    {
        tables next = _tables;
        for (const auto t : all_tables_)
        {
            auto& doc = docs.at(index_(t));
            auto& tbl = next.at(index_(t));
            if (!doc)
            {
                if (reset) tbl.clear();
                continue;
            }
            (void)tbl.extend(std::move(*doc), mode, [t](const core::tsv_row& row) { validate_(t, row); });
        }
        auto built = build_model_(next);
        _tables = std::move(next);
        _model = std::move(*built);
    }

    // -- State ------------------------------------------------------------

    tables _tables{make_tables_()};
    model _model;
    core::rng _engine{core::rng::from_entropy()};

    struct auto_probe_tag {};

    explicit vhg(auto_probe_tag /*tag*/)
    {
        if (!load(dataset::lite)) (void)load(dataset::full);
    }

    static std::span<const core::embedded_file> embedded_files_()
    {
#ifdef DASMIG_VEHICLE_EMBED
        return embedded::vehiclegen::files;
#else
        return {};
#endif
    }

    std::uint64_t draw_seed_() { return _engine(); }

    void require_data_() const
    {
        if (!has_data()) throw std::runtime_error("vehicle-generator: no data loaded. Call load() first.");
    }

    // -- Request ------------------------------------------------------------

    struct context
    {
        int day{0};
        int year{0};
        const country_rec* country{nullptr};
        std::optional<vehicle_kind> kind;
        std::optional<std::string> make;
        std::optional<std::size_t> model;
        std::optional<vehicle_body> body;
        std::optional<vehicle_fuel> fuel;
        int min_age{0};
        int max_age{max_age_};
        const owner_band* owner{nullptr};

        [[nodiscard]] bool narrowed() const { return make || model || body; }
    };

    static std::string upper_(std::string_view text)
    {
        std::string out{text};
        for (auto& c : out)
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        return out;
    }

    const country_rec& country_or_throw_(std::string_view code) const
    {
        const auto it = _model.country_index.find(upper_(code));
        if (it == _model.country_index.end()) throw core::unknown_country_error(code);
        return _model.countries[it->second];
    }

    // Whether a region code (any case) is one of the country's plate regions.
    [[nodiscard]] bool has_region_(const country_rec& c, std::string_view region) const
    {
        const auto code = upper_(region);
        return std::ranges::any_of(plate_regions(c.code), [&](const std::string& r) { return upper_(r) == code; });
    }

    context resolve_(const vehicle_request& request) const
    {
        context ctx;
        const auto& date = request.reference_date;
        if (!date.ok()) throw std::invalid_argument("vehicle-generator: the reference date is not a valid date");
        if (date < vehicle_request::earliest_reference_date || date > vehicle_request::latest_reference_date)
            throw std::invalid_argument("vehicle-generator: the reference date must be from 2000-01-01 to 2050-12-31");
        ctx.day = static_cast<int>(std::chrono::sys_days{date}.time_since_epoch().count());
        ctx.year = static_cast<int>(date.year());
        if (request.country) ctx.country = &country_or_throw_(*request.country);
        ctx.kind = request.kind;
        const auto imply_kind = [&](vehicle_kind k, const std::string& what) {
            if (ctx.kind && *ctx.kind != k)
                throw std::invalid_argument("vehicle-generator: " + what + " is a " + std::string{dasmig::to_string(k)} +
                                            ", not a " + std::string{dasmig::to_string(*ctx.kind)});
            ctx.kind = k;
        };
        if (request.body) imply_kind(kind_of(*request.body), "body " + std::string{dasmig::to_string(*request.body)});
        if (request.model)
        {
            const auto it = _model.model_index.find(*request.model);
            if (it == _model.model_index.end())
                throw std::invalid_argument("vehicle-generator: unknown model " + *request.model);
            ctx.model = it->second;
            const auto& make = _model.models[it->second].make;
            if (request.make && *request.make != make)
                throw std::invalid_argument("vehicle-generator: model " + *request.model + " is a " + make + ", not a " +
                                            *request.make);
            ctx.make = make;
            imply_kind(_model.models[it->second].kind, "model " + *request.model);
        }
        else if (request.make)
        {
            if (!std::ranges::binary_search(_model.make_names, *request.make))
                throw std::invalid_argument("vehicle-generator: unknown make " + *request.make);
            ctx.make = *request.make;
        }
        if (ctx.country != nullptr && !(ctx.kind ? ctx.country->fleet(*ctx.kind).drawable : ctx.country->drawable))
            throw std::invalid_argument("vehicle-generator: no " +
                                        std::string{ctx.kind ? plural_(*ctx.kind) : "vehicles"} + " are known in " +
                                        ctx.country->code);
        ctx.body = request.body;
        ctx.fuel = request.fuel;
        ctx.min_age = static_cast<int>(std::min<unsigned>(request.min_age.value_or(0), 1000));
        ctx.max_age = static_cast<int>(std::min<unsigned>(request.max_age.value_or(max_age_), max_age_));
        if (request.min_age && request.max_age && *request.min_age > *request.max_age)
            throw std::invalid_argument("vehicle-generator: min_age is above max_age");
        ctx.max_age = std::min(ctx.max_age, ctx.year - first_year_);
        if (ctx.min_age > ctx.max_age)
            throw std::invalid_argument("vehicle-generator: no model year from 1990 fits the age range at the "
                                        "reference date");
        if (request.owner_age)
            for (const auto& b : _model.owners)
                if (*request.owner_age >= b.lo && *request.owner_age <= b.hi) ctx.owner = &b;
        return ctx;
    }

    static std::string_view plural_(vehicle_kind k) { return k == vehicle_kind::car ? "cars" : "motorcycles"; }

    std::string describe_(const context& ctx) const
    {
        std::string s;
        if (ctx.kind) s += " " + std::string{dasmig::to_string(*ctx.kind)};
        if (ctx.model) s += " model " + _model.models[*ctx.model].key;
        else if (ctx.make) s += " make " + *ctx.make;
        if (ctx.body) s += " body " + std::string{dasmig::to_string(*ctx.body)};
        if (ctx.fuel) s += " fuel " + std::string{dasmig::to_string(*ctx.fuel)};
        s += " aged " + std::to_string(ctx.min_age) + " to " + std::to_string(ctx.max_age);
        return s;
    }

    // -- Draws ------------------------------------------------------------

    // Index of a weighted pick; uniform over the positive weights when not weighted.
    static std::size_t pick_index_(std::span<const double> weights, bool weighted, core::rng& r)
    {
        if (weighted) return core::discrete{weights}(r);
        std::vector<double> flat;
        flat.reserve(weights.size());
        for (const double w : weights) flat.push_back(w > 0.0 ? 1.0 : 0.0);
        return core::discrete{flat}(r);
    }

    static std::size_t below_(core::rng& r, std::size_t n)
    {
        return static_cast<std::size_t>(core::bounded(r, n));
    }

    static bool any_positive_(std::span<const double> weights)
    {
        return std::ranges::any_of(weights, [](double w) { return w > 0.0; });
    }

    bool model_ok_(const model_entry& e, const context& ctx) const
    {
        if (ctx.model && e.model != *ctx.model) return false;
        if (ctx.body)
        {
            const auto& bodies = _model.models[e.model].bodies;
            if (std::ranges::none_of(bodies, [&](const auto& b) { return b.first == *ctx.body && b.second > 0.0; }))
                return false;
        }
        return true;
    }

    // Model years each fuel can be supplied in, given the request's make, model and body.
    fuel_masks supply_(const fleet_rec& fl, const context& ctx) const
    {
        if (!ctx.narrowed()) return fl.supply;
        fuel_masks out{};
        for (const auto& mk : fl.makes)
        {
            if (!(mk.weight > 0.0) || (ctx.make && mk.make != *ctx.make)) continue;
            for (const auto& me : mk.models)
                if (model_ok_(me, ctx))
                    for (std::size_t f = 0; f < fuel_count_; ++f) out.at(f) |= me.years.at(f);
        }
        return out;
    }

    // Share of new vehicles of a model year that the request can be: the allowed fuels that are supplied.
    static double fuel_weight_(const fleet_rec& fl, const fuel_masks& supply, const context& ctx, int year,
                               std::size_t f)
    {
        if (ctx.fuel && static_cast<std::size_t>(*ctx.fuel) != f) return 0.0;
        if ((supply.at(f) & bit_(year)) == 0) return 0.0;
        return fl.fuels.at(static_cast<std::size_t>(slot_(year))).at(f);
    }

    static double year_weight_(const fleet_rec& fl, const fuel_masks& supply, const context& ctx, int year)
    {
        double w = 0.0;
        for (std::size_t f = 0; f < fuel_count_; ++f) w += fuel_weight_(fl, supply, ctx, year, f);
        return w;
    }

    // The vehicles of a kind in a country the request can be: the fleet times the owner's factor, or 0.
    double kind_weight_(const country_rec& c, vehicle_kind k, const context& ctx) const
    {
        if (ctx.kind && *ctx.kind != k) return 0.0;
        const auto& fl = c.fleet(k);
        if (!fl.drawable) return 0.0;
        const auto supply = supply_(fl, ctx);
        for (int a = ctx.min_age; a <= ctx.max_age; ++a)
            if (year_weight_(fl, supply, ctx, ctx.year - a) > 0.0)
                return ctx.owner ? core::math::fenced(fl.size * ctx.owner->kind.at(static_cast<std::size_t>(k)))
                                 : fl.size;
        return 0.0;
    }

    const country_rec& draw_country_(const context& ctx, const vehicle_request& request, const core::rng& call) const
    {
        std::vector<double> w;
        w.reserve(_model.countries.size());
        for (const auto& c : _model.countries)
        {
            double weight = 0.0;
            if (c.drawable && (!request.region || has_region_(c, *request.region)))
                for (const auto k : kinds_) weight += kind_weight_(c, k, ctx);
            w.push_back(weight);
        }
        if (!any_positive_(w))
        {
            const std::string where = request.region ? " with plate region " + *request.region : std::string{};
            throw std::invalid_argument("vehicle-generator: no country" + where + " has a" + describe_(ctx));
        }
        auto r = call.split("country");
        return _model.countries[pick_index_(w, request.weighted, r)];
    }

    vehicle_kind draw_kind_(const country_rec& c, const context& ctx, const vehicle_request& request,
                            const core::rng& call) const
    {
        if (ctx.kind) return *ctx.kind;
        std::array<double, kind_count_> w{};
        for (const auto k : kinds_) w.at(static_cast<std::size_t>(k)) = kind_weight_(c, k, ctx);
        if (!any_positive_(w))
            throw std::invalid_argument("vehicle-generator: nobody sells a" + describe_(ctx) + " in " + c.code);
        auto r = call.split("kind");
        return static_cast<vehicle_kind>(pick_index_(w, request.weighted, r));
    }

    std::string draw_jurisdiction_(const country_rec& c, const vehicle_request& request, const core::rng& call) const
    {
        if (c.jurisdictions.empty()) return {};
        const auto& set = _model.region_sets.at(c.jurisdictions);
        if (request.region)
        {
            const auto code = upper_(*request.region);
            for (const auto& r : set.codes)
                if (!r.reserved && upper_(r.code) == code) return r.code;
            throw std::invalid_argument("vehicle-generator: " + *request.region + " is not a plate region of " + c.code);
        }
        auto r = call.split("region");
        return set.codes[pick_index_(set.weights, request.weighted, r)].code;
    }

    int draw_age_(const country_rec& c, const fleet_rec& fl, const context& ctx, const fuel_masks& supply,
                  const vehicle_request& request, const core::rng& call) const
    {
        std::vector<double> w(static_cast<std::size_t>(ctx.max_age + 1), 0.0);
        for (int a = ctx.min_age; a <= ctx.max_age; ++a)
        {
            double base = fl.ages.at(static_cast<std::size_t>(a));
            if (ctx.owner) base *= ctx.owner->age.at(band_of_(a));
            w.at(static_cast<std::size_t>(a)) = core::math::fenced(base * year_weight_(fl, supply, ctx, ctx.year - a));
        }
        if (!any_positive_(w))
            throw std::invalid_argument("vehicle-generator: nobody sells a" + describe_(ctx) + " in " + c.code);
        auto r = call.split("age");
        return static_cast<int>(pick_index_(w, request.weighted, r));
    }

    // A day from 1 October of the year before the model year to the end of the model year, before the reference day.
    static int draw_registration_(int model_year, int reference_day, const core::rng& call)
    {
        const int lo = day_of_(model_year - 1, 10, 1);
        const int hi = std::min(day_of_(model_year, 12, 31), reference_day);
        auto r = call.split("registration");
        return core::uniform_int(r, lo, std::max(lo, hi));
    }

    static vehicle_fuel draw_fuel_(const fleet_rec& fl, const context& ctx, const fuel_masks& supply, int year,
                                   const vehicle_request& request, const core::rng& call)
    {
        if (ctx.fuel) return *ctx.fuel;
        std::array<double, fuel_count_> w{};
        for (std::size_t f = 0; f < fuel_count_; ++f) w.at(f) = fuel_weight_(fl, supply, ctx, year, f);
        auto r = call.split("fuel");
        return static_cast<vehicle_fuel>(pick_index_(w, request.weighted, r));
    }

    bool make_offers_(const make_entry& mk, const context& ctx, vehicle_fuel fuel, int year) const
    {
        return std::ranges::any_of(mk.models, [&](const model_entry& me) {
            return model_ok_(me, ctx) && (me.years.at(static_cast<std::size_t>(fuel)) & bit_(year)) != 0;
        });
    }

    const make_entry& draw_make_(const fleet_rec& fl, const context& ctx, vehicle_fuel fuel, int year,
                                 const vehicle_request& request, const core::rng& call) const
    {
        std::vector<double> w;
        w.reserve(fl.makes.size());
        for (const auto& mk : fl.makes)
        {
            double weight = 0.0;
            if (mk.weight > 0.0 && (!ctx.make || mk.make == *ctx.make) && make_offers_(mk, ctx, fuel, year))
            {
                weight = mk.weight;
                if (mk.growth != 0.0)
                    weight = core::math::fenced(
                        mk.weight * core::math::exp(core::math::fenced(mk.growth * static_cast<double>(year - trend_year_))));
            }
            w.push_back(weight);
        }
        if (ctx.make)
        {
            for (std::size_t i = 0; i < w.size(); ++i)
                if (w[i] > 0.0) return fl.makes[i];
        }
        auto r = call.split("make");
        return fl.makes[pick_index_(w, request.weighted, r)];
    }

    double body_factor_(const model_rec& mdl, const context& ctx) const
    {
        if (!ctx.owner) return 1.0;
        double total = 0.0;
        double scaled = 0.0;
        for (const auto& [b, w] : mdl.bodies)
        {
            total += w;
            scaled += core::math::fenced(w * ctx.owner->body.at(static_cast<std::size_t>(b)));
        }
        return total > 0.0 ? scaled / total : 1.0;
    }

    // The production lifecycle: half weight in the first and the last model year.
    static double lifecycle_(const model_rec& mdl, int year)
    {
        double w = 1.0;
        if (year == mdl.from) w *= 0.5;
        if (year == mdl.to) w *= 0.6;
        return w;
    }

    std::size_t draw_model_(const make_entry& mk, const context& ctx, vehicle_fuel fuel, int year,
                            const vehicle_request& request, const core::rng& call) const
    {
        if (ctx.model) return *ctx.model;
        std::vector<double> w;
        w.reserve(mk.models.size());
        for (const auto& me : mk.models)
        {
            double weight = 0.0;
            if (model_ok_(me, ctx) && (me.years.at(static_cast<std::size_t>(fuel)) & bit_(year)) != 0)
            {
                const auto& mdl = _model.models[me.model];
                weight = core::math::fenced(me.weight * lifecycle_(mdl, year) * body_factor_(mdl, ctx));
            }
            w.push_back(weight);
        }
        auto r = call.split("model");
        return mk.models[pick_index_(w, request.weighted, r)].model;
    }

    static vehicle_body draw_body_(const model_rec& mdl, const context& ctx, const vehicle_request& request,
                                   const core::rng& call)
    {
        if (ctx.body) return *ctx.body;
        std::vector<double> w;
        for (const auto& [b, weight] : mdl.bodies)
            w.push_back(ctx.owner ? core::math::fenced(weight * ctx.owner->body.at(static_cast<std::size_t>(b))) : weight);
        if (!any_positive_(w)) return mdl.bodies.front().first;
        auto r = call.split("body");
        return mdl.bodies[pick_index_(w, request.weighted, r)].first;
    }

    static unsigned doors_(vehicle_body b)
    {
        if (kind_of(b) == vehicle_kind::motorcycle) return 0;
        switch (b)
        {
        case vehicle_body::sedan:
        case vehicle_body::pickup: return 4;
        case vehicle_body::coupe:
        case vehicle_body::convertible: return 2;
        default: return 5;
        }
    }

    static std::string_view drive_name_(char d)
    {
        switch (d)
        {
        case 'f': return "fwd";
        case 'r': return "rwd";
        case 'a': return "awd";
        case 'c': return "chain";
        case 'b': return "belt";
        case 's': return "shaft";
        default: return "hub";
        }
    }

    void apply_trim_(vehicle& v, const country_rec& c, const model_rec& mdl, vehicle_fuel fuel, int year,
                     const vehicle_request& request, const core::rng& call) const
    {
        const auto candidates = [&](vehicle_fuel f) {
            std::vector<const trim_rec*> out;
            for (const auto& tr : mdl.trims)
                if (tr.fuel == f && tr.from <= year && year <= tr.to && trim_sold_(tr, c)) out.push_back(&tr);
            return out;
        };
        auto list = candidates(fuel);
        const bool converted = list.empty() && mdl.kind == vehicle_kind::car &&
                               (fuel == vehicle_fuel::lpg || fuel == vehicle_fuel::cng || fuel == vehicle_fuel::flex);
        if (converted) list = candidates(vehicle_fuel::petrol);
        if (list.empty()) throw std::logic_error("vehicle-generator: no trim for " + mdl.key);
        std::vector<double> w;
        for (const auto* tr : list) w.push_back(tr->weight);
        auto r = call.split("trim");
        const trim_rec& tr = *list[pick_index_(w, request.weighted, r)];
        v.trim = tr.label;
        if (converted) v.trim += fuel == vehicle_fuel::lpg ? " LPG" : fuel == vehicle_fuel::cng ? " CNG" : " Flex";
        v.engine_cc = tr.cc;
        v.power_kw = tr.kw;
        bool automatic = tr.automatic;
        if (tr.manual && tr.automatic)
        {
            const bool electrified =
                fuel == vehicle_fuel::electric || fuel == vehicle_fuel::hybrid || fuel == vehicle_fuel::plug_in;
            const double share = mdl.kind == vehicle_kind::car
                                     ? c.fleet(vehicle_kind::car).automatic.at(static_cast<std::size_t>(slot_(year)))
                                     : dual_clutch_share_;
            automatic = electrified || core::bernoulli(r, share);
        }
        v.transmission = automatic ? "automatic" : tr.manual ? "manual" : "semi-automatic";
        char drive = tr.drives.front();
        if (tr.drives.size() > 1 && !core::bernoulli(r, 0.7))
            drive = tr.drives[1 + below_(r, tr.drives.size() - 1)];
        v.drive = drive_name_(drive);
    }

    void apply_colour_(vehicle& v, const fleet_rec& fl, int year, const vehicle_request& request,
                       const core::rng& call) const
    {
        const auto ki = static_cast<std::size_t>(v.kind);
        if (fl.colours == npos_) return;
        const auto& w = _model.colours.at(ki).weights[fl.colours].at(static_cast<std::size_t>(slot_(year)));
        if (!any_positive_(w)) return;
        auto r = call.split("colour");
        const auto family = pick_index_(w, request.weighted, r);
        v.colour_family = families_.at(family);
        const auto names_for = [&](const std::string& make) -> std::vector<const colour_name_rec*> {
            std::vector<const colour_name_rec*> out;
            const auto& names = _model.colour_names.at(ki);
            const auto it = names.find(make);
            if (it == names.end()) return out;
            for (const auto& n : it->second.at(family))
                if (n.from <= year && year <= n.to && n.weight > 0.0) out.push_back(&n);
            return out;
        };
        auto names = names_for(v.make);
        if (names.empty()) names = names_for("*");
        if (names.empty())
        {
            v.colour = v.colour_family;
            v.colour.front() = static_cast<char>(v.colour.front() - 'a' + 'A');
            return;
        }
        std::vector<double> nw;
        for (const auto* n : names) nw.push_back(n->weight);
        v.colour = names[pick_index_(nw, request.weighted, r)]->name;
    }

    static void apply_odometer_(vehicle& v, const country_rec& c, const fleet_rec& fl, int registered, int day,
                                const vehicle_request& request, const core::rng& call)
    {
        const bool miles = request.units == distance_units::imperial ||
                           (request.units == distance_units::country && c.miles);
        v.odometer_unit = miles ? "mi" : "km";
        auto r = call.split("odometer");
        const int days = day - registered;
        double km = 0.0;
        if (days < 30)
        {
            km = static_cast<double>(core::uniform_int(r, 5, 300));
        }
        else
        {
            const double years = static_cast<double>(days) / 365.25;
            const double median =
                core::math::fenced(fl.annual_km * c.mileage.at(static_cast<std::size_t>(v.fuel)) * years);
            km = core::lognormal(r, core::math::log(median), years < 1.0 ? 0.25 : 0.35);
        }
        const double value = miles ? km / 1.609344 : km;
        v.odometer = static_cast<unsigned>(std::min(value, 2.0e6) / 10.0 + 0.5) * 10U;
        if (v.odometer == 0) v.odometer = 10;
    }

    // -- Plates -----------------------------------------------------------

    static std::string two_digits_(int n)
    {
        n = ((n % 100) + 100) % 100;
        return std::string(1, static_cast<char>('0' + (n / 10))) + static_cast<char>('0' + (n % 10));
    }

    // The British age identifier: 23 for March to August 2023, 73 for September 2023 to February 2024.
    static std::string uk_age_(int day)
    {
        const auto d = ymd_(day);
        const int y = static_cast<int>(d.year());
        const auto mo = static_cast<unsigned>(d.month());
        if (mo >= 3 && mo <= 8) return two_digits_(y);
        return two_digits_((mo >= 9 ? y : y - 1) + 50);
    }

    // The British prefix year letter (August 1983 to August 2001).
    static std::string uk_prefix_(int day)
    {
        const auto d = ymd_(day);
        const int months = ((static_cast<int>(d.year()) - 1983) * 12) + static_cast<int>(static_cast<unsigned>(d.month())) - 8;
        constexpr std::string_view yearly = "ABCDEFGHJKLMNPR";
        constexpr std::string_view half = "STVWXY";
        if (months < 15 * 12) return std::string(1, yearly.at(static_cast<std::size_t>(std::max(0, months / 12))));
        const int k = months - (15 * 12);
        const int i = k < 7 ? 0 : 1 + ((k - 7) / 6);
        return std::string(1, half.at(static_cast<std::size_t>(std::min(i, 5))));
    }

    static std::string japanese_serial_(unsigned n)
    {
        if (n >= 1000) return two_digits_(static_cast<int>(n / 100)) + "-" + two_digits_(static_cast<int>(n % 100));
        std::string digits = std::to_string(n);
        std::string out;
        for (std::size_t i = digits.size(); i < 4; ++i) out += "\xE3\x83\xBB";  // KATAKANA MIDDLE DOT
        return out + digits;
    }

    // The index a sequential format was issuing on a day, and its daily rate.
    static std::pair<double, double> series_at_(const plate_format& f, int day)
    {
        const auto& a = f.anchors;
        std::size_t i = 1;
        while (i + 1 < a.size() && day > a[i].first) ++i;
        const double rate = (a[i].second - a[i - 1].second) / static_cast<double>(a[i].first - a[i - 1].first);
        const double index = a[i - 1].second + core::math::fenced(rate * static_cast<double>(day - a[i - 1].first));
        return {std::clamp(index, 0.0, f.capacity - 1.0), rate};
    }

    struct plate_slot
    {
        char token{0};  // 0 for literal text
        unsigned group{0};
        std::string text;
    };

    struct plate_out
    {
        std::string text, region;
        bool reserved{false};
    };

    static std::size_t visible_chars_(const std::string& s)
    {
        std::size_t n = 0;
        for (const auto& cp : code_points_(s))
            if (cp != " " && cp != "-" && cp != "." && cp != "/" && cp != "\xC2\xB7") ++n;
        return n;
    }

    static bool excluded_(const plate_format& f, const std::vector<plate_slot>& slots)
    {
        if (f.exclude.empty()) return false;
        std::string run;
        const auto check = [&] {
            const bool bad = std::ranges::any_of(f.exclude, [&](const std::string& x) { return run.find(x) != std::string::npos; });
            run.clear();
            return bad;
        };
        for (const auto& s : slots)
        {
            if (s.token == 'L' && s.group == 0) run += s.text;
            else if (check()) return true;
        }
        return check();
    }

    plate_out make_plate_(const plate_format& f, int issued, const std::string& region_request, bool safe,
                          bool weighted, core::rng& r, core::rng& rr) const
    {
        const region_set* set = f.regions.empty() ? nullptr : &_model.region_sets.at(f.regions);
        plate_out out;
        const region_rec* region = nullptr;
        if (set)
        {
            if (!region_request.empty())
            {
                for (const auto& code : set->codes)
                    if (!code.reserved && upper_(code.code) == upper_(region_request)) region = &code;
                if (!region)
                    throw std::invalid_argument("vehicle-generator: " + region_request +
                                                " is not a region of plate format " + f.key);
            }
            else if (safe && !set->reserved.empty() && !f.sequential)
            {
                region = &set->codes[set->reserved[below_(rr, set->reserved.size())]];
                out.reserved = true;
            }
            else
            {
                region = &set->codes[pick_index_(set->weights, weighted, rr)];
            }
            out.region = region->code;
        }

        std::optional<double> index;
        if (f.sequential)
        {
            auto [at, rate] = series_at_(f, issued);
            at += core::math::fenced(rate * core::uniform_real(r, -7.0, 7.0));
            if (safe && !out.reserved && unused_letters_(f).empty())
            {
                // Past the last plate of the series: where it ended, or where it will be at the end of the data.
                const double last = series_at_(f, std::min(f.until, latest_day_())).first + 1.0;
                if (last < f.capacity - 1.0)
                {
                    at = core::uniform_real(r, last, f.capacity - 1.0);
                    out.reserved = true;
                }
            }
            index = std::clamp(at, 0.0, f.capacity - 1.0);
        }

        const auto letter = [&] { return f.letters[below_(r, f.letters.size())]; };
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            std::vector<plate_slot> slots;
            for (const auto& it : f.items)
            {
                if (it.kind == 'l')
                {
                    slots.push_back({.text = it.text});
                    continue;
                }
                if (it.kind == 'c')
                {
                    slots.push_back({.text = it.options[below_(r, it.options.size())]});
                    continue;
                }
                const unsigned n = it.lo == it.hi ? it.lo : core::uniform_int(r, it.lo, it.hi);
                for (unsigned k = 0; k < n; ++k)
                {
                    plate_slot s{.token = it.token, .group = it.group, .text = {}};
                    switch (it.token)
                    {
                    case 'L': s.text = letter(); break;
                    case 'D': s.text = std::string(1, static_cast<char>('0' + below_(r, 10))); break;
                    case 'N': s.text = std::string(1, static_cast<char>('1' + below_(r, 9))); break;
                    case 'X':
                    {
                        const auto pick = below_(r, f.letters.size() + 10);
                        s.text = pick < f.letters.size() ? f.letters[pick]
                                                         : std::string(1, static_cast<char>('0' + pick - f.letters.size()));
                        break;
                    }
                    case 'R': s.text = region ? region->code : std::string{}; break;
                    case 'S':
                        s.text = two_digits_(static_cast<int>(1 + below_(r, region && region->sub ? region->sub : 99)));
                        break;
                    case 'C':
                    {
                        constexpr std::string_view office = "ABCDEFGHJKLMNPQRSTUVWXYZ";
                        const auto count = std::min<std::size_t>(region && region->sub ? region->sub : 1, office.size());
                        s.text = std::string(1, office.at(below_(r, count)));
                        break;
                    }
                    case 'A': s.text = uk_age_(issued); break;
                    case 'Y': s.text = uk_prefix_(issued); break;
                    case 'V': s.text = two_digits_(static_cast<int>(ymd_(issued).year())); break;
                    case 'H': s.text = static_cast<unsigned>(ymd_(issued).month()) <= 6 ? "1" : "2"; break;
                    case 'J': s.text = japanese_serial_(static_cast<unsigned>(core::uniform_int(r, 1, 9999))); break;
                    default: break;
                    }
                    slots.push_back(std::move(s));
                }
            }
            if (index)
            {
                // Write the index into the group characters, least significant last.
                auto rest = static_cast<std::uint64_t>(*index);
                std::vector<std::size_t> group_slots;
                for (std::size_t i = 0; i < slots.size(); ++i)
                    if (slots[i].group != 0) group_slots.push_back(i);
                std::ranges::stable_sort(group_slots, {}, [&](std::size_t i) { return slots[i].group; });
                for (std::size_t k = group_slots.size(); k-- > 0;)
                {
                    auto& s = slots[group_slots[k]];
                    const std::uint64_t radix = s.token == 'L' ? f.letters.size() : s.token == 'D' ? 10U : 9U;
                    const auto digit = rest % radix;
                    rest /= radix;
                    s.text = s.token == 'L' ? f.letters[digit]
                                            : std::string(1, static_cast<char>((s.token == 'N' ? '1' : '0') + digit));
                }
            }
            if (excluded_(f, slots)) continue;
            if (safe && !out.reserved) out.reserved = mark_unissued_(f, slots, r);
            std::string text;
            for (const auto& s : slots) text += s.text;
            if (f.max_chars != 0 && visible_chars_(text) > f.max_chars) continue;
            out.text = std::move(text);
            return out;
        }
        throw std::runtime_error("vehicle-generator: plate format " + f.key + " produced no valid plate");
    }

    // Letters the format's alphabet never takes: I, O and Q, or, when it takes all three, the other capitals it lacks.
    static std::string unused_letters_(const plate_format& f)
    {
        const auto collect = [&](std::string_view candidates) {
            std::string out;
            for (const char c : candidates)
                if (std::ranges::find(f.letters, std::string(1, c)) == f.letters.end()) out += c;
            return out;
        };
        auto unused = collect("IOQ");
        if (unused.empty()) unused = collect("ABCDEFGHJKLMNPRSTUVWXYZ");
        return unused;
    }

    static int latest_day_()
    {
        return static_cast<int>(
            std::chrono::sys_days{vehicle_request::latest_reference_date}.time_since_epoch().count());
    }

    // Make a plate one no authority issues: a letter the format never uses, a 0 where it never stands, or all zeros.
    static bool mark_unissued_(const plate_format& f, std::vector<plate_slot>& slots, core::rng& r)
    {
        std::vector<std::size_t> letters;
        std::vector<std::size_t> nonzero;
        std::vector<std::size_t> digits;
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            if (slots[i].token == 'L') letters.push_back(i);
            if (slots[i].token == 'N') nonzero.push_back(i);
            if (slots[i].token == 'D') digits.push_back(i);
        }
        const auto unused = unused_letters_(f);
        if (!letters.empty() && !unused.empty())
        {
            slots[letters[below_(r, letters.size())]].text = std::string(1, unused[below_(r, unused.size())]);
            return true;
        }
        if (!nonzero.empty())
        {
            slots[nonzero[below_(r, nonzero.size())]].text = "0";
            return true;
        }
        for (const auto i : digits) slots[i].text = "0";
        return !digits.empty();
    }

    // The formats a vehicle can carry: its country (and jurisdiction), the issuing day, its kind, fuel and age. A
    // format of the vehicle's kind for every fuel and age takes the place of the formats of every kind that day.
    std::vector<const plate_format*> formats_at_(const country_rec& c, const std::string& jurisdiction, int day,
                                                 vehicle_kind kind, vehicle_fuel fuel, int age) const
    {
        std::vector<const plate_format*> out;
        bool own = false;
        for (const auto i : c.formats)
        {
            const auto& f = _model.formats[i];
            if (f.region != "*" && f.region != jurisdiction) continue;
            if (day < f.from || day > f.until) continue;
            if (f.kinds != 0 && (f.kinds & kind_bit_(kind)) == 0) continue;
            if (f.fuels != 0 && (f.fuels & (1U << static_cast<unsigned>(fuel))) == 0) continue;
            if (age < f.min_age) continue;
            own = own || (f.kinds != 0 && f.fuels == 0 && f.min_age == 0);
            out.push_back(&f);
        }
        if (own) std::erase_if(out, [](const plate_format* f) { return f->kinds == 0; });
        return out;
    }

    void apply_plate_(vehicle& v, const country_rec& c, const std::string& jurisdiction, int age, int registered,
                      int day, const vehicle_request& request, const core::rng& call) const
    {
        auto r = call.split("plate");
        auto rr = call.split("region");
        const auto pick = [&](int at) -> const plate_format* {
            const auto list = formats_at_(c, jurisdiction, at, v.kind, v.fuel, age);
            if (list.empty()) return nullptr;
            std::vector<double> w;
            for (const auto* f : list) w.push_back(f->weight);
            if (!any_positive_(w)) return nullptr;
            return list[pick_index_(w, request.weighted, r)];
        };
        int issued = registered;
        const plate_format* f = pick(issued);
        if (f && f->reissue > 0.0 && f->until < day)
        {
            const double years = static_cast<double>(day - f->until) / 365.25;
            const double keep = core::math::exp(core::math::fenced(core::math::log(1.0 - std::min(f->reissue, 0.999)) * years));
            if (!core::bernoulli(r, keep))
            {
                issued = core::uniform_int(r, std::max(registered, f->until + 1), day);
                if (const auto* g = pick(issued)) f = g;
                else issued = registered;
            }
        }
        if (!f) return;
        const std::string region_request = c.jurisdictions.empty() && request.region ? *request.region : std::string{};
        auto out = make_plate_(*f, issued, region_request, request.safe, request.weighted, r, rr);
        v.plate = std::move(out.text);
        v.plate_region = jurisdiction.empty() ? std::move(out.region) : jurisdiction;
        v.plate_format = f->key;
        v.reserved = request.safe && out.reserved;
    }

    // -- VINs ---------------------------------------------------------------

    static int vin_value_(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        constexpr std::string_view letters = "ABCDEFGHJKLMNPRSTUVWXYZ";
        constexpr std::array<int, 23> values{1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 7, 9, 2, 3, 4, 5, 6, 7, 8, 9};
        const auto i = letters.find(c);
        return i == std::string_view::npos ? 0 : values.at(i);
    }

    static char check_digit_(const std::string& vin)
    {
        constexpr std::array<int, 17> weights{8, 7, 6, 5, 4, 3, 2, 10, 0, 9, 8, 7, 6, 5, 4, 3, 2};
        int sum = 0;
        for (std::size_t i = 0; i < 17; ++i) sum += vin_value_(vin[i]) * weights.at(i);
        const int rem = sum % 11;
        return rem == 10 ? 'X' : static_cast<char>('0' + rem);
    }

    static char year_code_(int year)
    {
        constexpr std::string_view codes = "ABCDEFGHJKLMNPRSTVWXY123456789";
        const int i = (((year - 1980) % 30) + 30) % 30;
        return codes.at(static_cast<std::size_t>(i));
    }

    void apply_vin_(vehicle& v, const country_rec& c, const model_rec& mdl, const vehicle_request& request,
                    const core::rng& call) const
    {
        auto r = call.split("vin");
        const auto rand_char = [&] { return vin_chars_.at(below_(r, vin_chars_.size())); };
        const auto hashed = [](std::string_view text, std::size_t n) {
            std::uint64_t h = core::fnv1a64(text);
            std::string out;
            for (std::size_t i = 0; i < n; ++i)
            {
                out += vin_chars_.at(h % vin_chars_.size());
                h /= vin_chars_.size();
            }
            return out;
        };

        std::string wmi;
        std::string plants = "0";
        bool zzz = false;
        if (request.safe && !_model.reserved_wmis.empty())
        {
            wmi = _model.reserved_wmis[below_(r, _model.reserved_wmis.size())];
        }
        else
        {
            const auto& factories = _model.factories.at(static_cast<std::size_t>(v.kind));
            const auto it = factories.find(v.make);
            if (it == factories.end() || it->second.empty()) return;
            const auto& rows = it->second;
            std::vector<const factory_rec*> list;
            const auto collect = [&](std::string_view market) {
                for (const auto& f : rows)
                    if (f.market == market) list.push_back(&f);
            };
            collect(c.code);
            for (std::size_t i = 0; list.empty() && i < c.markets.size(); ++i) collect(c.markets[i]);
            if (list.empty()) collect("*");
            if (list.empty())
                for (const auto& f : rows) list.push_back(&f);
            std::vector<double> w;
            for (const auto* f : list) w.push_back(f->weight);
            const factory_rec& f = *list[any_positive_(w) ? pick_index_(w, request.weighted, r) : 0];
            wmi = v.body == vehicle_body::suv ? f.wmi_suv : v.body == vehicle_body::pickup ? f.wmi_truck : f.wmi;
            plants = f.plants;
            zzz = f.zzz;
        }
        std::string vin = wmi;
        if (zzz)
        {
            vin += "ZZZ";
            vin += hashed(mdl.key, 2);
        }
        else
        {
            vin += hashed(mdl.key + "/" + v.trim, 3);
            vin += rand_char();
            vin += rand_char();
        }
        vin += '0';
        vin += year_code_(v.model_year);
        vin += plants.at(below_(r, plants.size()));
        for (int i = 0; i < 6; ++i) vin += static_cast<char>('0' + below_(r, 10));
        vin[8] = check_digit_(vin);
        v.vin = std::move(vin);
        v.wmi = std::move(wmi);
        if (request.safe) v.reserved = v.reserved && !_model.reserved_wmis.empty();
    }
};

} // namespace dasmig

#endif // DASMIG_VEHICLEGEN_HPP
