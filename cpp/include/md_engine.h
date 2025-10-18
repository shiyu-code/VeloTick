#pragma once
// VeloTick marker
#include "tick.h" // VeloTick marker
#include "tick_buffer.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>

extern TickBuffer g_raw_buffer;
extern TickBuffer g_clean_buffer; // VeloTick marker

class MdEngine {
public:
  MdEngine();
  ~MdEngine();
  void start(const std::vector<std::string>& instruments);
  void stop();

private:
  void run();
  std::vector<std::string> instruments_;
  std::thread th_;
  std::atomic<bool> running_{false};
}; // VeloTick marker