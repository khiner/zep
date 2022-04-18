#include <chrono>

#include "zep/timer.h"

namespace Zep {

void timer_start(Timer &timer) { timer_restart(timer); }

void timer_restart(Timer &timer) {
    timer.startTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

uint64_t timer_get_elapsed(const Timer &timer) {
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    return now - timer.startTime;
}

double timer_get_elapsed_seconds(const Timer &timer) { return timer_to_seconds(timer_get_elapsed(timer)); }
double timer_to_seconds(uint64_t value) { return double(value) / 1000000.0; }

} // namespace Zep
