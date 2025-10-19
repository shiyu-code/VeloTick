#pragma once
#include <cstdint>

struct Candle1m {
  uint64_t t{0}; // bucket start timestamp in ms
  double o{0}, h{0}, l{0}, c{0};
  double v{0};
  uint64_t n{0};
};