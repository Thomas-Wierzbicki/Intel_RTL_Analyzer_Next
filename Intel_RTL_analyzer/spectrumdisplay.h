#ifndef SPECTRUMDISPLAY_H
#define SPECTRUMDISPLAY_H
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// Select the strongest bin in each display group, preserving its actual
// frequency. Increasing groups keeps indices strictly increasing.
inline std::vector<std::size_t> spectrumDisplayBins(const double* values,
                                                    std::size_t count,
                                                    std::size_t budget)
{
    const std::size_t groups = (std::min)(count, budget);
    std::vector<std::size_t> indices;
    indices.reserve(groups);
    for (std::size_t group = 0; group < groups; ++group) {
        const std::size_t begin = group * count / groups;
        const std::size_t end = (group + 1) * count / groups;
        std::size_t peak = begin;
        for (std::size_t i = begin + 1; i < end; ++i)
            if (std::isfinite(values[i]) &&
                (!std::isfinite(values[peak]) || values[i] > values[peak])) peak = i;
        indices.push_back(peak);
    }
    return indices;
}
// Reduce only visible bins so zooming reveals the original FFT resolution.
inline std::vector<std::size_t> spectrumVisibleBins(const double* frequencies,
    const double* values, std::size_t count, std::size_t budget,
    double lowerHz, double upperHz)
{
    if (count == 0 || budget == 0 || upperHz < lowerHz) return {};
    const auto* first = std::lower_bound(frequencies, frequencies + count, lowerHz);
    const auto* last = std::upper_bound(first, frequencies + count, upperHz);
    const std::size_t offset = static_cast<std::size_t>(first - frequencies);
    auto bins = spectrumDisplayBins(values + offset, static_cast<std::size_t>(last - first), budget);
    for (auto& bin : bins) bin += offset;
    return bins;
}
#endif

