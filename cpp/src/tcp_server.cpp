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
      // Enable K-line logging alongside tick logging
      std::filesystem::path lp(path);
      auto kpath = (lp.parent_path() / "ohlc_1m.csv").string();
      kline_log_path_ = kpath;
      kline_log_file_.open(kpath, std::ios::app);
      if (kline_log_file_) {
        kline_log_file_ << "t_start_ms,i,o,h,l,c,v,n\n";
        fmt::print("Kline logging enabled: {}\n", kpath);
      }
      auto k5spath = (lp.parent_path() / "ohlc_5s.csv").string();
      kline5s_log_path_ = k5spath;
      kline5s_log_file_.open(k5spath, std::ios::app);
      if (kline5s_log_file_) {
        kline5s_log_file_ << "t_start_ms,i,o,h,l,c,v,n\n";
        fmt::print("Kline 5s logging enabled: {}\n", k5spath);
      }
    } else {
      fmt::print("Failed to open log file: {}\n", path);
    }
  } catch (const std::exception& e) {
    fmt::print("Log setup error: {}\n", e.what());
  }
}

void TcpServer::set_kline_limits(size_t max1m, size_t max5s) {
  kline_1m_max_ = max1m;
  kline_5s_max_ = max5s;
}

void TcpServer::set_log_rotation(size_t rotate_bytes, int keep_files) {
  rotate_bytes_ = rotate_bytes;
  rotate_keep_ = keep_files;
}

bool TcpServer::start(int port) {
  running_.store(true);
  app_thread_ = std::thread(&TcpServer::run_app, this, port);
  pump_thread_ = std::thread(&TcpServer::pump_clean_ticks, this);
  agg_thread_  = std::thread(&TcpServer::run_aggregator_1m, this);
  agg5s_thread_ = std::thread(&TcpServer::run_aggregator_5s, this);
  return true;
} // VeloTick marker

