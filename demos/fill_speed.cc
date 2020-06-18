#include <algorithm>
#include <chrono>
#include <iostream>
#include <functional>
#include <vector>

#include "compiler_specifics.h"

using Clock = std::chrono::high_resolution_clock;

void CompilerOpaqueUse(void* p) {
  asm volatile ("" :: "r"(p) : "memory");
}

Clock::duration Runtime(std::function<void()> f) {
  auto start = Clock::now();
  f();
  return Clock::now() - start;
}

int64_t Microseconds(const Clock::duration &d) {
  return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}

SAFESIDE_NEVER_INLINE
Clock::duration TestOne(int buffer[], int size, int v) {
  int warmup = 5;  // avoid transient effects, improve consistency
  int rounds = 50;  // amplify result

  Clock::duration d;

  for (int i = 0; i < warmup + 1; ++i) {
    d = Runtime([&]() {
      for (int r = 0; r < rounds; ++r) {
        std::fill(buffer, buffer + size, v);
        CompilerOpaqueUse(buffer);
      }
    });
  }

  return d;
}

void Test() {
  // target ~50% of L3 size
  // see `sudo lshw` or /sys/devices/system/cpu/cpu0/cache/index3/size
  int buffer_bytes = 2 * 1024 * 1024;

  std::vector<int> buffer(buffer_bytes / sizeof(int));

  std::vector<Clock::duration> d0s, d1s, d2s;

  int samples = 30;  // reduce variation

  for (int i = 0; i < samples; ++i) {
    d0s.push_back(TestOne(buffer.data(), buffer.size(), 0));

    // Use two non-zero values so we can also show the amount of variation/
    // jitter between runs that we *always* expect to act the same.
    d1s.push_back(TestOne(buffer.data(), buffer.size(), 1));
    d2s.push_back(TestOne(buffer.data(), buffer.size(), 2));
  }

  std::sort(d0s.begin(), d0s.end());
  std::sort(d1s.begin(), d1s.end());
  std::sort(d2s.begin(), d2s.end());

  std::cout << "Fill with 0: " << Microseconds(d0s[samples/2]) << "us"
            << std::endl;
  std::cout << "Fill with 1: " << Microseconds(d1s[samples/2]) << "us"
            << std::endl;
  std::cout << "Fill with 2: " << Microseconds(d2s[samples/2]) << "us"
            << std::endl;
}

int main(int argc, char* argv[]) {
  Test();

  return 0;
}
