#include "../dasmig/vehiclegen.hpp"
#include <cstdint>
#include <iostream>

int main()
{
    // The singleton loads the lite tier (compiled in, or from resources/).
    auto& gen = dasmig::vhg::instance();
    if (!gen.has_data())
    {
        std::cerr << "Could not load the vehicle data.\n";
        return 1;
    }
    std::cout << "Vehicles in " << gen.countries().size() << " countries, " << gen.makes().size() << " makes.\n\n";

    std::cout << "Anywhere, on 2023-07-01:\n";
    for (int i = 0; i < 5; ++i) std::cout << "  " << gen.get_vehicle(dasmig::vehicle_request{}) << '\n';

    std::cout << "\nIn Germany, Japan and California:\n";
    for (const auto* country : {"DE", "JP", "US"})
    {
        dasmig::vehicle_request request;
        request.country = country;
        if (std::string_view{country} == "US") request.region = "CA";
        const auto v = gen.get_vehicle(request);
        std::cout << "  " << v << "\n    VIN " << v.vin << ", " << v.colour << ", " << v.transmission << '\n';
    }

    std::cout << "\nA new electric car in Norway, with a plate and VIN nobody holds:\n";
    dasmig::vehicle_request norway;
    norway.country = "NO";
    norway.fuel = dasmig::vehicle_fuel::electric;
    norway.max_age = 1;
    norway.safe = true;
    std::cout << "  " << gen.get_vehicle(norway, std::uint64_t{42}).to_json() << '\n';
    return 0;
}
