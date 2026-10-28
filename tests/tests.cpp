#include "catch_amalgamated.hpp"
#include "../dasmig/vehiclegen.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

using dasmig::vehicle;
using dasmig::vehicle_body;
using dasmig::vehicle_fuel;
using dasmig::vehicle_request;
using dasmig::vehicle_table;
using dasmig::vhg;

static std::filesystem::path lite_dir()
{
    for (const auto* candidate : {"resources/lite", "../resources/lite"})
        if (std::filesystem::is_directory(candidate)) return candidate;
    return "resources/lite";
}

static vhg make_loaded_generator()
{
    vhg gen;
    if (!gen.load(dasmig::dataset::lite)) gen.load(lite_dir());
    REQUIRE(gen.has_data());
    return gen;
}

static const vhg& shared()
{
    static const vhg gen = make_loaded_generator();
    return gen;
}

// Rows of a data file, as column -> value maps.
static std::vector<std::map<std::string, std::string>> read_table(const std::string& name)
{
    std::ifstream in(lite_dir() / name);
    std::string line;
    std::vector<std::string> header;
    std::vector<std::map<std::string, std::string>> rows;
    const auto split = [](const std::string& s) {
        std::vector<std::string> out;
        std::string cell;
        std::istringstream ss(s);
        while (std::getline(ss, cell, '\t')) out.push_back(cell);
        if (!s.empty() && s.back() == '\t') out.emplace_back();
        return out;
    };
    while (std::getline(in, line))
    {
        if (line.starts_with("#")) continue;
        if (header.empty())
        {
            header = split(line);
            continue;
        }
        const auto cells = split(line);
        std::map<std::string, std::string> row;
        for (std::size_t i = 0; i < header.size(); ++i) row[header[i]] = i < cells.size() ? cells[i] : "";
        rows.push_back(std::move(row));
    }
    return rows;
}

static vehicle_request at(int y, unsigned m, unsigned d)
{
    vehicle_request r;
    r.reference_date = std::chrono::year_month_day{std::chrono::year{y}, std::chrono::month{m}, std::chrono::day{d}};
    return r;
}

static int days(const std::chrono::year_month_day& d)
{
    return static_cast<int>(std::chrono::sys_days{d}.time_since_epoch().count());
}

static bool vin_valid(const std::string& vin)
{
    if (vin.size() != 17 || vin.find_first_of("IOQ") != std::string::npos) return false;
    const std::string letters = "ABCDEFGHJKLMNPRSTUVWXYZ";
    const int values[] = {1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 7, 9, 2, 3, 4, 5, 6, 7, 8, 9};
    const int weights[] = {8, 7, 6, 5, 4, 3, 2, 10, 0, 9, 8, 7, 6, 5, 4, 3, 2};
    int sum = 0;
    for (std::size_t i = 0; i < 17; ++i)
    {
        const char c = vin[i];
        int v = 0;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (const auto p = letters.find(c); p != std::string::npos) v = values[p];
        else return false;
        sum += v * weights[i];
    }
    const int rem = sum % 11;
    return vin[8] == (rem == 10 ? 'X' : static_cast<char>('0' + rem));
}

