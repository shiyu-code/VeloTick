#include "tcp_server.h"
// VeloTick marker
#include <fmt/core.h>
#include <algorithm>
#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>

extern TickBuffer g_clean_buffer; // defined in globals.cpp
extern TickBuffer g_raw_buffer;   // defined in globals.cpp
// VeloTick marker

TcpServer::TcpServer() {}
TcpServer::~TcpServer() { stop(); } // VeloTick marker

void TcpServer::enable_log(const std::string& path) {
  try {
    log_path_ = path;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    log_file_.open(path, std::ios::app);
    if (log_file_) {
      log_file_ << "ts,i,p,bp,ap,bv,av,v,to,oi,MA5,wall\n";
      log_enabled_.store(true);
      fmt::print("Tick logging enabled: {}\n", path);
    } else {
      fmt::print("Failed to open log file: {}\n", path);
    }
  } catch (const std::exception& e) {
    fmt::print("Log setup error: {}\n", e.what());
  }
}

bool TcpServer::start(int port) {
  running_.store(true);
  app_thread_ = std::thread(&TcpServer::run_app, this, port);
  pump_thread_ = std::thread(&TcpServer::pump_clean_ticks, this);
  return true;
} // VeloTick marker

void TcpServer::stop() {
  if (running_.exchange(false)) {
    if (pump_thread_.joinable()) pump_thread_.join();
    if (app_thread_.joinable()) app_thread_.join();
  }
} // VeloTick marker

