#pragma once
#include <string>

struct KafkaConfig {
  bool enable{false};
  std::string brokers{"localhost:9092"};
  std::string topic_prefix{"ctp"}; // e.g., ctp.md.{instrument}, clean.md.{instrument}
};

struct RedisConfig {
  bool enable{false};
  std::string host{"127.0.0.1"};
  int port{6379};
  int db{0};
  std::string key_prefix{"velotick:"};
};

struct InfluxConfig {
  bool enable{false};
  std::string url{"http://127.0.0.1:8086"};
  std::string org{"org"};
  std::string bucket{"velotick"};
  std::string token{""};
};

struct SqliteConfig {
  bool enable{false};
  std::string path{"data/velotick.db"};
};

struct CleanConfig {
  // mode: python | local | grpc
  std::string mode{"local"};
  std::string grpc_addr{"127.0.0.1:50051"};
};

struct BackendConfig {
  KafkaConfig kafka;
  RedisConfig redis;
  InfluxConfig influx;
  SqliteConfig sqlite;
  CleanConfig clean;
};