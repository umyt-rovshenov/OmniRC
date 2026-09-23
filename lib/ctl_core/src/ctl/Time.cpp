#include "ctl/Time.h"

#if defined(CTL_PLATFORM_NATIVE)
#include <chrono>
#include <thread>
#elif defined(ESP_PLATFORM)
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <Arduino.h>
#endif

namespace ctl {
namespace time {

#if defined(CTL_PLATFORM_NATIVE)

namespace {
std::chrono::steady_clock::time_point bootTime() {
    static const std::chrono::steady_clock::time_point kBoot = std::chrono::steady_clock::now();
    return kBoot;
}
}  // namespace

uint64_t millis() {
    const auto elapsed = std::chrono::steady_clock::now() - bootTime();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

uint64_t micros() {
    const auto elapsed = std::chrono::steady_clock::now() - bootTime();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
}

void delayMs(uint32_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

#elif defined(ESP_PLATFORM)

// esp_timer_get_time() is already a 64-bit microsecond counter, so there is
// nothing to extend here.
uint64_t micros() {
    return static_cast<uint64_t>(esp_timer_get_time());
}

uint64_t millis() {
    return micros() / 1000ULL;
}

void delayMs(uint32_t milliseconds) {
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

#else

namespace {
/// Extends Arduino's 32-bit counters by counting the wraps. Safe as long as it
/// is called more often than once per wrap period, which holds for millis()
/// (49 days) and for micros() (71 minutes) in any running firmware.
template<typename Reader>
uint64_t extend(Reader read, uint32_t& lastRaw, uint64_t& highBits) {
    const uint32_t raw = read();
    if (raw < lastRaw) {
        highBits += 0x1'0000'0000ULL;
    }
    lastRaw = raw;
    return highBits + raw;
}

uint32_t g_lastMillis = 0;
uint64_t g_millisHigh = 0;
uint32_t g_lastMicros = 0;
uint64_t g_microsHigh = 0;
}  // namespace

uint64_t millis() {
    return extend([] { return ::millis(); }, g_lastMillis, g_millisHigh);
}

uint64_t micros() {
    return extend([] { return ::micros(); }, g_lastMicros, g_microsHigh);
}

void delayMs(uint32_t milliseconds) {
    ::delay(milliseconds);
}

#endif

}  // namespace time
}  // namespace ctl
