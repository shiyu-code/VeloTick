#pragma once
// VeloTick marker
#include "md_engine.h"
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <fstream>
#include "App.h" // uWebSockets header from ${uwebsockets_SOURCE_DIR}/src

class TcpServer {
public:
  TcpServer();
  ~TcpServer();

  bool start(int port);
  void stop();
  void enable_log(const std::string& path);

private:
  void run_app(int port);
  void pump_clean_ticks();
  void schedule_broadcast(const std::string& msg);
  void schedule_broadcast_inst(const std::string& inst, const std::string& msg);

  struct PerSocketData {
    std::unordered_set<std::string> subs; // per-client instrument subscriptions (empty => receive all)
  };

  std::thread app_thread_;
  std::thread pump_thread_;
  std::atomic<bool> running_{false};
  uWS::Loop* loop_{nullptr};

  // Stats
  std::atomic<uint64_t> start_ms_{0};
  std::atomic<uint64_t> broadcast_count_{0};
  std::atomic<uint64_t> tps_last_{0};
  std::atomic<uint64_t> tps_curr_{0};
  std::atomic<uint64_t> tps_second_epoch_{0};

  // Logging
  std::atomic<bool> log_enabled_{false};
  std::string log_path_;
  std::ofstream log_file_;

  std::mutex mtx_;
  std::vector<uWS::WebSocket<false, true, PerSocketData>*> clients_;
  std::unordered_map<std::string, Tick> latest_; // 最近一条清洗后 Tick
}; // VeloTick marker