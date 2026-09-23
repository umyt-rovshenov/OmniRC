#pragma once

#include <stdint.h>

namespace ctl {
namespace time {

/// Milliseconds since boot, as a 64-bit value.
///
/// Arduino's millis() wraps every 49 days. A transmitter that is left on, or a
/// timeout computed across the wrap, would then misbehave in a way that is
/// almost impossible to reproduce, so every timing decision in this firmware
/// goes through these functions instead.
uint64_t millis();

/// Microseconds since boot, as a 64-bit value.
uint64_t micros();

/// Blocks for the given number of milliseconds. Only for startup code; tasks
/// should use their scheduler's delay so they yield properly.
void delayMs(uint32_t milliseconds);

/// Fires once per period without drifting.
///
/// The next deadline is advanced by exactly one period rather than being
/// restarted from "now", so a control loop that occasionally runs late keeps
/// its average rate instead of slowly falling behind.
class Interval {
public:
    Interval() = default;
    explicit Interval(uint32_t periodMs) : m_periodMs(periodMs) {}

    void setPeriod(uint32_t periodMs) { m_periodMs = periodMs; }
    uint32_t periodMs() const { return m_periodMs; }

    /// Restarts the period from `now`.
    void reset(uint64_t now) {
        m_next = now + m_periodMs;
        m_started = true;
    }

    /// True at most once per period. Pass the current time explicitly so the
    /// caller can be unit tested without a clock.
    bool expired(uint64_t now) {
        if (m_periodMs == 0) {
            return true;
        }
        if (!m_started) {
            reset(now);
            return false;
        }
        if (now < m_next) {
            return false;
        }
        m_next += m_periodMs;
        // After a long stall, skip the backlog instead of firing repeatedly to
        // "catch up", which would flood whatever the interval drives.
        if (m_next <= now) {
            m_next = now + m_periodMs;
        }
        return true;
    }

    bool expired() { return expired(millis()); }

private:
    uint64_t m_next = 0;
    uint32_t m_periodMs = 0;
    bool m_started = false;
};

/// One-shot timeout, used for things like the failsafe watchdog.
class Deadline {
public:
    void arm(uint32_t timeoutMs, uint64_t now) {
        m_expiresAt = now + timeoutMs;
        m_armed = true;
    }

    void arm(uint32_t timeoutMs) { arm(timeoutMs, millis()); }

    void disarm() { m_armed = false; }

    bool armed() const { return m_armed; }

    /// A disarmed deadline never expires.
    bool expired(uint64_t now) const { return m_armed && now >= m_expiresAt; }
    bool expired() const { return expired(millis()); }

    /// Milliseconds left, or 0 once expired or disarmed.
    uint32_t remainingMs(uint64_t now) const {
        if (!m_armed || now >= m_expiresAt) {
            return 0;
        }
        return static_cast<uint32_t>(m_expiresAt - now);
    }

private:
    uint64_t m_expiresAt = 0;
    bool m_armed = false;
};

}  // namespace time
}  // namespace ctl
