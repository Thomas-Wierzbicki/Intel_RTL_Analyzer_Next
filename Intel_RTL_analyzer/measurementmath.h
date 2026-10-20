#ifndef MEASUREMENTMATH_H
#define MEASUREMENTMATH_H
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
// Complex full-scale bin-centred tone (magnitude 1) = 0 dBFS.
// Coherent Hann gain is removed; this is tone power/bin, NOT dBFS/Hz.
inline double normalizedToneDb(double fftPower, double windowSum)
{
    if (!(windowSum > 0) || !std::isfinite(fftPower) || fftPower <= 0) return -200;
    return (std::max)(-200.0, 10.0 * std::log10(fftPower / (windowSum * windowSum)));
}
// Floating-point multiplication avoids signed integer overflow after ~64 min at 2.4 MS/s.
inline std::int64_t simulationDeadlineNs(std::int64_t samples, double sampleRate)
{
    if (samples <= 0 || !(sampleRate > 0)) return 0;
    return static_cast<std::int64_t>(static_cast<long double>(samples) * 1000000000.0L / sampleRate);
}
struct DisplayPeak { std::size_t bin = 0; double db = -200; };
// Include every intersecting source cell. Narrow peaks survive reduction.
inline DisplayPeak pixelPeak(const float* values, std::size_t count,
                             double sourceBegin, double sourceEnd)
{
    DisplayPeak result;
    if (!count || !std::isfinite(sourceBegin) || !std::isfinite(sourceEnd) ||
        sourceEnd <= sourceBegin || sourceEnd <= 0 || sourceBegin >= double(count)) return result;
    const auto first = std::size_t((std::max)(0.0, std::floor(sourceBegin)));
    const auto last = (std::min)(count, std::size_t(std::ceil((std::min)(double(count), sourceEnd))));
    result.bin = first;
    for (auto i = first; i < last; ++i)
        if (std::isfinite(values[i]) && values[i] > result.db)
            result = {i, values[i]};
    return result;
}
#endif
