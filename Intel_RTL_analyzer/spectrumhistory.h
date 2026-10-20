#ifndef SPECTRUMHISTORY_H
#define SPECTRUMHISTORY_H

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

// Apply to EVERY completed FFT, not just GUI refreshes.
template <typename Container>
void accumulatePeakHold(Container& peaks, const Container& frame)
{
    if (peaks.size() != frame.size()) {
        peaks = frame;
        return;
    }
    // Detach Qt's implicitly shared buffer once, not via operator[] per bin.
    auto* peakData = peaks.data();
    const auto* frameData = frame.data();
    for (decltype(frame.size()) i = 0; i < frame.size(); ++i) {
        if (std::isfinite(frameData[i]) &&
            (!std::isfinite(peakData[i]) || frameData[i] > peakData[i]))
            peakData[i] = frameData[i];
    }
}

// Newest row first. Sequence numbers prevent duplicate timer samples.
class WaterfallHistory
{
public:
    explicit WaterfallHistory(std::size_t capacity) : m_capacity(capacity) {}
    void clear()
    {
        m_rows.clear();
        m_lastSequence = 0;
    }
    bool append(std::uint64_t sequence, const std::vector<double>& row)
    {
        if (row.empty() || m_capacity == 0 || sequence == 0 || sequence <= m_lastSequence)
            return false;
        if (!m_rows.empty() && m_rows.front().size() != row.size()) m_rows.clear();
        m_rows.push_front(row);
        if (m_rows.size() > m_capacity) m_rows.pop_back();
        m_lastSequence = sequence;
        return true;
    }
    std::size_t size() const { return m_rows.size(); }
    const std::vector<double>& row(std::size_t age) const { return m_rows.at(age); }
private:
    std::size_t m_capacity;
    std::uint64_t m_lastSequence = 0;
    std::deque<std::vector<double>> m_rows;
};
#endif

