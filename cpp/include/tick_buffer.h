#pragma once
#include <atomic>
#include <cstdint>
#include "tick.h"

class TickBuffer {
public:
  static constexpr size_t Capacity = (1u << 20);
  TickBuffer() : head_(0) {}

  bool push(const Tick& v) noexcept {
    buffer_[head_ & (Capacity - 1)] = v;
    head_.fetch_add(1, std::memory_order_release);
    return true;
  }

  class Cursor {
  public:
    explicit Cursor(TickBuffer* r) : rb_(r), index_(0) {}
    const Tick* next_ptr() noexcept {
      uint64_t h = rb_->head_.load(std::memory_order_acquire);
      if (index_ >= h) return nullptr;
      const Tick* ptr = &rb_->buffer_[index_ & (Capacity - 1)];
      index_++;
      return ptr;
    }
  private:
    TickBuffer* rb_;
    uint64_t index_;
  };

  Cursor make_cursor() { return Cursor(this); }
  uint64_t size() const noexcept { return head_.load(std::memory_order_acquire); }

private:
  Tick buffer_[Capacity];
  std::atomic<uint64_t> head_;
};