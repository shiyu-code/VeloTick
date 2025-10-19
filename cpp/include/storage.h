#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <mutex>
#include <memory>
#include <filesystem>
#include "tick.h"
#include "candle.h" // Candle1m
#include "backend_config.h"

class RedisStore {
public:
  explicit RedisStore(const BackendConfig& cfg) : cfg_(cfg) {}
  void set_latest(const Tick& t) {
    if (!cfg_.redis.enable) return; // stub: in-memory only
    std::lock_guard<std::mutex> lk(mtx_);
    latest_[t.instrument] = t;
    // TODO: when USE_REDIS, push to Redis with key_prefix + instrument
  }
private:
  BackendConfig cfg_;
  std::mutex mtx_;
  std::unordered_map<std::string, Tick> latest_;
};

class InfluxWriter {
public:
  explicit InfluxWriter(const BackendConfig& cfg) : cfg_(cfg) {
    if (cfg_.influx.enable) {
      std::filesystem::create_directories("logs");
      log_.open("logs/influx_line.log", std::ios::app);
    }
  }
  void write_tick(const Tick& t) {
    if (!cfg_.influx.enable || !log_) return;
    // measurement: tick, tags: instrument, fields: p,bp,ap,bv,av,v,to,oi,ma5, ts in ns
    // line protocol
    auto ns = static_cast<long long>(t.ts) * 1000000LL;
    log_ << "tick,instrument=" << t.instrument
         << " p=" << t.p << ",bp=" << t.bp << ",ap=" << t.ap
         << ",bv=" << t.bv << ",av=" << t.av << ",v=" << t.v
         << ",to=" << t.to << ",oi=" << t.oi << ",ma5=" << t.ma5
         << " " << ns << "\n";
  }
  void write_kline(const Candle1m& c, const std::string& inst, const std::string& granularity) {
    if (!cfg_.influx.enable || !log_) return;
    auto ns = static_cast<long long>(c.t) * 1000000LL;
    log_ << "kline,instrument=" << inst << ",granularity=" << granularity
         << " o=" << c.o << ",h=" << c.h << ",l=" << c.l << ",c=" << c.c
         << ",v=" << c.v << ",n=" << c.n
         << " " << ns << "\n";
  }
private:
  BackendConfig cfg_;
  std::ofstream log_;
};

class SqliteRecorder {
public:
  explicit SqliteRecorder(const BackendConfig& cfg) : cfg_(cfg) {
    if (cfg_.sqlite.enable) {
      std::filesystem::create_directories("data");
      // Stub: write CSV as local replay file when SQLite not enabled
      csv1m_.open("data/sqlite_ohlc_1m.csv", std::ios::app);
      csv5s_.open("data/sqlite_ohlc_5s.csv", std::ios::app);
      if (csv1m_) csv1m_ << "t_start_ms,i,o,h,l,c,v,n\n";
      if (csv5s_) csv5s_ << "t_start_ms,i,o,h,l,c,v,n\n";
    }
  }
  void insert_kline_1m(const Candle1m& c, const std::string& inst) {
    if (!cfg_.sqlite.enable) return;
    if (csv1m_) {
      csv1m_ << c.t << ',' << inst << ',' << c.o << ',' << c.h << ','
             << c.l << ',' << c.c << ',' << c.v << ',' << c.n << '\n';
    }
    // TODO: when USE_SQLITE, insert into SQLite DB via sqlite3
  }
  void insert_kline_5s(const Candle1m& c, const std::string& inst) {
    if (!cfg_.sqlite.enable) return;
    if (csv5s_) {
      csv5s_ << c.t << ',' << inst << ',' << c.o << ',' << c.h << ','
             << c.l << ',' << c.c << ',' << c.v << ',' << c.n << '\n';
    }
    // TODO: sqlite insert
  }
private:
  BackendConfig cfg_;
  std::ofstream csv1m_;
  std::ofstream csv5s_;
};

struct StorageSuite {
  std::unique_ptr<RedisStore> redis;
  std::unique_ptr<InfluxWriter> influx;
  std::unique_ptr<SqliteRecorder> sqlite;
};

inline StorageSuite make_storage(const BackendConfig& cfg) {
  StorageSuite s{};
#if defined(USE_REDIS)
  s.redis = std::make_unique<RedisStore>(cfg);
#else
  s.redis = std::make_unique<RedisStore>(cfg);
#endif
#if defined(USE_INFLUX)
  s.influx = std::make_unique<InfluxWriter>(cfg);
#else
  s.influx = std::make_unique<InfluxWriter>(cfg);
#endif
#if defined(USE_SQLITE)
  s.sqlite = std::make_unique<SqliteRecorder>(cfg);
#else
  s.sqlite = std::make_unique<SqliteRecorder>(cfg);
#endif
  return s;
}