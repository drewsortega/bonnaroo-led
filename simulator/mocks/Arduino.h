#ifndef ARDUINO_H
#define ARDUINO_H

/**
 * Arduino.h Mock for LED Grid Simulator
 * Provides Arduino-compatible API for desktop compilation.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <thread>
#include <string>
#include <cstdarg>

// Arduino types
typedef uint8_t byte;
typedef bool boolean;

// Pin modes (not used in simulator)
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

#define HIGH 1
#define LOW 0

// Disable LED feedback for IR (not needed in simulator)
#define DISABLE_LED_FEEDBACK 0

// Time functions using std::chrono
static auto _start_time = std::chrono::steady_clock::now();

inline unsigned long millis() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - _start_time).count();
}

inline unsigned long micros() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - _start_time).count();
}

inline void delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void delayMicroseconds(unsigned int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

// Math functions
#ifndef min
template<typename T>
inline T min(T a, T b) { return (a < b) ? a : b; }
#endif

#ifndef max
template<typename T>
inline T max(T a, T b) { return (a > b) ? a : b; }
#endif

template<typename T>
inline T constrain(T x, T a, T b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

inline long random(long max) {
    return rand() % max;
}

inline long random(long min, long max) {
    return min + (rand() % (max - min));
}

inline void randomSeed(unsigned long seed) {
    srand(seed);
}

// String class (simplified Arduino String)
class String {
public:
    std::string _str;
    
    String() : _str() {}
    String(const char* s) : _str(s ? s : "") {}
    String(const std::string& s) : _str(s) {}
    String(int value) : _str(std::to_string(value)) {}
    String(unsigned int value) : _str(std::to_string(value)) {}
    String(long value) : _str(std::to_string(value)) {}
    String(unsigned long value) : _str(std::to_string(value)) {}
    String(float value) : _str(std::to_string(value)) {}
    String(double value) : _str(std::to_string(value)) {}
    
    const char* c_str() const { return _str.c_str(); }
    unsigned int length() const { return _str.length(); }
    
    void toUpperCase() {
        for (auto& c : _str) c = toupper(c);
    }
    
    void toLowerCase() {
        for (auto& c : _str) c = tolower(c);
    }
    
    bool endsWith(const char* suffix) const {
        std::string s(suffix);
        if (s.length() > _str.length()) return false;
        return _str.compare(_str.length() - s.length(), s.length(), s) == 0;
    }
    
    int lastIndexOf(const char* s) const {
        size_t pos = _str.rfind(s);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }
    
    void remove(unsigned int index, unsigned int count = 1) {
        if (index < _str.length()) {
            _str.erase(index, count);
        }
    }
    
    char operator[](unsigned int index) const {
        return _str[index];
    }
    
    String& operator+=(const String& rhs) {
        _str += rhs._str;
        return *this;
    }
    
    String& operator+=(const char* rhs) {
        if (rhs) _str += rhs;
        return *this;
    }
    
    bool operator==(const String& rhs) const {
        return _str == rhs._str;
    }
};

// Serial mock (prints to stdout)
class SerialClass {
public:
    void begin(unsigned long baud) {
        // Nothing to do for simulator
        (void)baud;
    }
    
    void print(const char* s) { printf("%s", s); }
    void print(const String& s) { printf("%s", s.c_str()); }
    void print(int n) { printf("%d", n); }
    void print(unsigned int n) { printf("%u", n); }
    void print(long n) { printf("%ld", n); }
    void print(unsigned long n) { printf("%lu", n); }
    void print(double n) { printf("%f", n); }
    
    void println() { printf("\n"); }
    void println(const char* s) { printf("%s\n", s); }
    void println(const String& s) { printf("%s\n", s.c_str()); }
    void println(int n) { printf("%d\n", n); }
    void println(unsigned int n) { printf("%u\n", n); }
    void println(long n) { printf("%ld\n", n); }
    void println(unsigned long n) { printf("%lu\n", n); }
    void println(double n) { printf("%f\n", n); }
    
    int available() { return 0; }
    int read() { return -1; }

    void printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }

    void flush() {
        fflush(stdout);
    }
};

extern SerialClass Serial;

// Pin functions (no-ops for simulator)
inline void pinMode(uint8_t pin, uint8_t mode) {
    (void)pin; (void)mode;
}

inline void digitalWrite(uint8_t pin, uint8_t val) {
    (void)pin; (void)val;
}

inline int digitalRead(uint8_t pin) {
    (void)pin;
    return LOW;
}

inline int analogRead(uint8_t pin) {
    (void)pin;
    return 0;
}

inline void analogWrite(uint8_t pin, int val) {
    (void)pin; (void)val;
}

#endif // ARDUINO_H
