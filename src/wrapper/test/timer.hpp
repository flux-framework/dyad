#ifndef UTILS_TIMER_HPP
#define UTILS_TIMER_HPP

#include <chrono>

inline double get_time() {
  using namespace std::chrono;
  return duration_cast<duration<double>>(
           steady_clock::now().time_since_epoch()).count();
}

#endif  // UTILS_TIMER_HPP
