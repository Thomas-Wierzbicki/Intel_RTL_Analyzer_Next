#include "../Intel_RTL_analyzer/measurementmath.h"
#include "../Intel_RTL_analyzer/spectrumhistory.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
int main() {
    assert(std::abs(normalizedToneDb(2048.0*2048, 2048)) < 1e-9);
    assert(std::abs(normalizedToneDb(1024.0*1024, 2048) + 6.020599913) < 1e-6);
    assert(normalizedToneDb(0, 2048) == -200);
    assert(normalizedToneDb(std::numeric_limits<double>::quiet_NaN(), 2048) == -200);
    std::vector<double> accumulated;
    accumulatePeakHold(accumulated, std::vector<double>{-100, -5, -100});
    accumulatePeakHold(accumulated, std::vector<double>{-100, -90, -100});
    assert(accumulated[1] == -5); // Burst between display ticks is retained.
    const float bins[] = {-100, -100, -4, -100, -100, -100, -100, -100};
    auto pick = pixelPeak(bins, 8, 0, 4);
    assert(pick.bin == 2 && pick.db == -4);
    pick = pixelPeak(bins, 8, 2.1, 2.2);
    assert(pick.bin == 2 && pick.db == -4); // Zoomed cell.
    pick = pixelPeak(bins, 8, 4, 8);
    assert(pick.db == -100);
}