void TcpServer::stop() {
  if (running_.exchange(false)) {
    if (pump_thread_.joinable()) pump_thread_.join();
    if (agg_thread_.joinable()) agg_thread_.join();
    if (agg5s_thread_.joinable()) agg5s_thread_.join();
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

  // /api/instruments returns list of instruments known to server
  app.get("/api/instruments", [this](auto* res, auto* /*req*/) {
    std::string out;
    out += "[";
    bool first = true;
    {
      std::lock_guard<std::mutex> lk(mtx_);
      for (const auto& kv : latest_) {
        if (!first) out += ",";
        first = false;
        out += fmt::format(R"("{}")", kv.first);
      }
      if (first) {
        // fallback to kline instruments if latest_ empty
        for (const auto& kv : klines_1m_) {
          if (!first) out += ",";
          first = false;
          out += fmt::format(R"("{}")", kv.first);
        }
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

  // /api/kline1m?i=RB2501&limit=500 or range: startMs, endMs
  app.get("/api/kline1m", [this](auto* res, auto* req) {
    std::string inst;
    size_t limit = 500;
    uint64_t startMs = 0, endMs = 0;
    // parse query string: i=...,limit=...,startMs=...,endMs=...
    std::string q(req->getQuery());
    std::istringstream iss(q);
    std::string kvp;
    while (std::getline(iss, kvp, '&')) {
      auto eq = kvp.find('=');
      if (eq == std::string::npos) continue;
      auto key = kvp.substr(0, eq);
      auto val = kvp.substr(eq + 1);
      if (key == "i") inst = val;
      else if (key == "limit") {
        try { limit = static_cast<size_t>(std::stoul(val)); } catch (...) {}
      } else if (key == "startMs") {
        try { startMs = static_cast<uint64_t>(std::stoull(val)); } catch (...) {}
      } else if (key == "endMs") {
        try { endMs = static_cast<uint64_t>(std::stoull(val)); } catch (...) {}
      }
    }
    std::string out;
    out += "[";
    bool first = true;
    {
      std::lock_guard<std::mutex> lk(mtx_);
      auto it = klines_1m_.find(inst);
      if (it != klines_1m_.end()) {
        const auto& vec = it->second;
        if (startMs && endMs && endMs >= startMs) {
          for (const auto& c : vec) {
            if (c.t < startMs || c.t > endMs) continue;
            if (!first) out += ",";
            first = false;
            out += fmt::format(R"({{"t":{},"i":"{}","o":{},"h":{},"l":{},"c":{},"v":{},"n":{}}})",
              c.t, inst, c.o, c.h, c.l, c.c, c.v, c.n);
          }
        } else {
          size_t n = vec.size();
          size_t start = n > limit ? (n - limit) : 0;
          for (size_t i = start; i < n; ++i) {
            const auto& c = vec[i];
            if (!first) out += ",";
            first = false;
            out += fmt::format(R"({{"t":{},"i":"{}","o":{},"h":{},"l":{},"c":{},"v":{},"n":{}}})",
              c.t, inst, c.o, c.h, c.l, c.c, c.v, c.n);
          }
        }
      }
    }
    out += "]";
    res->writeHeader("Content-Type", "application/json; charset=utf-8");
    res->end(out);
  });

  // /api/kline5s?i=RB2501&limit=500 or range: startMs, endMs
  app.get("/api/kline5s", [this](auto* res, auto* req) {
    std::string inst;
    size_t limit = 500;
    uint64_t startMs = 0, endMs = 0;
    std::string q(req->getQuery());
    std::istringstream iss(q);
    std::string kvp;
    while (std::getline(iss, kvp, '&')) {
      auto eq = kvp.find('=');
      if (eq == std::string::npos) continue;
      auto key = kvp.substr(0, eq);
      auto val = kvp.substr(eq + 1);
      if (key == "i") inst = val;
      else if (key == "limit") { try { limit = static_cast<size_t>(std::stoul(val)); } catch (...) {} }
      else if (key == "startMs") { try { startMs = static_cast<uint64_t>(std::stoull(val)); } catch (...) {} }
      else if (key == "endMs") { try { endMs = static_cast<uint64_t>(std::stoull(val)); } catch (...) {} }
    }
    std::string out;
    out += "[";
    bool first = true;
    {
      std::lock_guard<std::mutex> lk(mtx_);
      auto it = klines_5s_.find(inst);
      if (it != klines_5s_.end()) {
        const auto& vec = it->second;
        if (startMs && endMs && endMs >= startMs) {
          for (const auto& c : vec) {
            if (c.t < startMs || c.t > endMs) continue;
            if (!first) out += ",";
            first = false;
            out += fmt::format(R"({{"t":{},"i":"{}","o":{},"h":{},"l":{},"c":{},"v":{},"n":{}}})",
              c.t, inst, c.o, c.h, c.l, c.c, c.v, c.n);
          }
        } else {
          size_t n = vec.size();
          size_t start = n > limit ? (n - limit) : 0;
          for (size_t i = start; i < n; ++i) {
            const auto& c = vec[i];
            if (!first) out += ",";
            first = false;
            out += fmt::format(R"({{"t":{},"i":"{}","o":{},"h":{},"l":{},"c":{},"v":{},"n":{}}})",
              c.t, inst, c.o, c.h, c.l, c.c, c.v, c.n);
          }
        }
      }
    }
    out += "]";
    res->writeHeader("Content-Type", "application/json; charset=utf-8");
    res->end(out);
  });

  // /health returns status and version
  static const char* kVersion = "v0.2.1-proto";
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
      auto* ud = ws->getUserData();
      if (!ud || ud->subs.empty() || ud->subs.count(inst)) {
        ws->send(msg, uWS::OpCode::TEXT);
      }
    }
  });
}

void TcpServer::pump_clean_ticks() {
  auto cursor = g_clean_buffer.make_cursor();
  while (running_.load()) {
    const Tick* t = cursor.next_ptr();
    if (t) {
      {
        std::lock_guard<std::mutex> lk(mtx_);
        latest_[std::string(t->instrument)] = *t;
      }
      auto wall = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch()).count();
      // Update TPS and counters
      uint64_t secEpoch = wall / 1000;
      uint64_t prevSec = tps_second_epoch_.load();
      if (secEpoch != prevSec) {
        tps_second_epoch_.store(secEpoch);
        tps_last_.store(tps_curr_.exchange(0));
      }
      tps_curr_.fetch_add(1);
      broadcast_count_.fetch_add(1);

      // Optional CSV logging
      if (log_enabled_.load() && log_file_) {
        maybe_rotate(log_file_, log_path_, "ts,i,p,bp,ap,bv,av,v,to,oi,MA5,wall");
        log_file_ << t->ts << ',' << t->instrument << ',' << t->p << ','
                  << t->bp << ',' << t->ap << ',' << t->bv << ',' << t->av << ','
                  << t->v << ',' << t->to << ',' << t->oi << ',' << t->ma5 << ','
                  << wall << '\n';
      }

      std::string json = fmt::format(R"({{"t":{},"i":"{}","p":{},"bp":{},"ap":{},"bv":{},"av":{},"v":{},"to":{},"oi":{},"MA5":{},"w":{}}})",
        t->ts, t->instrument, t->p, t->bp, t->ap, t->bv, t->av, t->v, t->to, t->oi, t->ma5, wall);
      // schedule_broadcast(json);
      schedule_broadcast_inst(std::string(t->instrument), json);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
} // VeloTick marker

void TcpServer::run_aggregator_1m() {
  auto cursor = g_clean_buffer.make_cursor();
  while (running_.load()) {
    const Tick* t = cursor.next_ptr();
    if (!t) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }
    std::string inst(t->instrument);
    uint64_t minute_ms = (static_cast<uint64_t>(t->ts) / 60000ULL) * 60000ULL;
    std::lock_guard<std::mutex> lk(mtx_);
    auto& cur = kline_building_[inst];
    if (cur.n == 0 || cur.t != minute_ms) {
      // finalize previous
      if (cur.n > 0) {
        auto& vec = klines_1m_[inst];
        vec.push_back(cur);
        if (vec.size() > kline_1m_max_) {
          vec.erase(vec.begin(), vec.begin() + (vec.size() - kline_1m_max_));
        }
        if (log_enabled_.load() && kline_log_file_) {
          maybe_rotate(kline_log_file_, kline_log_path_, "t_start_ms,i,o,h,l,c,v,n");
          kline_log_file_ << cur.t << ',' << inst << ',' << cur.o << ',' << cur.h << ','
                          << cur.l << ',' << cur.c << ',' << cur.v << ',' << cur.n << '\n';
        }
      }
      // start new minute
      cur.t = minute_ms;
      cur.o = cur.h = cur.l = cur.c = t->p;
      cur.v = t->v;
      cur.n = 1;
    } else {
      // update current minute
      cur.h = std::max(cur.h, t->p);
      cur.l = std::min(cur.l, t->p);
      cur.c = t->p;
      cur.v += t->v;
      cur.n += 1;
    }
  }
}

void TcpServer::run_aggregator_5s() {
  auto cursor = g_clean_buffer.make_cursor();
  while (running_.load()) {
    const Tick* t = cursor.next_ptr();
    if (!t) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }
    std::string inst(t->instrument);
    uint64_t sec5_ms = (static_cast<uint64_t>(t->ts) / 5000ULL) * 5000ULL;
    std::lock_guard<std::mutex> lk(mtx_);
    auto& cur = kline_building_5s_[inst];
    if (cur.n == 0 || cur.t != sec5_ms) {
      // finalize previous
      if (cur.n > 0) {
        auto& vec = klines_5s_[inst];
        vec.push_back(cur);
        if (vec.size() > kline_5s_max_) {
          vec.erase(vec.begin(), vec.begin() + (vec.size() - kline_5s_max_));
        }
        if (log_enabled_.load() && kline5s_log_file_) {
          maybe_rotate(kline5s_log_file_, kline5s_log_path_, "t_start_ms,i,o,h,l,c,v,n");
          kline5s_log_file_ << cur.t << ',' << inst << ',' << cur.o << ',' << cur.h << ','
                             << cur.l << ',' << cur.c << ',' << cur.v << ',' << cur.n << '\n';
        }
      }
      // start new bucket
      cur.t = sec5_ms;
      cur.o = cur.h = cur.l = cur.c = t->p;
      cur.v = t->v;
      cur.n = 1;
    } else {
      // update current bucket
      cur.h = std::max(cur.h, t->p);
      cur.l = std::min(cur.l, t->p);
      cur.c = t->p;
      cur.v += t->v;
      cur.n += 1;
    }
  }
}

void TcpServer::maybe_rotate(std::ofstream& f, const std::string& path, const char* header) {
  if (rotate_bytes_ == 0) return;
  try {
    auto sz = std::filesystem::exists(path) ? std::filesystem::file_size(path) : 0ull;
    if (sz >= rotate_bytes_) {
      // close current file, rename, reopen and write header
      if (f.is_open()) f.close();
      auto now = std::chrono::system_clock::now();
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
      std::string rotated = path + "." + std::to_string(ms);
      std::filesystem::rename(path, rotated);
      f.open(path, std::ios::app);
      if (f && header && *header) {
        f << header << '\n';
      }
      prune_rotated(path);
    }
  } catch (...) {
    // ignore rotation errors
  }
}

void TcpServer::prune_rotated(const std::string& path) {
  if (rotate_keep_ <= 0) return;
  try {
    std::filesystem::path p(path);
    auto base = p.filename().string();
    std::vector<std::filesystem::directory_entry> files;
    for (auto& entry : std::filesystem::directory_iterator(p.parent_path())) {
      if (!entry.is_regular_file()) continue;
      auto name = entry.path().filename().string();
      if (name.rfind(base + ".", 0) == 0) {
        files.push_back(entry);
      }
    }
    if (files.size() > static_cast<size_t>(rotate_keep_)) {
      std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
      });
      size_t to_del = files.size() - rotate_keep_;
      for (size_t i = 0; i < to_del; ++i) {
        try { std::filesystem::remove(files[i]); } catch (...) {}
      }
    }
  } catch (...) {
    // ignore prune errors
  }
}