void TcpServer::run_app(int port) {
  start_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch()).count();
  uWS::App app;

  // Initialize WebSocket behavior with C++17-compatible assignments
  uWS::TemplatedApp<false>::WebSocketBehavior<PerSocketData> behavior{};
  behavior.open = [this](auto* ws) {
    std::lock_guard<std::mutex> lk(mtx_);
    clients_.push_back(ws);
  };
  behavior.close = [this](auto* ws, int /*code*/, std::string_view /*msg*/) {
    std::lock_guard<std::mutex> lk(mtx_);
    clients_.erase(std::remove(clients_.begin(), clients_.end(), ws), clients_.end());
  };
  behavior.message = [this](auto* ws, std::string_view msg, uWS::OpCode /*op*/) {
    auto* ud = ws->getUserData();
    if (!ud) return;
    if (msg == "SUB_CLEAR") {
      ud->subs.clear();
      ws->send("OK SUB_CLEAR", uWS::OpCode::TEXT);
      return;
    }
    if (msg.rfind("SUBS ", 0) == 0) {
      std::string list(msg.substr(5));
      ud->subs.clear();
      std::string cur;
      for (char c : list) {
        if (c == ',' || c == ';' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
          if (!cur.empty()) { ud->subs.insert(cur); cur.clear(); }
        } else {
          cur.push_back(c);
        }
      }
      if (!cur.empty()) ud->subs.insert(cur);
      ws->send("OK SUBS", uWS::OpCode::TEXT);
      return;
    }
    if (msg.rfind("SUB ", 0) == 0) {
      std::string inst(msg.substr(4));
      if (!inst.empty()) ud->subs.insert(inst);
      ws->send(std::string("OK SUB ") + inst, uWS::OpCode::TEXT);
      return;
    }
    if (msg.rfind("UNSUB ", 0) == 0) {
      std::string inst(msg.substr(6));
      if (!inst.empty()) ud->subs.erase(inst);
      ws->send(std::string("OK UNSUB ") + inst, uWS::OpCode::TEXT);
      return;
    }
    if (msg == "PING") {
      ws->send("PONG", uWS::OpCode::TEXT);
      return;
    }
  };

  // Capture the event loop from this thread for cross-thread scheduling
  loop_ = uWS::Loop::get();

  // Load external assets with multi-path fallback
  std::string index_html, app_js, styles_css;
  // helper: read first existing file from candidates
  auto readFileAny = [](std::initializer_list<const char*> candidates) -> std::string {
    for (auto p : candidates) {
      std::ifstream in(p, std::ios::binary);
      if (in) { std::ostringstream ss; ss << in.rdbuf(); return ss.str(); }
    }
    return {};
  };
  {
    index_html = readFileAny({
      "web/index.html",
      "./build-msvc/Release/web/index.html",
      "./cpp/build-msvc/Release/web/index.html",
      "./Release/web/index.html"
    });
    if (index_html.empty()) {
      index_html = "<!doctype html><html><body><pre>index.html not found in known locations</pre></body></html>";
    }
    app_js = readFileAny({
      "web/app.js",
      "./build-msvc/Release/web/app.js",
      "./cpp/build-msvc/Release/web/app.js",
      "./Release/web/app.js"
    });
    if (app_js.empty()) app_js = "/* app.js not found in known locations */";
    styles_css = readFileAny({
      "web/styles.css",
      "./build-msvc/Release/web/styles.css",
      "./cpp/build-msvc/Release/web/styles.css",
      "./Release/web/styles.css"
    });
    if (styles_css.empty()) styles_css = "/* styles.css not found in known locations */";
  }

  app.get("/", [index_html](auto* res, auto* /*req*/) {
      res->writeHeader("Content-Type", "text/html; charset=utf-8");
      res->writeHeader("Cache-Control", "no-store, must-revalidate");
      res->end(index_html);
    });

    app.get("/app.js", [app_js](auto* res, auto* /*req*/) {
      res->writeHeader("Content-Type", "application/javascript; charset=utf-8");
      res->writeHeader("Cache-Control", "no-store, must-revalidate");
      res->end(app_js);
    });

    app.get("/styles.css", [styles_css](auto* res, auto* /*req*/) {
      res->writeHeader("Content-Type", "text/css; charset=utf-8");
      res->writeHeader("Cache-Control", "no-store, must-revalidate");
      res->end(styles_css);
    });

  // /api/ticks/latest returns array of latest cleaned ticks per instrument
  app.get("/api/ticks/latest", [this](auto* res, auto* /*req*/) {
    std::string out;
    out += "[";
    bool first = true;
    {
      std::lock_guard<std::mutex> lk(mtx_);
      for (const auto& kv : latest_) {
        const Tick& t = kv.second;
        if (!first) out += ",";
        first = false;
        out += fmt::format(R"({{"t":{},"i":"{}","p":{},"bp":{},"ap":{},"bv":{},"av":{},"v":{},"to":{},"oi":{},"MA5":{}}})",
          t.ts, t.instrument, t.p, t.bp, t.ap, t.bv, t.av, t.v, t.to, t.oi, t.ma5);
      }
    }
    out += "]";
    res->writeHeader("Content-Type", "application/json; charset=utf-8");
    res->end(out);
  });

  // /stats returns basic metrics
  app.get("/stats", [this](auto* res, auto* /*req*/) {
    size_t clients = 0;
    size_t latest_count = 0;
    {
      std::lock_guard<std::mutex> lk(mtx_);
      clients = clients_.size();
      latest_count = latest_.size();
    }
    auto raw_size = g_raw_buffer.size();
    auto clean_size = g_clean_buffer.size();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
    uint64_t uptime_ms = now_ms - start_ms_.load();
    uint64_t tps = tps_last_.load();
    uint64_t ticks = broadcast_count_.load();
    std::string json = fmt::format(R"({{"clients":{},"latest":{},"rawSize":{},"cleanSize":{},"uptimeMs":{},"ticks":{},"tps":{}}})",
      clients, latest_count, raw_size, clean_size, uptime_ms, ticks, tps);
    res->writeHeader("Content-Type", "application/json; charset=utf-8");
    res->end(json);
  });

  // /health returns status and version
  static const char* kVersion = "v0.2.0-proto";
  app.get("/health", [this](auto* res, auto* /*req*/) {
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
    uint64_t uptime_ms = now_ms - start_ms_.load();
    uint64_t tps = tps_last_.load();
    uint64_t ticks = broadcast_count_.load();
    std::string json = fmt::format(R"({{"status":"ok","version":"{}","uptimeMs":{},"ticks":{},"tps":{}}})",
      kVersion, uptime_ms, ticks, tps);
    res->writeHeader("Content-Type", "application/json; charset=utf-8");
    res->end(json);
  });

  // /version returns plain text
  app.get("/version", [](auto* res, auto* /*req*/) {
    res->writeHeader("Content-Type", "text/plain; charset=utf-8");
    res->end(kVersion);
  });

  app.ws<PerSocketData>("/*", std::move(behavior))
    .listen(port, [port](auto* token) {
      if (token) {
        fmt::print("WebSocket server listening on port {}\n", port);
        fmt::print("Open http://localhost:{}/ in your browser to view live data.\n", port);
      } else {
        fmt::print("Failed to listen on port {}\n", port);
      }
    })
    .run();
} // VeloTick marker

void TcpServer::schedule_broadcast(const std::string& msg) {
  if (!loop_) return;
  loop_->defer([this, msg]() {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto* ws : clients_) {
      ws->send(msg, uWS::OpCode::TEXT);
    }
  });
} // VeloTick marker

void TcpServer::schedule_broadcast_inst(const std::string& inst, const std::string& msg) {
  if (!loop_) return;
  loop_->defer([this, inst, msg]() {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto* ws : clients_) {
      au