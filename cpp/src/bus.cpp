#include "bus.h"
#include <fmt/core.h>

// Currently NullBus does nothing; KafkaBus stub prints when enabled macros are set
// The real Kafka integration should use librdkafka and serialize Tick to protobuf/json.

// No extra implementation needed: methods are defined inline or as TODO stubs.