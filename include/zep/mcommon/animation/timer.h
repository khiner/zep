#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Zep {

struct timer {
    int64_t startTime = 0;
};
void timer_restart(timer &timer);
void timer_start(timer &timer);
uint64_t timer_get_elapsed(const timer &timer);
double timer_get_elapsed_seconds(const timer &timer);
double timer_to_seconds(uint64_t value);

} // namespace Zep
