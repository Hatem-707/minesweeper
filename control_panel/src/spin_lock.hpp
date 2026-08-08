#pragma once

#include <atomic>
template<typename T>
class SpinLockGuard
{
  template<typename U>
  friend class SpinLock;

public:
  T& data;
  ~SpinLockGuard() { flag.exchange(false, std::memory_order::release); }

private:
  std::atomic<bool>& flag;

  SpinLockGuard(T& data, std::atomic<bool>& flag)
    : data(data)
    , flag(flag)
  {
    while (flag.exchange(true, std::memory_order::acquire)) {
    }
  }
};

template<typename T>
class SpinLock
{
  alignas(64) std::atomic<bool> flag;
  T data;

public:
  SpinLock(T data)
    : data(std::move(data))
  {
  }
  SpinLockGuard<T> unlock() { return SpinLockGuard<T>(data, flag); }
};
