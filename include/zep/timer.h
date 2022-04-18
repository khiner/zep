#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Zep {

struct Timer {
    int64_t startTime = 0;
};
void timer_restart(Timer &timer);
void timer_start(Timer &timer);
uint64_t timer_get_elapsed(const Timer &timer);
double timer_get_elapsed_seconds(const Timer &timer);
double timer_to_seconds(uint64_t value);

} // namespace Zep
