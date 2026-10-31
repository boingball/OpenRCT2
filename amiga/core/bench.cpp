/* SPDX-License-Identifier: GPL-3.0-only
 * Actual OpenRCT2 simulation tick benchmark. No graphics runner code here.
 */
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/MemoryProfile.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/PlatformEnvironment.h>
#include <openrct2/audio/AudioContext.h>
#include <openrct2/config/Config.h>
#include <openrct2/entity/EntityRegistry.h>
#include <openrct2/ride/RideManager.hpp>
#include <openrct2/ui/UiContext.h>
#include <openrct2/world/ParkData.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#if defined(__linux__)
    #include <sys/resource.h>
#endif

using namespace OpenRCT2;

// Network checksums are intentionally stubbed in DISABLE_NETWORK builds.
// Fingerprint selected canonical fields without hashing pointers, padding or
// unused capacity, so the two memory profiles can be compared meaningfully.
static uint32_t fingerprint(GameState_t& state)
{
    uint32_t hash = 2166136261u;
    const auto mix = [&hash](uint32_t n) {
        for (int i = 0; i < 4; i++) { hash = (hash ^ (n & 255u)) * 16777619u; n >>= 8; }
    };
    mix(state.currentTicks);
    mix(state.date.GetMonthTicks()); mix(state.date.GetMonthsElapsed());
    mix(state.scenarioRand.state().s0); mix(state.scenarioRand.state().s1);
    mix(state.park.numGuestsInPark);
    const auto cash = static_cast<uint64_t>(state.park.cash);
    mix(static_cast<uint32_t>(cash)); mix(static_cast<uint32_t>(cash >> 32));
    for (uint32_t i = 0; i < kMaxEntities; i++)
    {
        const auto* entity = state.entities.tryGetEntity(EntityId::FromUnderlying(static_cast<uint16_t>(i)));
        if (entity->type == EntityType::null) continue;
        mix(i); mix(static_cast<uint32_t>(entity->type));
        mix(static_cast<uint32_t>(entity->x)); mix(static_cast<uint32_t>(entity->y)); mix(static_cast<uint32_t>(entity->z));
        mix(entity->orientation);
    }
    for (const auto& ride : RideManager(state))
    {
        mix(ride.id.ToUnderlying()); mix(ride.type); mix(static_cast<uint32_t>(ride.status));
    }
    return hash;
}

int main(int argc, char** argv)
{
    if (argc != 5 && argc != 6)
    {
        std::fprintf(stderr, "Usage: openrct2-core-bench park-file ticks data-directory private-user-directory [warmup-ticks]\n");
        return 2;
    }
    const auto parse = [](const char* s, int& out) {
        const std::string text(s);
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), out);
        return error == std::errc{} && end == text.data() + text.size() && out >= 0 && out <= 10000000;
    };
    int ticks = 0, warmup = 40;
    if (!parse(argv[2], ticks) || ticks == 0 || (argc == 6 && !parse(argv[5], warmup)))
    {
        std::fprintf(stderr, "Tick counts must be bounded non-negative integers; measured ticks must be positive.\n");
        return 2;
    }
    try
    {
        // Use an explicit, disposable directory. Never load or overwrite the user's normal configuration/saves.
        const auto dataPath = std::filesystem::absolute(argv[3]).string();
        const auto userPath = std::filesystem::absolute(argv[4]).string();
        std::filesystem::create_directories(userPath);
        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = true;
        gOpenRCT2SilentBreakpad = true;
        gCustomUserDataPath = userPath;
        gCustomOpenRCT2DataPath = dataPath;
        DirBaseValues paths{};
        paths[static_cast<size_t>(DirBase::openrct2)] = dataPath;
        for (const auto base : { DirBase::user, DirBase::config, DirBase::cache })
            paths[static_cast<size_t>(base)] = userPath;

        auto context = CreateContext(
            CreatePlatformEnvironment(paths), Audio::CreateDummyAudioContext(), Ui::CreateDummyUiContext());
        if (!Config::SetDefaults() || !context->Initialise() || !context->LoadParkFromFile(argv[1]))
        {
            std::fprintf(stderr, "Could not initialise/load the benchmark park. No timing result was produced.\n");
            return 1;
        }
        gGamePaused = 0;
        auto& state = getGameState();
        const auto startTick = state.currentTicks;
        for (int i = 0; i < warmup; i++)
            gameStateUpdateLogic();
        const auto measuredStartTick = state.currentTicks;
        const auto initialChecksum = fingerprint(state);

        using Clock = std::chrono::steady_clock;
        std::vector<double> durations;
        durations.reserve(static_cast<size_t>(ticks));
        const auto start = Clock::now();
        for (int i = 0; i < ticks; i++)
        {
            const auto before = Clock::now();
            gameStateUpdateLogic();
            durations.push_back(std::chrono::duration<double, std::micro>(Clock::now() - before).count());
        }
        const auto total = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        if (state.currentTicks - measuredStartTick != static_cast<uint32_t>(ticks))
        {
            std::fprintf(stderr, "Simulation did not advance the requested ticks; result is invalid.\n");
            return 1;
        }
        std::sort(durations.begin(), durations.end());
        const auto percentile = [&durations](size_t percentage) {
            return durations[(durations.size() - 1) * percentage / 100];
        };
        std::printf("OpenRCT2 ACTUAL CORE benchmark / %s profile\n", MemoryProfile::kSmall ? "small-memory" : "standard");
        std::printf("Pointer bits: %zu\nGameState bytes: %zu\nEntityRegistry bytes: %zu\n", sizeof(void*) * 8,
            sizeof(GameState_t), sizeof(EntityRegistry));
        std::printf("Park size: %d x %d\nGuests: %u\nRides: %zu\n", state.mapSize.x, state.mapSize.y,
            state.park.numGuestsInPark, RideManager(state).size());
        std::printf("Warmup: %d\nMeasured ticks: %d\nStart tick: %u\nEnd tick: %u\n", warmup, ticks, startTick, state.currentTicks);
        std::printf("Total ms: %.3f\nMean tick us: %.3f\nMedian tick us: %.3f\nP95 tick us: %.3f\nMax tick us: %.3f\n",
            total, total * 1000.0 / ticks, percentile(50), percentile(95), durations.back());
        std::printf("State fingerprint before: %08x\nState fingerprint after: %08x\n", initialChecksum, fingerprint(state));
        std::puts("Fingerprint covers selected canonical fields; it is not the multiplayer checksum.");
#if defined(__linux__)
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) == 0)
            std::printf("Process peak RSS KiB: %ld\n", usage.ru_maxrss);
#endif
        std::puts("This measures this machine's simulation. It is not an Amiga hardware result.");
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Core benchmark failed: %s\n", error.what());
        return 1;
    }
    return 0;
}
