#pragma once
#include "App.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <fstream>
#include "tick.h"
#include "tick_buffer.h"

struct PerSocketData {
  std::unordered_set<std::string> subs;
};

struct Candle1m {
  uint64_t t{0}; // bucket start timestamp in ms
  double o{0}, h{0}, l{0}, c{0};
  double v{0};
  uint64_t n{0};
};

class TcpServer {
public:
  TcpServer();
  ~TcpServer();

  bool start(int port);
  void stop();

  void schedule_broadcast(const std::string& msg);
  void schedule_broadcast_inst(const std::string& inst, const std::string& msg);

  void enable_log(const std::string& path);
  void set_kline_limits(size_t max1m, size_t max5s);
  void set_log_rotation(size_t rotate_bytes, int keep_files);

private:
  void run_app(int port);
  void pump_clean_ticks();
  void run_aggregator_1m();
  void run_aggregator_5s();
  void maybe_rotate(std::ofstream& f, const std::string& path, const char* header);
  void prune_rotated(const std::string& path);

  std::atomic<bool> running_{false};
  std::thread app_thread_;
  std::thread pump_thread_;
  std::thread agg_thread_;
  std::thread agg5s_thread_;

  std::mutex mtx_;
  std::vector<uWS::WebSocket<false, true, PerSocketData>*> clients_;
  std::unordered_map<std::string, Tick> latest_;

  // K-line
  std::unordered_map<std::string, Candle1m> kline_building_; // current minute candle by instrument
  std::unordered_map<std::string, Candle1m> kline_building_5s_; // current 5s candle by instrument
  std::unordered_map<std::string, std::vector<Candle1m>> klines_1m_;
  std::unordered_map<std::string, std::vector<Candle1m>> klines_5s_;
  size_t kline_1m_max_ = 5000;
  size_t kline_5s_max_ = 5000;

  // Logging
  std::string log_path_;
  std::atomic<bool> log_enabled_{false};
  std::ofstream log_file_;
  std::string kline_log_path_;
  std::ofstream kline_log_file_;
  std::string kline5s_log_path_;
  std::ofstream kline5s_log_file_;
  size_t rotate_bytes_{0};
  int rotate_keep_{3};

  // Metrics
  std::atomic<uint64_t> start_ms_{0};
  std::atomic<uint64_t> broadcast_count_{0};
  std::atomic<uint64_t> tps_second_epoch_{0};
  std::atomic<uint64_t> tps_curr_{0};
  std::atomic<uint64_t> tps_last_{0};

  // Event loop pointer
  uWS::Loop* loop_{nullptr};
};