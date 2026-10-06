#pragma once

#include <stdint.h>

namespace omnirc {

/// Turns the rolling 8-bit sequence numbers carried by every frame into a link
/// quality figure, on whichever side is receiving.
///
/// The window decays instead of accumulating forever, so the reported loss
/// describes the link right now rather than averaging away a problem that
/// started ten minutes into a flight.
class SequenceTracker {
public:
    /// Gaps larger than this are treated as a resync rather than as loss. A
    /// receiver that was power cycled, or a link that dropped for a second,
    /// would otherwise report a burst of "lost" frames that never existed.
    static constexpr uint8_t kResyncGap = 32;

    /// Frames counted before the window is halved.
    static constexpr uint16_t kWindowSize = 128;

    /// Forgets everything, including sync. Call this when a transport starts
    /// or when switching profiles.
    void reset();

    /// Feeds the sequence number of a frame that just arrived.
    /// Returns how many frames appear to have been lost immediately before it;
    /// duplicates, reordered frames and resyncs return 0.
    uint8_t onFrame(uint8_t seq);

    /// Recent loss as a percentage, rounded to nearest. Returns 0 before any
    /// frame has been seen.
    uint8_t lossPercent() const;

    /// True once at least one frame has been seen, so callers can tell
    /// "perfect link" apart from "no link yet".
    bool synced() const { return m_synced; }

    uint16_t windowReceived() const { return m_windowReceived; }
    uint16_t windowLost() const { return m_windowLost; }
    uint32_t totalReceived() const { return m_totalReceived; }
    uint32_t totalLost() const { return m_totalLost; }

private:
    void decayWindow();

    uint16_t m_windowReceived = 0;
    uint16_t m_windowLost = 0;
    uint32_t m_totalReceived = 0;
    uint32_t m_totalLost = 0;
    uint8_t m_lastSeq = 0;
    bool m_synced = false;
};

}  // namespace omnirc
