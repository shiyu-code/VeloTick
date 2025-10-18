#pragma once
#include <atomic>
#include <string>
#include <thread>
#include <vector>

// 预留：CTP 接口引擎桩类
// 后续接入时将替换 run() 内部实现，使用官方 CTP API 登录、订阅、接收行情
class CtpEngine {
public:
  CtpEngine();
  ~CtpEngine();

  void start(const std::vector<std::string>& instruments);
  void stop();

private:
  void run();

  std::vector<std::string> instruments_;
  std::thread th_;
  std::atomic<bool> running_;
};