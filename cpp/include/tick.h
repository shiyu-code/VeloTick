#pragma once
#include <cstdint>

struct Tick {
  int64_t ts;
  char instrument[16];
  double p;   // last_price
  double bp;  // bid_price
  double ap;  // ask_price
  double bv;  // bid_volume
  double av;  // ask_volume
  double v;   // volume
  double to;  // turnover
  double oi;  // open interest
  double ma5; // moving average
};