static char year_code(int year)
{
    const std::string codes = "ABCDEFGHJKLMNPRSTVWXY123456789";
    return codes[static_cast<std::size_t>((((year - 1980) % 30) + 30) % 30)];
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

TEST_CASE("load(dataset::lite) loads every table", "[loading]")
{
    const auto& gen = shared();
    REQUIRE(gen.countries().size() > 150);
    REQUIRE(gen.makes().size() > 40);
    for (const auto t : {vehicle_table::countries, vehicle_table::ages, vehicle_table::fuel_shares,
                         vehicle_table::make_shares, vehicle_table::models, vehicle_table::trims,
                         vehicle_table::transmission, vehicle_table::colours, vehicle_table::colour_names,
                         vehicle_table::mileage, vehicle_table::plate_formats, vehicle_table::plate_series,
                         vehicle_table::plate_regions, vehicle_table::wmis, vehicle_table::factories,
                         vehicle_table::owner_factors})
    {
        REQUIRE(gen.layers(t).size() == 1);
    }
}

TEST_CASE("load - non-existent path is a no-op", "[loading]")
{
    vhg gen;
    gen.load(std::filesystem::path{"no/such/directory"});
    REQUIRE_FALSE(gen.has_data());
    REQUIRE(gen.data_hash() == 0);
}

TEST_CASE("load(dataset::full) loads the full tier", "[loading]")
{
    vhg gen;
    if (!gen.load(dasmig::dataset::full)) return; // the full tier is not compiled in
    REQUIRE(gen.countries().size() > shared().countries().size());
    REQUIRE(gen.models("Volkswagen").size() > shared().models("Volkswagen").size());
    for (std::uint64_t s = 0; s < 2000; ++s) REQUIRE_NOTHROW(gen.get_vehicle({}, s));
}

#ifndef DASMIG_VEHICLE_EMBED // compiled-in data is never missing
TEST_CASE("load(dataset) returns false for a missing tier", "[loading]")
{
    const auto old_cwd = std::filesystem::current_path();
    const auto tmp = std::filesystem::temp_directory_path() / "vhg_empty_dir";
    std::filesystem::create_directories(tmp);
    std::filesystem::current_path(tmp);
    vhg gen;
    const bool loaded = gen.load(dasmig::dataset::full);
    std::filesystem::current_path(old_cwd);
    std::filesystem::remove_all(tmp);
    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(gen.has_data());
}
#endif

TEST_CASE("the singleton loads data on first use", "[loading]")
{
    REQUIRE(vhg::instance().has_data());
}

// ---------------------------------------------------------------------------
// Replay
// ---------------------------------------------------------------------------

TEST_CASE("get_vehicle throws when no data is loaded", "[generation]")
{
    vhg gen;
    REQUIRE_THROWS_AS(gen.get_vehicle(vehicle_request{}), std::runtime_error);
    REQUIRE_THROWS_AS(gen.get_vehicle(vehicle_request{}, std::uint64_t{1}), std::runtime_error);
}

TEST_CASE("get_vehicle with a seed is deterministic and replays", "[generation][seed]")
{
    auto gen = make_loaded_generator();
    for (std::uint64_t s = 0; s < 100; ++s)
        REQUIRE(gen.get_vehicle({}, s).to_json() == shared().get_vehicle({}, s).to_json());
    const auto v = gen.get_vehicle(vehicle_request{});
    REQUIRE(gen.get_vehicle({}, v.seed()).to_json() == v.to_json());
    REQUIRE(v.signature().data == gen.data_hash());
}

// ---------------------------------------------------------------------------
// Coherence
// ---------------------------------------------------------------------------

TEST_CASE("vehicles are coherent with the data and the date", "[coherence]")
{
    struct span
    {
        int from, to;
    };
    std::map<std::string, std::vector<std::pair<std::string, span>>> trims; // model -> (label|fuel, years)
    for (const auto& row : read_table("trims.tsv"))
    {
        const int from = std::stoi(row.at("from_year"));
        const int to = row.at("to_year").empty() ? 9999 : std::stoi(row.at("to_year"));
        trims[row.at("model")].emplace_back(row.at("label") + "|" + row.at("fuel"), span{from, to});
    }
    std::map<std::string, std::string> make_of;
    for (const auto& row : read_table("models.tsv")) make_of[row.at("key")] = row.at("make");

    const auto& gen = shared();
    for (const auto& date : {at(2005, 3, 1), at(2015, 9, 30), at(2023, 7, 1), at(2026, 1, 15)})
    {
        const int day = days(date.reference_date);
        const int year = static_cast<int>(date.reference_date.year());
        for (std::uint64_t s = 0; s < 3000; ++s)
        {
            const auto v = gen.get_vehicle(date, s);
            INFO(v.to_json());
            REQUIRE(make_of.at(v.model_key) == v.make);
            REQUIRE(v.model_year >= 1990);
            REQUIRE(v.model_year <= year);
            const int reg = days(v.first_registration);
            REQUIRE(reg <= day);
            REQUIRE(reg >= days(std::chrono::year_month_day{std::chrono::year{v.model_year - 1}, std::chrono::month{10},
                                                            std::chrono::day{1}}));
            REQUIRE(reg <= days(std::chrono::year_month_day{std::chrono::year{v.model_year}, std::chrono::month{12},
                                                            std::chrono::day{31}}));

            // The trim was offered in the model year with the fuel (or is a converted petrol car).
            std::string label = v.trim;
            std::string fuel{dasmig::to_string(v.fuel)};
            for (const auto* suffix : {" LPG", " CNG", " Flex"})
            {
                if (label.ends_with(suffix))
                {
                    label.resize(label.size() - std::string{suffix}.size());
                    fuel = "petrol";
                }
            }
            bool found = false;
            for (const auto& [key, years] : trims.at(v.model_key))
                found = found || (key == label + "|" + fuel && years.from <= v.model_year && v.model_year <= years.to);
            REQUIRE(found);

            if (v.fuel == vehicle_fuel::electric) REQUIRE(v.engine_cc == 0);
            REQUIRE((v.transmission == "manual" || v.transmission == "automatic"));
            REQUIRE((v.drive == "fwd" || v.drive == "rwd" || v.drive == "awd"));
            REQUIRE(v.odometer >= 10);
            REQUIRE(v.odometer < 2000000);
            REQUIRE((v.odometer_unit == "km" || v.odometer_unit == "mi"));
            REQUIRE(!v.colour.empty());
            REQUIRE(!v.plate.empty());
            REQUIRE(v.plate.find("  ") == std::string::npos);
            REQUIRE_FALSE(v.reserved);
        }
    }
}

TEST_CASE("electric cars follow the fleet: rare in 2012 Norway, most of 2024's", "[coherence][stats]")
{
    const auto& gen = shared();
    auto request = at(2025, 6, 30);
    request.country = "NO";
    request.min_age = 1;
    request.max_age = 1;
    int electric = 0;
    for (std::uint64_t s = 0; s < 2000; ++s) electric += gen.get_vehicle(request, s).fuel == vehicle_fuel::electric;
    REQUIRE(electric > 1600);
    request.min_age = 13;
    request.max_age = 13;
    electric = 0;
    for (std::uint64_t s = 0; s < 2000; ++s) electric += gen.get_vehicle(request, s).fuel == vehicle_fuel::electric;
    REQUIRE(electric < 200);
}

// ---------------------------------------------------------------------------
// VINs
// ---------------------------------------------------------------------------

TEST_CASE("VINs are valid ISO 3779 numbers of the make and model year", "[vin]")
{
    std::map<std::string, std::set<std::string>> wmis_of; // make -> WMIs
    for (const auto& row : read_table("factories.tsv"))
        for (const auto* col : {"wmi", "wmi_suv", "wmi_truck"})
            if (!row.at(col).empty()) wmis_of[row.at("make")].insert(row.at(col));
    std::map<std::string, std::string> vds_of; // model/trim -> VIN positions 4-6
    const auto& gen = shared();
    for (std::uint64_t s = 0; s < 5000; ++s)
    {
        const auto v = gen.get_vehicle({}, s);
        INFO(v.to_json());
        REQUIRE(vin_valid(v.vin));
        REQUIRE(v.vin[9] == year_code(v.model_year));
        REQUIRE(v.vin.substr(0, 3) == v.wmi);
        REQUIRE(wmis_of.at(v.make).contains(v.wmi));
        if (v.vin.substr(3, 3) != "ZZZ")
        {
            const auto key = v.model_key + "/" + v.trim;
            const auto [it, inserted] = vds_of.emplace(key, v.vin.substr(3, 3));
            REQUIRE(it->second == v.vin.substr(3, 3));
        }
    }
    REQUIRE(year_code(2024) == 'R');
    REQUIRE(year_code(2009) == '9');
    REQUIRE(year_code(2000) == 'Y');
}

// ---------------------------------------------------------------------------
// Plates
// ---------------------------------------------------------------------------

TEST_CASE("plates match their national formats", "[plates]")
{
    // Written from the national rules, not from plate_formats.tsv.
    const std::map<std::string, std::regex> rules{
        {"de", std::regex{R"(^(?:[A-Z]|Ä|Ö|Ü){1,3}-[A-Z]{1,2} [1-9][0-9]{0,3}$)"}},
        {"de-e", std::regex{R"(^(?:[A-Z]|Ä|Ö|Ü){1,3}-[A-Z]{1,2} [1-9][0-9]{0,2}E$)"}},
        {"gb-2001", std::regex{R"(^[A-Y][A-Z] [0-9]{2} [A-HJ-PR-Z]{3}$)"}},
        {"es-2000", std::regex{R"(^[0-9]{4} [BCDFGHJKLMNPRSTVWXYZ]{3}$)"}},
        {"fr-siv", std::regex{R"(^[A-HJ-NP-TV-Z]{2}-[0-9]{3}-[A-HJ-NP-TV-Z]{2}$)"}},
        {"it-1994", std::regex{R"(^[A-HJ-NPR-TV-Z]{2} [0-9]{3} [A-HJ-NPR-TV-Z]{2}$)"}},
        {"nl-2019", std::regex{R"(^[BDFGHJKLNPRSTVXZ]-[0-9]{3}-[BDFGHJKLNPRSTVXZ]{2}$)"}},
        {"us-ca", std::regex{R"(^[1-9][A-HJ-NPR-Z]{3}[0-9]{3}$)"}},
        {"us-tx", std::regex{R"(^[A-HJ-NPR-Z]{3}-[0-9]{4}$)"}},
        {"in", std::regex{R"(^[A-Z]{2} [0-9]{2} [A-HJ-NP-Z]{1,2} [0-9]{4}$)"}},
        {"br-2018", std::regex{R"(^[A-Z]{3}[0-9][A-Z][0-9]{2}$)"}},
        {"ch", std::regex{R"(^[A-Z]{2} [1-9][0-9]{0,5}$)"}},
    };
    std::map<std::string, int> checked;
    const auto& gen = shared();
    for (const auto* country : {"DE", "GB", "ES", "FR", "IT", "NL", "US", "IN", "BR", "CH"})
    {
        auto request = at(2024, 6, 1);
        request.country = country;
        for (std::uint64_t s = 0; s < 1500; ++s)
        {
            const auto v = gen.get_vehicle(request, s);
            const auto rule = rules.find(v.plate_format);
            if (rule == rules.end()) continue;
            INFO(v.plate_format << " " << v.plate);
            REQUIRE(std::regex_match(v.plate, rule->second));
            ++checked[v.plate_format];
        }
    }
    for (const auto& [format, _] : rules)
        if (format != "de-e") REQUIRE(checked[format] > 0);
}

TEST_CASE("plates agree with the registration date, the fuel and the age", "[plates]")
{
    const auto& gen = shared();
    for (const auto* country : {"GB", "DE", "CN", "ES"})
    {
        auto request = at(2024, 6, 1);
        request.country = country;
        std::vector<std::pair<int, std::string>> spain;
        for (std::uint64_t s = 0; s < 3000; ++s)
        {
            const auto v = gen.get_vehicle(request, s);
            INFO(v.to_json());
            const int reg = days(v.first_registration);
            const auto ymd = v.first_registration;
            if (v.plate_format == "gb-2001")
            {
                const int y = static_cast<int>(ymd.year());
                const auto mo = static_cast<unsigned>(ymd.month());
                const int id = mo >= 3 && mo <= 8 ? y % 100 : ((mo >= 9 ? y : y - 1) % 100) + 50;
                REQUIRE(std::stoi(v.plate.substr(3, 2)) == id);
            }
            if (v.plate_format == "de-e")
                REQUIRE((v.fuel == vehicle_fuel::electric || v.fuel == vehicle_fuel::plug_in));
            if (v.plate_format == "de-h") REQUIRE(2024 - v.model_year >= 30);
            if (v.plate_format == "cn-nev")
            {
                REQUIRE((v.fuel == vehicle_fuel::electric || v.fuel == vehicle_fuel::plug_in));
                REQUIRE(reg >= days(std::chrono::year_month_day{std::chrono::year{2017}, std::chrono::month{11},
                                                                std::chrono::day{20}}));
            }
            if (v.plate_format == "es-2000") spain.emplace_back(reg, v.plate.substr(5, 3));
        }
        // Spanish series follow the registration date.
        std::ranges::sort(spain);
        for (std::size_t i = 0; i < spain.size(); ++i)
            for (std::size_t j = i + 1; j < spain.size() && j < i + 50; ++j)
                if (spain[j].first - spain[i].first > 30) REQUIRE(spain[i].second <= spain[j].second);
    }
}

TEST_CASE("plate regions: US states, German districts and requests", "[plates][region]")
{
    const auto& gen = shared();
    const auto states = gen.plate_regions("us");
    REQUIRE(states.size() == 51);
    REQUIRE(std::ranges::find(states, "CA") != states.end());
    const auto districts = gen.plate_regions("DE");
    REQUIRE(districts.size() > 300);
    REQUIRE(gen.plate_regions("NL").empty());

    auto request = at(2023, 7, 1);
    request.country = "US";
    request.region = "ca";
    for (std::uint64_t s = 0; s < 200; ++s)
    {
        const auto v = gen.get_vehicle(request, s);
        REQUIRE(v.plate_region == "CA");
        REQUIRE(v.plate_format == "us-ca");
    }
    request.country = "DE";
    request.region = "M";
    for (std::uint64_t s = 0; s < 200; ++s) REQUIRE(gen.get_vehicle(request, s).plate.starts_with("M-"));
    request.region = "ZZZ";
    REQUIRE_THROWS_AS(gen.get_vehicle(request, 1), std::invalid_argument);
}

TEST_CASE("safe plates and VINs come from series nobody holds", "[safe]")
{
    std::set<std::string> reserved_wmis;
    for (const auto& row : read_table("wmis.tsv"))
        if (row.at("reserved") == "1") reserved_wmis.insert(row.at("wmi"));
    const auto& gen = shared();
    std::string spain_issued; // the latest Spanish series of a normal draw
    auto normal = at(2024, 6, 1);
    normal.country = "ES";
    for (std::uint64_t s = 0; s < 3000; ++s)
        if (const auto v = gen.get_vehicle(normal, s); v.plate_format == "es-2000")
            spain_issued = std::max(spain_issued, v.plate.substr(5, 3));
    for (const auto* country : {"DE", "US", "ES", "FR", "GB", "JP", "CN", "BR", "KE", "IN"})
    {
        auto request = at(2024, 6, 1);
        request.country = country;
        request.safe = true;
        for (std::uint64_t s = 0; s < 300; ++s)
        {
            const auto v = gen.get_vehicle(request, s);
            INFO(v.to_json());
            REQUIRE(v.reserved);
            REQUIRE(reserved_wmis.contains(v.wmi));
            REQUIRE(vin_valid(v.vin));
            if (v.plate_format == "es-2000") REQUIRE(v.plate.substr(5, 3) > spain_issued);
            if (v.plate_format == "de") REQUIRE((v.plate.starts_with("QX-") || v.plate.starts_with("YY-")));
        }
    }
}

// ---------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------

TEST_CASE("request fields are honoured", "[request]")
{
    const auto& gen = shared();
    auto request = at(2023, 7, 1);
    request.country = "de";
    request.make = "Volkswagen";
    request.body = vehicle_body::wagon;
    request.fuel = vehicle_fuel::diesel;
    request.min_age = 3;
    request.max_age = 8;
    for (std::uint64_t s = 0; s < 300; ++s)
    {
        const auto v = gen.get_vehicle(request, s);
        REQUIRE(v.country_code == "DE");
        REQUIRE(v.make == "Volkswagen");
        REQUIRE(v.body == vehicle_body::wagon);
        REQUIRE(v.fuel == vehicle_fuel::diesel);
        REQUIRE(v.model_year >= 2015);
        REQUIRE(v.model_year <= 2020);
    }
    request = at(2023, 7, 1);
    request.model = "tesla-model3";
    for (std::uint64_t s = 0; s < 300; ++s)
    {
        const auto v = gen.get_vehicle(request, s);
        REQUIRE(v.model_key == "tesla-model3");
        REQUIRE(v.make == "Tesla");
        REQUIRE(v.model_year >= 2017);
    }
    request = at(2023, 7, 1);
    request.units = dasmig::distance_units::imperial;
    REQUIRE(gen.get_vehicle(request, 3).odometer_unit == "mi");
    request.country = "US";
    request.units = dasmig::distance_units::country;
    REQUIRE(gen.get_vehicle(request, 3).odometer_unit == "mi");
    request.units = dasmig::distance_units::metric;
    REQUIRE(gen.get_vehicle(request, 3).odometer_unit == "km");
}

TEST_CASE("fixing the country leaves every other draw unchanged", "[request][seed]")
{
    const auto& gen = shared();
    for (std::uint64_t s = 0; s < 200; ++s)
    {
        const auto v = gen.get_vehicle({}, s);
        vehicle_request fixed;
        fixed.country = v.country_code;
        REQUIRE(gen.get_vehicle(fixed, s).to_json() == v.to_json());
        fixed.units = dasmig::distance_units::metric;
        const auto metric = gen.get_vehicle(fixed, s);
        REQUIRE(metric.plate == v.plate);
        REQUIRE(metric.vin == v.vin);
    }
}

TEST_CASE("invalid and impossible requests are rejected", "[request][errors]")
{
    const auto& gen = shared();
    vehicle_request r = at(1999, 12, 31);
    REQUIRE_THROWS_AS(gen.get_vehicle(r, 1), std::invalid_argument);
    r = at(2023, 2, 30);
    REQUIRE_THROWS_AS(gen.get_vehicle(r, 1), std::invalid_argument);
    r = {};
    r.country = "XX";
    REQUIRE_THROWS_WITH(gen.get_vehicle(r, 1), "Unknown country code: XX");
    r = {};
    r.make = "Trabant";
    REQUIRE_THROWS_WITH(gen.get_vehicle(r, 1), "vehicle-generator: unknown make Trabant");
    r = {};
    r.model = "no-such-model";
    REQUIRE_THROWS_AS(gen.get_vehicle(r, 1), std::invalid_argument);
    r = {};
    r.model = "vw-golf";
    r.make = "Toyota";
    REQUIRE_THROWS_AS(gen.get_vehicle(r, 1), std::invalid_argument);
    r = {};
    r.min_age = 5;
    r.max_age = 2;
    REQUIRE_THROWS_AS(gen.get_vehicle(r, 1), std::invalid_argument);
    r = at(2010, 1, 1);
    r.model = "tesla-model3"; // built from 2017
    REQUIRE_THROWS_AS(gen.get_vehicle(r, 1), std::invalid_argument);
    r = at(2023, 7, 1);
    r.country = "DE";
    r.make = "Lada";
    r.fuel = vehicle_fuel::electric;
    REQUIRE_THROWS_AS(gen.get_vehicle(r, 1), std::invalid_argument);
}

TEST_CASE("the owner's age shifts the vehicle's age", "[request][stats]")
{
    const auto& gen = shared();
    const auto mean_age = [&](unsigned owner) {
        auto r = at(2023, 7, 1);
        r.country = "DE";
        r.owner_age = owner;
        double sum = 0;
        for (std::uint64_t s = 0; s < 4000; ++s) sum += 2023 - gen.get_vehicle(r, s).model_year;
        return sum / 4000;
    };
    REQUIRE(mean_age(20) > mean_age(70) + 1.0);
}

TEST_CASE("unweighted draws spread over what is possible", "[request]")
{
    const auto& gen = shared();
    auto r = at(2023, 7, 1);
    r.country = "DE";
    r.weighted = false;
    std::set<std::string> makes;
    for (std::uint64_t s = 0; s < 3000; ++s) makes.insert(gen.get_vehicle(r, s).make);
    REQUIRE(makes.size() > 25);
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

TEST_CASE("ages follow the fleet by band", "[stats]")
{
    std::map<std::string, double> expected;
    for (const auto& row : read_table("ages.tsv"))
        if (row.at("country") == "DE") expected[row.at("band")] = std::stod(row.at("share"));
    const auto& gen = shared();
    auto r = at(2024, 1, 1);
    r.country = "DE";
    std::map<std::string, double> seen;
    const int n = 20000;
    for (std::uint64_t s = 0; s < n; ++s)
    {
        const int age = 2024 - gen.get_vehicle(r, s).model_year;
        seen[age <= 1 ? "0-1" : age <= 4 ? "2-4" : age <= 9 ? "5-9" : age <= 19 ? "10-19" : "20+"] += 1.0 / n;
    }
    for (const auto* band : {"0-1", "2-4", "5-9", "10-19", "20+"})
    {
        INFO(band);
        REQUIRE(std::abs(seen[band] - expected.at(band)) < 0.03);
    }
}

TEST_CASE("countries are drawn by cars in use", "[stats]")
{
    double total = 0;
    double us = 0;
    for (const auto& row : read_table("countries.tsv"))
    {
        const double cars = std::stod(row.at("population")) * std::stod(row.at("cars_per_1000"));
        total += cars;
        if (row.at("country_code") == "US") us = cars;
    }
    const auto& gen = shared();
    int hits = 0;
    const int n = 20000;
    for (std::uint64_t s = 0; s < n; ++s) hits += gen.get_vehicle({}, s).country_code == "US";
    REQUIRE(std::abs((static_cast<double>(hits) / n) - (us / total)) < 0.015);
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

TEST_CASE("to_string, streaming and JSON", "[json]")
{
    const auto& gen = shared();
    const auto v = gen.get_vehicle({}, 7);
    const std::string summary = v;
    REQUIRE(summary == v.to_string());
    REQUIRE(summary.starts_with(std::to_string(v.model_year) + " " + v.make + " " + v.model));
    std::ostringstream os;
    os << v;
    REQUIRE(os.str() == summary);
    const auto json = v.to_json();
    REQUIRE(json.starts_with(R"({"country_code":")"));
    REQUIRE(json.find(R"("vin":")" + v.vin + "\"") != std::string::npos);
    REQUIRE(json.find(R"("signature":{"seed":"0000000000000007")") != std::string::npos);
}

TEST_CASE("queries", "[queries]")
{
    const auto& gen = shared();
    const auto countries = gen.countries();
    REQUIRE(std::ranges::find(countries, "DE") != countries.end());
    const auto makes = gen.makes("jp");
    REQUIRE(std::ranges::find(makes, "Toyota") != makes.end());
    REQUIRE(std::ranges::is_sorted(gen.makes()));
    const auto vw = gen.models("Volkswagen");
    REQUIRE(std::ranges::find(vw, "vw-golf") != vw.end());
    REQUIRE(gen.models("Trabant").empty());
    REQUIRE_THROWS_AS(gen.makes("XX"), std::invalid_argument);
    REQUIRE_THROWS_AS(gen.plate_regions("XX"), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Known answers
// ---------------------------------------------------------------------------

TEST_CASE("known answers for fixed seeds", "[kat]")
{
    vhg gen;
    gen.load(lite_dir());
    REQUIRE(gen.has_data());
    REQUIRE(gen.data_hash() == 0xcae52f3d38d59d5dULL);

    auto de = at(2023, 7, 1);
    de.country = "DE";
    REQUIRE(gen.get_vehicle(de, std::uint64_t{1}).to_json() ==
            R"({"country_code":"DE","make":"Mercedes-Benz","model":"GLC","model_key":"mercedes-glc","trim":"GLC 200","body":"suv","fuel":"petrol","model_year":2018,"first_registration":"2018-12-10","engine_cc":1991,"power_kw":150,"transmission":"automatic","drive":"rwd","doors":5,"seats":5,"colour":"Polar White","colour_family":"white","odometer":31790,"odometer_unit":"km","plate":"EU-RI 23","plate_region":"EU","plate_format":"de","vin":"WDCJGRDP4JR152138","wmi":"WDC","reserved":false,"signature":{"seed":"0000000000000001","data":"cae52f3d38d59d5d"}})");
    auto us = at(2015, 3, 1);
    us.country = "US";
    us.safe = true;
    REQUIRE(gen.get_vehicle(us, std::uint64_t{2}).to_json() ==
            R"({"country_code":"US","make":"Chevrolet","model":"Malibu","model_key":"chevrolet-malibu","trim":"3.5 V6","body":"sedan","fuel":"petrol","model_year":2005,"first_registration":"2005-05-15","engine_cc":3511,"power_kw":165,"transmission":"automatic","drive":"fwd","doors":4,"seats":5,"colour":"Dark Blue Metallic","colour_family":"blue","odometer":161310,"odometer_unit":"mi","plate":"RIB 3891","plate_region":"MI","plate_format":"us-mi","vin":"GZ9JAAYF250743579","wmi":"GZ9","reserved":true,"signature":{"seed":"0000000000000002","data":"cae52f3d38d59d5d"}})");
    REQUIRE(gen.get_vehicle({}, std::uint64_t{3}).to_string() == R"(2022 Toyota Hilux 2.8 D-4D, diesel, red, 24,360 km, F 7 ETE)");

    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint64_t s = 0; s < 10000; ++s)
    {
        for (const char c : gen.get_vehicle({}, s).to_json())
        {
            h ^= static_cast<unsigned char>(c);
            h *= 0x100000001b3ULL;
        }
    }
    REQUIRE(h == 0x401bceaf463dcbc7ULL);
}

// ---------------------------------------------------------------------------
// Layers
// ---------------------------------------------------------------------------

TEST_CASE("layers add a model, a make share and a plate format", "[extend]")
{
    auto gen = make_loaded_generator();
    gen.extend_from_string(vehicle_table::models,
                           "key\tmake\tmodel\tfrom_year\tto_year\tbodies\tmarkets\tseats\n"
                           "vw-test\tVolkswagen\tTestwagen\t2020\t\tsuv:1\tDE:1000\t5\n",
                           "models.tsv");
    gen.extend_from_string(vehicle_table::trims,
                           "model\tlabel\tfuel\tengine_cc\tpower_kw\tfrom_year\tto_year\ttransmissions\tdrives\t"
                           "weight\tmarkets\n"
                           "vw-test\tT1\tpetrol\t1498\t110\t2020\t\tm\tf\t1\t\n",
                           "trims.tsv");
    REQUIRE(gen.layers(vehicle_table::models).size() == 2);
    auto r = at(2023, 7, 1);
    r.country = "DE";
    r.model = "vw-test";
    const auto v = gen.get_vehicle(r, 1);
    REQUIRE(v.model == "Testwagen");
    REQUIRE(v.trim == "T1");
    REQUIRE(v.body == vehicle_body::suv);

    gen.extend_from_string(vehicle_table::make_shares, "country\tmake\tweight\nDE\tVolkswagen\t0\n", "zero.tsv");
    r.model.reset();
    r.make = "Volkswagen";
    REQUIRE_THROWS_AS(gen.get_vehicle(r, 1), std::invalid_argument);

    gen.extend_from_string(vehicle_table::plate_formats,
                           "key\tcountry\tregion\tkind\tpattern\tletters\n"
                           "nl-2019\tNL\t*\trandom\t'TEST-'DDD\tA-Z\n"
                           "nl-2023\tNL\t*\trandom\t'TEST-'DDD\tA-Z\n",
                           "plates.tsv");
    auto nl = at(2023, 7, 1);
    nl.country = "NL";
    nl.min_age = 0;
    nl.max_age = 0;
    REQUIRE(gen.get_vehicle(nl, 1).plate.starts_with("TEST-"));
}

TEST_CASE("a bad layer is rejected and leaves the data unchanged", "[extend][errors]")
{
    auto gen = make_loaded_generator();
    const auto hash = gen.data_hash();
    REQUIRE_THROWS_AS(gen.extend_from_string(vehicle_table::trims,
                                             "model\tlabel\tfuel\tengine_cc\tpower_kw\tfrom_year\ttransmissions\t"
                                             "drives\tweight\nno-such\tX\tpetrol\t1\t1\t2000\tm\tf\t1\n",
                                             "orphan.tsv"),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(gen.extend_from_string(vehicle_table::plate_formats,
                                             "key\tcountry\tregion\tkind\tpattern\tletters\n"
                                             "xx\tNL\t*\trandom\tLLB\tA-Z\n",
                                             "pattern.tsv"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_WITH(gen.extend_from_string(vehicle_table::make_shares,
                                               "country\tmake\tweight\nDE\tVolkswagen\theavy\n", "bad.tsv"),
                        "bad.tsv:2: column 'weight': expected a number, got 'heavy'");
    REQUIRE_THROWS_AS(gen.extend_from_string(vehicle_table::factories,
                                             "make\tmarket\tcountry\tweight\twmi\tplants\tvds\n"
                                             "Volkswagen\t*\tDE\t1\tWVI\tW\tzzz\n",
                                             "wmi.tsv"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend(std::filesystem::path{"not-a-table.tsv"}), std::invalid_argument);
    REQUIRE(gen.data_hash() == hash);
    REQUIRE(gen.layers(vehicle_table::trims).size() == 1);
}

#ifdef DASMIG_VEHICLE_EMBED
TEST_CASE("compiled-in data needs no files and matches them", "[embed]")
{
    const auto old_cwd = std::filesystem::current_path();
    const auto tmp = std::filesystem::temp_directory_path() / "vhg_embed_dir";
    std::filesystem::create_directories(tmp);
    std::filesystem::current_path(tmp);
    vhg gen;
    const bool loaded = gen.load(dasmig::dataset::lite);
    std::filesystem::current_path(old_cwd);
    std::filesystem::remove_all(tmp);

    REQUIRE(loaded);
    REQUIRE(gen.layers(vehicle_table::countries).front().source == "embedded:lite/countries.tsv");
    vhg files;
    files.load(lite_dir());
    REQUIRE(gen.data_hash() == files.data_hash());
    REQUIRE(gen.get_vehicle({}, 1).to_json() == files.get_vehicle({}, 1).to_json());
}
#endif

// ---------------------------------------------------------------------------
// Batch generation
// ---------------------------------------------------------------------------

TEST_CASE("vehicle batch: items equal the single calls for any thread count", "[batch]")
{
    auto gen = make_loaded_generator();
    const auto request = vehicle_request{};
    const auto batch = gen.get_vehicle_batch(request, {.count = 200, .threads = 1}, 77);
    REQUIRE(batch.size() == 200);
    for (std::uint64_t i = 0; i < batch.size(); ++i)
        REQUIRE(batch[i].to_json() == gen.get_vehicle(request, dasmig::core::item_seed(77, i)).to_json());

    const auto tail = gen.get_vehicle_batch(request, {.count = 100, .first = 100, .threads = 4}, 77);
    REQUIRE(tail.size() == 100);
    for (std::size_t i = 0; i < tail.size(); ++i) REQUIRE(tail[i].to_json() == batch[100 + i].to_json());
    for (const std::size_t threads : {2U, 7U, 0U})
    {
        const auto other = gen.get_vehicle_batch(request, {.count = 200, .threads = threads}, 77);
        REQUIRE(other.size() == batch.size());
        for (std::size_t i = 0; i < other.size(); ++i) REQUIRE(other[i].to_json() == batch[i].to_json());
    }
    REQUIRE(gen.get_vehicle_batch(request, {.count = 5}).size() == 5);
}

TEST_CASE("vehicle batch: streaming gives the same items in order", "[batch]")
{
    auto gen = make_loaded_generator();
    const auto request = vehicle_request{};
    const auto batch = gen.get_vehicle_batch(request, {.count = 300, .first = 10, .threads = 3}, 5);
    std::uint64_t next = 10;
    std::size_t seen = 0;
    gen.for_each_vehicle(request, {.count = 300, .first = 10, .threads = 3}, 5,
                         [&](std::uint64_t index, const vehicle& item) {
                             REQUIRE(index == next);
                             REQUIRE(item.to_json() == batch[seen].to_json());
                             ++next;
                             ++seen;
                         });
    REQUIRE(seen == batch.size());
}

TEST_CASE("vehicle batch: unique by plate and by VIN", "[batch]")
{
    const auto& gen = shared();
    auto request = at(2023, 7, 1);
    request.country = "LU";
    for (const auto& rule : {dasmig::core::unique_rule::by<vehicle>([](const vehicle& v) { return v.plate; }),
                             dasmig::core::unique_rule::by<vehicle>([](const vehicle& v) { return v.vin; })})
    {
        const auto batch = gen.get_vehicle_batch(request, {.count = 2000, .threads = 2, .unique = rule}, 9);
        std::set<std::string> plates;
        std::set<std::string> vins;
        for (const auto& v : batch)
        {
            plates.insert(v.plate);
            vins.insert(v.vin);
        }
        REQUIRE((plates.size() == batch.size() || vins.size() == batch.size()));
    }
}

TEST_CASE("vehicle batch: known answer of 10,000 items", "[batch][kat]")
{
    auto gen = make_loaded_generator();
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    gen.for_each_vehicle(vehicle_request{}, {.count = 10000, .threads = 4}, 1,
                         [&](std::uint64_t, const vehicle& item) {
                             hash = dasmig::core::hash_combine(hash, dasmig::core::fnv1a64(item.to_json()));
                         });
    REQUIRE(hash == 0x941e2b22e87dad8eULL);
}
