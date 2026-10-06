#include "omnirc/SequenceTracker.h"

namespace omnirc {

void SequenceTracker::reset() {
    m_windowReceived = 0;
    m_windowLost = 0;
    m_totalReceived = 0;
    m_totalLost = 0;
    m_lastSeq = 0;
    m_synced = false;
}

uint8_t SequenceTracker::onFrame(uint8_t seq) {
    uint8_t lost = 0;

    if (m_synced) {
        // Unsigned arithmetic wraps, so this stays correct across 255 -> 0.
        const uint8_t gap = static_cast<uint8_t>(seq - m_lastSeq);
        if (gap == 0) {
            // Duplicate or retransmission: neither progress nor loss.
            return 0;
        }
        if (gap <= kResyncGap) {
            lost = static_cast<uint8_t>(gap - 1);
        }
        // A larger gap means the link was down or the peer restarted. Counting
        // it as loss would make the statistics meaningless, so it is ignored.
    } else {
        m_synced = true;
    }

    m_lastSeq = seq;
    m_windowReceived = static_cast<uint16_t>(m_windowReceived + 1);
    m_windowLost = static_cast<uint16_t>(m_windowLost + lost);
    ++m_totalReceived;
    m_totalLost += lost;
    decayWindow();

    return lost;
}

uint8_t SequenceTracker::lossPercent() const {
    const uint32_t total = static_cast<uint32_t>(m_windowReceived) + m_windowLost;
    if (total == 0) {
        return 0;
    }
    return static_cast<uint8_t>((static_cast<uint32_t>(m_windowLost) * 100u + total / 2u) / total);
}

void SequenceTracker::decayWindow() {
    if (static_cast<uint32_t>(m_windowReceived) + m_windowLost < kWindowSize) {
        return;
    }
    // Halving both counters keeps the ratio while letting old frames fade out.
    m_windowReceived = static_cast<uint16_t>(m_windowReceived / 2u);
    m_windowLost = static_cast<uint16_t>(m_windowLost / 2u);
}

}  // namespace omnirc
