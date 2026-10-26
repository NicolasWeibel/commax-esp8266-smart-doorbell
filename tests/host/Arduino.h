#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <type_traits>

using std::size_t;

class __FlashStringHelper {};

// Only the Arduino String operations exercised by the firmware's formatter.
class String {
 public:
  String() = default;
  String(const char* value) : value_(value != nullptr ? value : "") {}
  String(const __FlashStringHelper* value)
      : String(reinterpret_cast<const char*>(value)) {}

  template <typename T,
            typename = std::enable_if_t<std::is_integral<T>::value>>
  String(T value) : value_(std::to_string(value)) {}

  void reserve(size_t capacity) { value_.reserve(capacity); }
  size_t length() const { return value_.length(); }
  const char* c_str() const { return value_.c_str(); }

  String& operator+=(const String& other) {
    value_ += other.value_;
    return *this;
  }
  String& operator+=(const char* other) {
    if (other != nullptr) {
      value_ += other;
    }
    return *this;
  }
  String& operator+=(const __FlashStringHelper* other) {
    return *this += reinterpret_cast<const char*>(other);
  }
  String& operator+=(char other) {
    value_ += other;
    return *this;
  }
  bool operator==(const char* other) const {
    return value_ == (other != nullptr ? other : "");
  }

 private:
  std::string value_;
};

#ifdef _WIN32
inline struct tm* localtime_r(const time_t* value, struct tm* result) {
  return localtime_s(result, value) == 0 ? result : nullptr;
}
#endif

#define F(value) reinterpret_cast<const __FlashStringHelper*>(value)
#define D5 14
#define LED_BUILTIN 2
#define INPUT 0
#define HIGH 1
#define LOW 0
#define CHANGE 2
#define IRAM_ATTR

namespace HostArduino {
inline uint32_t nowMs = 0;
inline uint32_t nowUs = 0;
inline int digitalLevel = LOW;
inline void (*interruptHandler)() = nullptr;

inline void setTime(uint32_t ms, uint32_t us) {
  nowMs = ms;
  nowUs = us;
}

inline void setLevel(int level) {
  digitalLevel = level;
}

inline void triggerInterrupt() {
  if (interruptHandler != nullptr) {
    interruptHandler();
  }
}
}  // namespace HostArduino

inline uint32_t millis() { return HostArduino::nowMs; }
inline uint32_t micros() { return HostArduino::nowUs; }
inline void yield() {}
inline void noInterrupts() {}
inline void interrupts() {}
inline void pinMode(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return HostArduino::digitalLevel; }
inline int digitalPinToInterrupt(uint8_t pin) { return pin; }
inline void attachInterrupt(int, void (*handler)(), int) {
  HostArduino::interruptHandler = handler;
}
