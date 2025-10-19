#pragma once
#include <string>
#include <memory>
#include "tick.h"
#include "backend_config.h"

class IMsgBus {
public:
  virtual ~IMsgBus() = default;
  virtual void publish_raw(const Tick& t) = 0;
  virtual void publish_clean(const Tick& t) = 0;
};

class NullBus : public IMsgBus {
public:
  explicit NullBus(const BackendConfig&) {}
  void publish_raw(const Tick&) override {}
  void publish_clean(const Tick&) override {}
};

// Stub for future Kafka integration (compile guarded)
class KafkaBus : public IMsgBus {
public:
  explicit KafkaBus(const BackendConfig& cfg) : cfg_(cfg) {}
  void publish_raw(const Tick& t) override { (void)t; /* TODO: librdkafka produce */ }
  void publish_clean(const Tick& t) override { (void)t; /* TODO: librdkafka produce */ }
private:
  BackendConfig cfg_;
};

inline std::unique_ptr<IMsgBus> make_bus(const BackendConfig& cfg) {
#if defined(USE_KAFKA)
  if (cfg.kafka.enable) {
    return std::make_unique<KafkaBus>(cfg);
  }
#endif
  return std::make_unique<NullBus>(cfg);
}