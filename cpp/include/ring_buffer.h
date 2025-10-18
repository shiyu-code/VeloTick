#pragma once
// VeloTick marker
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <array>

// 单生产者，多消费者的无锁环形队列（覆盖旧数据，容量为 2 的幂）
template <typename T, size_t Capacity>
class RingBuffer {
  static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
public:
  RingBuffer() : head_(0) {}

  // 生产者推入（覆盖旧数据），无锁
  bool push(const T& v) noexcept {
    buffer_[head_ & (Capacity - 1)] = v;
    head_.fetch_add(1, std::memory_order_release);
    return true;
  }

  struct Cursor {
    RingBuffer* rb;
    uint64_t index;
    Cursor(RingBuffer* r) : rb(r), index(0) {}
    const T* next_ptr() noexcept {
      uint64_t h = rb->head_.load(std::memory_order_acquire);
      if (index >= h) return nullptr;
      const T* ptr = &rb->buffer_[index & (Capacity - 1)];
      index++;
      return ptr;
    }
  };

  Cursor make_cursor() { return Cursor(this); }

  uint64_t size() const noexcept { return head_.load(std::memory_order_acquire); }

private:
  std::array<T, Capacity> buffer_;
  std::atomic<uint64_t> head_;
};
// VeloTick marker