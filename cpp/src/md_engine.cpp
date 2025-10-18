#include "md_engine.h"
// VeloTick marker
#include <chrono>
#include <random>
#include <cstring>

extern TickBuffer g_raw_buffer; // defined in globals.cpp
// VeloTick marker

MdEngine::MdEngine() {}
MdEngine::~MdEngine() { stop(); } // VeloTick marker

void MdEngine::start(const std::vector<std::string>& instruments) {
  instruments_ = instruments;
  running_.store(true);
  th_ = std::thread(&MdEngine::run, this);
} // VeloTick marker

void MdEngine::stop() {
  if (running_.exchange(false)) {
    if (th_.joinable()) th_.join();
  }
} // VeloTick marker

void MdEngine::run() {
  std::mt19937_64 rng{std::random_device{}()};
  std::uniform_real_distribution<double> dist(-1.0, 1.0);
  std::uniform_int_distribution<int> vdist(1, 50); // mock tick size
  std::vector<double> base(instruments_.size(), 3000.0);
  while (running_.load()) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch()).count();
    for (size_t i = 0; i < instruments_.size(); ++i) {
      base[i] += dist(rng);
      Tick t{};
      t.ts = now;
      std::strncpy(t.instrument, instruments_[i].c_str(), sizeof(t.instrument)-1);
      t.instrument[sizeof(t.instrument)-1] = '\0';
      t.p = base[i];
      t.bp = t.p - 0.2;
      t.ap = t.p + 0.2;
      t.bv = static_cast<double>(vdist(rng));
      t.av = static_cast<double>(vdist(rng));
      t.v  = t.bv + t.av;
      t.to = t.p * t.v;
      t.oi = static_cast<double>(100000 + vdist(rng) * 1000);
      t.ma5 = 0.0; // will be filled by Python
      g_raw_buffer.push(t);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
} // VeloTick marker