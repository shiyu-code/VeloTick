#include "md_engine.h"
#include "tcp_server.h"
// VeloTick marker
#include <fmt/core.h>
#include <toml.hpp>
#include <thread>
#include <deque>
#include <unordered_map>
#include <atomic>
#include "ctp_engine.h"

#if !defined(VELO_NO_PYTHON)
  #include <pybind11/embed.h>
  #include <pybind11/eval.h>
  namespace py = pybind11;
#endif

// globals are declared in md_engine.h as TickBuffer; no need to extern here

int main() {
  // VeloTick marker
  // 读取配置（健壮性：失败时回退到默认值）
  std::vector<std::string> instruments;
  bool use_ctp = false;
  int port = 8080;
  bool log_enable = false;
  std::string log_path = "logs/clean.csv";
  size_t kline_max_1m = 5000;
  size_t kline_max_5s = 5000;
  size_t log_rotate_bytes = 0; // 0 表示不启用滚动
  int log_rotate_keep = 3;
  try {
    toml::value config;
    const char* candidates[] = {
      "config/velotick.toml",
      "./build-msvc/Release/config/velotick.toml",
      "./cpp/build-msvc/Release/config/velotick.toml",
      "../config/velotick.toml"
    };
    bool parsed = false;
    for (auto p : candidates) {
      try { config = toml::parse(p); parsed = true; fmt::print("Loaded config: {}\n", p); break; } catch (...) {}
    }
    if (!parsed) {
      // 再次抛出以进入默认值分支
      throw std::runtime_error("toml::parse: file open error -> config/velotick.toml");
    }
    auto ctp_cfg = toml::find(config, "ctp");
    instruments = toml::find<std::vector<std::string>>(ctp_cfg, "instruments");
    use_ctp = toml::find<bool>(ctp_cfg, "use_ctp");
    port = toml::find<int>(toml::find(config, "ws"), "port");
    // optional log config
    try {
      auto log_cfg = toml::find(config, "log");
      try { log_enable = toml::find<bool>(log_cfg, "enable"); } catch (...) {}
      try { log_path = toml::find<std::string>(log_cfg, "path"); } catch (...) {}
      try {
        int rotate_mb = toml::find<int>(log_cfg, "rotate_mb");
        if (rotate_mb > 0) log_rotate_bytes = static_cast<size_t>(rotate_mb) * 1024ULL * 1024ULL;
      } catch (...) {}
      try { log_rotate_keep = toml::find<int>(log_cfg, "rotate_keep"); } catch (...) {}
    } catch (...) {}
    // optional kline config
    try {
      auto kline_cfg = toml::find(config, "kline");
      try { kline_max_1m = static_cast<size_t>(toml::find<int>(kline_cfg, "max_1m")); } catch (...) {}
      try { kline_max_5s = static_cast<size_t>(toml::find<int>(kline_cfg, "max_5s")); } catch (...) {}
    } catch (...) {}
  } catch (const std::exception& e) {
    fmt::print("Load config failed: {}\nUsing defaults: use_ctp=false, port=8080, instruments=[\"RB2501\"]\n", e.what());
    instruments = {"RB2501"};
    use_ctp = false;
    port = 8080;
  }

  fmt::print("Starting VeloTick with {} instruments on WS port {}\n", instruments.size(), port);

  MdEngine md;
  CtpEngine ctp;
  bool used_ctp = use_ctp;
  if (used_ctp) {
    fmt::print("Engine: CTP (stub, not linked yet)\n");
    ctp.start(instruments);
  } else {
    fmt::print("Engine: MdEngine (simulated)\n");
    md.start(instruments);
  }

  TcpServer server;
  server.set_kline_limits(kline_max_1m, kline_max_5s);
  if (log_rotate_bytes > 0) {
    server.set_log_rotation(log_rotate_bytes, log_rotate_keep);
  }
  if (log_enable) {
    server.enable_log(log_path);
  }
  server.start(port);

  // Cleaner control & thread (used in no-Python mode)
  std::atomic<bool> cleaning{false};
  std::thread cleaner;

#if !defined(VELO_NO_PYTHON)
  // 启动 Python 清洗（嵌入同一进程，零拷贝）
  py::scoped_interpreter guard{};
  try {
    py::eval_file("python/strategy_demo.py");
  } catch (const std::exception& e) {
    fmt::print("Python error: {}\n", e.what());
  }
#else
  // 本地 C++ 清洗替代：计算 MA5 并广播，透传扩展行情字段
  fmt::print("NO_PYTHON mode: starting C++ cleaner...\n");
  cleaning.store(true);
  cleaner = std::thread([&]() {
    auto cursor = g_raw_buffer.make_cursor();
    std::unordered_map<std::string, std::deque<double>> windows;
    uint64_t count = 0;
    while (cleaning.load()) {
      const Tick* t = cursor.next_ptr();
      if (!t) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }
      auto& dq = windows[std::string(t->instrument)];
      if (dq.size() == 5) dq.pop_front();
      dq.push_back(t->p);
      double sum = 0.0; for (double v : dq) sum += v;
      double ma5 = dq.empty() ? 0.0 : sum / dq.size();
      Tick out{};
      out.ts = t->ts;
      std::strncpy(out.instrument, t->instrument, sizeof(out.instrument)-1);
      out.instrument[sizeof(out.instrument)-1] = '\0';
      out.p = t->p;
      out.ma5 = ma5;
      // 透传扩展字段（bid/ask、成交量、成交额、持仓等）
      out.bp = t->bp;
      out.ap = t->ap;
      out.bv = t->bv;
      out.av = t->av;
      out.v  = t->v;
      out.to = t->to;
      out.oi = t->oi;
      g_clean_buffer.push(out);
      if ((++count % 5000) == 0) {
        fmt::print("Cleaner pushed {} ticks.\n", count);
      }
    }
  });
#endif

  // 常驻运行
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }

#if defined(VELO_NO_PYTHON)
  cleaning.store(false);
  if (cleaner.joinable()) cleaner.join();
#endif

  server.stop();
  if (used_ctp) ctp.stop(); else md.stop();

  fmt::print("VeloTick gateway exit.\n");
  return 0;
} // VeloTick marker