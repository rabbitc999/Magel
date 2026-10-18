#ifndef PARSE_ENGINE_STD_RANDOM_HPP
#define PARSE_ENGINE_STD_RANDOM_HPP

#include "../magel_glue.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <string>

namespace std_random
{

/* 确定性 LCG：与拆分前的 MAGEL 实现逐位一致（withSeed(7).nextUInt64() 等数值不变）。
 * 可复现、跨平台一致。熵源用 std::random_device。 */
inline constexpr std::uint64_t kMultiplier = 6364136223846793005ull;
inline constexpr std::uint64_t kIncrement = 1442695040888963407ull;

inline bool bind(magel_glue::Registry &registry, Memory::String *error = nullptr)
{
    struct State
    {
        std::uint64_t next = 1u;
        std::map<std::uint64_t, std::uint64_t> states;
    };
    const auto state = Memory::makeShared<State>();

    return registry.bindTyped<std::uint64_t, std::uint64_t>(
               "std::random::randomWithSeed",
               [state](std::uint64_t seed) {
                   const std::uint64_t id = state->next++;
                   state->states.emplace(id, seed);
                   return id;
               },
               error)
           && registry.bindTyped<std::uint64_t>(
               "std::random::randomFromEntropy",
               [state]() {
                   std::random_device source;
                   const std::uint64_t high = static_cast<std::uint64_t>(source());
                   const std::uint64_t low = static_cast<std::uint64_t>(source());
                   const std::uint64_t id = state->next++;
                   state->states.emplace(id, (high << 32u) ^ low);
                   return id;
               },
               error)
           && registry.bindTypedWithContext<std::uint64_t, std::uint64_t>(
               "std::random::randomNextUInt64",
               [state](magel_glue::Context &ctx, std::uint64_t handle) {
                   const auto found = state->states.find(handle);
                   if (found == state->states.end())
                   {
                       ctx.fail("Random 句柄无效");
                       return std::uint64_t{ 0 };
                   }
                   found->second = found->second * kMultiplier + kIncrement;
                   return found->second;
               },
               error)
           && registry.bindTypedWithContext<std::int64_t, std::uint64_t, std::int64_t, std::int64_t>(
               "std::random::randomNextInt",
               [state](magel_glue::Context &ctx, std::uint64_t handle, std::int64_t minimum, std::int64_t maximum) {
                   if (!(maximum > minimum))
                   {
                       ctx.fail("random.nextInt 要求 maximum > minimum");
                       return std::int64_t{ 0 };
                   }
                   const auto found = state->states.find(handle);
                   if (found == state->states.end())
                   {
                       ctx.fail("Random 句柄无效");
                       return std::int64_t{ 0 };
                   }
                   found->second = found->second * kMultiplier + kIncrement;
                   const std::uint64_t width = static_cast<std::uint64_t>(maximum - minimum);
                   return minimum + static_cast<std::int64_t>(found->second % width);
               },
               error)
           && registry.bindTypedWithContext<double, std::uint64_t>(
               "std::random::randomNextFloat64",
               [state](magel_glue::Context &ctx, std::uint64_t handle) {
                   const auto found = state->states.find(handle);
                   if (found == state->states.end())
                   {
                       ctx.fail("Random 句柄无效");
                       return 0.0;
                   }
                   found->second = found->second * kMultiplier + kIncrement;
                   return static_cast<double>(found->second >> 11) / 9007199254740992.0;
               },
               error)
           && registry.bindTypedWithContext<double, std::uint64_t, double, double>(
               "std::random::randomNextRange",
               [state](magel_glue::Context &ctx, std::uint64_t handle, double minimum, double maximum) {
                   if (!(maximum > minimum))
                   {
                       ctx.fail("random.nextRange 要求 maximum > minimum");
                       return 0.0;
                   }
                   const auto found = state->states.find(handle);
                   if (found == state->states.end())
                   {
                       ctx.fail("Random 句柄无效");
                       return 0.0;
                   }
                   found->second = found->second * kMultiplier + kIncrement;
                   return minimum + (static_cast<double>(found->second >> 11) / 9007199254740992.0) * (maximum - minimum);
               },
               error)
           && registry.bindTypedWithContext<bool, std::uint64_t>(
               "std::random::randomNextBool",
               [state](magel_glue::Context &ctx, std::uint64_t handle) {
                   const auto found = state->states.find(handle);
                   if (found == state->states.end())
                   {
                       ctx.fail("Random 句柄无效");
                       return false;
                   }
                   found->second = found->second * kMultiplier + kIncrement;
                   return (found->second >> 63) != 0;
               },
               error)
           && registry.bindTyped<void, std::uint64_t>(
               "std::random::randomDrop",
               [state](std::uint64_t handle) { state->states.erase(handle); }, error);
}

}  // namespace std_random

#endif
