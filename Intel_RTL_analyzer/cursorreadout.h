#ifndef CURSORREADOUT_H
#define CURSORREADOUT_H
#include <cmath>
#include <cstddef>
#include <limits>
#include "frequencyzoom.h"

struct SpectrumPick {
    std::size_t index = 0;
    double distanceSquared = std::numeric_limits<double>::infinity();
    bool valid = false;
};
// Pick actual displayed points by screen distance, not an interpolated dB value.
inline SpectrumPick pickSpectrumPoint(const double* frequencies, const double* levels,
    std::size_t count, FrequencyView view, double minDb, double maxDb,
    double width, double height, double mouseX, double mouseY)
{
    SpectrumPick pick;
    if (view.upper <= view.lower || maxDb <= minDb || width <= 0 || height <= 0) return pick;
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(frequencies[i]) || !std::isfinite(levels[i]) ||
            frequencies[i] < view.lower || frequencies[i] > view.upper ||
            levels[i] < minDb || levels[i] > maxDb) continue;
        const double x = (frequencies[i] - view.lower) / (view.upper - view.lower) * width;
        const double y = (maxDb - levels[i]) / (maxDb - minDb) * height;
        const double distance = (x - mouseX) * (x - mouseX) + (y - mouseY) * (y - mouseY);
        if (distance < pick.distanceSquared) {
            pick.index = i;
            pick.distanceSquared = distance;
            pick.valid = true;
        }
    }
    return pick;
}
struct WaterfallPick {
    int bin = 0;
    int age = 0;
    int storageRow = 0;
    bool valid = false;
};
inline WaterfallPick pickWaterfallCell(int head, int columns, int rows,
    FrequencyView view, double fullLower, double fullUpper,
    double xFraction, double yFraction)
{
    WaterfallPick pick;
    if (columns <= 0 || rows <= 0 || fullUpper <= fullLower ||
        !std::isfinite(xFraction) || !std::isfinite(yFraction) ||
        xFraction < 0 || xFraction > 1 || yFraction < 0 || yFraction > 1) return pick;
    const double frequency = view.lower + xFraction * (view.upper - view.lower);
    const double binPosition = (frequency - fullLower) / (fullUpper - fullLower) * columns;
    const int bin = static_cast<int>(std::floor(binPosition));
    pick.bin = bin < 0 ? 0 : (bin >= columns ? columns - 1 : bin);
    const int age = static_cast<int>(yFraction * rows);
    pick.age = age >= rows ? rows - 1 : age;
    pick.storageRow = (head + pick.age) % rows;
    pick.valid = true;
    return pick;
}
#endif
