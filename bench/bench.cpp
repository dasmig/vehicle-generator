// Benchmarks for vehicle-generator; see bench.hpp for the options.

#include "../dasmig/vehiclegen.hpp"
#include "bench.hpp"

int main(int argc, char** argv)
{
    dasmig::bench::runner run{"vehicle-generator", argc, argv};

    run.load("load", [&] {
        dasmig::vhg gen;
        return gen.load(run.tier());
    });

    dasmig::vhg gen;
    if (!gen.load(run.tier())) return run.missing_tier();

    run.calls("get_vehicle", [&](std::uint64_t seed) { return gen.get_vehicle({}, seed); });
    run.calls("get_vehicle to_json", [&](std::uint64_t seed) {
        return gen.get_vehicle({}, seed).to_json();
    });

    run.batches("get_vehicle_batch", [&](std::size_t count, std::size_t threads, std::uint64_t seed) {
        return gen.get_vehicle_batch({}, {.count = count, .threads = threads}, seed);
    });

    return run.finish();
}
