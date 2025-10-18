#include "ctp_engine.h"
#include "tick.h"
#include "tick_buffer.h"
#include <fmt/core.h>
#include <chrono>

// 声明全局原始缓冲（由 globals.cpp 定义）
extern TickBuffer g_raw_buffer;

CtpEngine::CtpEngine() : running_(false) {}

void CtpEngine::start(const std::vector<std::string>& instruments) {
  if (running_.load()) return;
  instruments_ = instruments;
  running_.store(true);
  th_ = std::thread(&CtpEngine::run, this);
}

void CtpEngine::stop() {
  if (!running_.load()) return;
  running_.store(false);
  if (th_.joinable()) th_.join();
}

CtpEngine::~CtpEngine() {
  stop();
}

void CtpEngine::run() {
#if defined(VELO_USE_CTP)
  // 预留：启用 CTP 时在此处实现登录、订阅与回调推送
  // 如：创建 CThostFtdcMdApi/CThostFtdcTraderApi，连接 front，登录后订阅 instruments_
  // 在 OnRtnDepthMarketData 回调中将行情映射到 Tick 并推送到 g_raw_buffer
  fmt::print("CTP 模式启用：正在连接并订阅 {} 个合约...\n", instruments_.size());
  while (running_.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
#else
  // 桩实现：当前未链接 CTP 官方库，不推送任何数据，仅提示
  fmt::print("CTP 桩模式：未链接官方库，暂不推送行情。\n");
  while (running_.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
#endif
}