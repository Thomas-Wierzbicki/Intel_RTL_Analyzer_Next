#ifndef WATERFALLRASTER_H
#define WATERFALLRASTER_H
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

// Only the write position moves. Render [head, height) then [0, head).
class WaterfallRowRing
{
public:
    explicit WaterfallRowRing(int height) : m_height(height) {
        if (height <= 0) throw std::invalid_argument("Invalid waterfall height");
    }
    void clear() { m_head = 0; }
    int advance() { m_head = (m_head + m_height - 1) % m_height; return m_head; }
    int head() const { return m_head; }
    int firstSpan() const { return m_height - m_head; }
private:
    int m_height;
    int m_head = 0;
};

// Every FFT bin contributes once. Max pooling preserves narrow spectral peaks
// when several frequency bins share one display column.
template <typename Container>
std::vector<double> waterfallDisplayRow(const Container& frame, std::size_t maxColumns)
{
    const std::size_t size = static_cast<std::size_t>(frame.size());
    const std::size_t columns = (std::min)(size, maxColumns);
    std::vector<double> result(columns, -200.0);
    const auto* values = frame.data();
    for (std::size_t x = 0; x < columns; ++x) {
        const std::size_t begin = x * size / columns;
        const std::size_t end = (x + 1) * size / columns;
        for (std::size_t i = begin; i < end; ++i)
            if (std::isfinite(values[i]) && values[i] > result[x]) result[x] = values[i];
    }
    return result;
}
#endif
