#include "../Intel_RTL_analyzer/spectrumhistory.h"
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
    std::vector<double> peaks;
    accumulatePeakHold(peaks, std::vector<double>{-80, -70});
    // A transient between GUI refreshes must survive later quiet FFTs.
    accumulatePeakHold(peaks, std::vector<double>{-10, -90});
    accumulatePeakHold(peaks, std::vector<double>{-85, -75});
    assert((peaks == std::vector<double>{-10, -70}));
    peaks.clear();
    accumulatePeakHold(peaks, std::vector<double>{-100, -95});
    assert((peaks == std::vector<double>{-100, -95}));
    accumulatePeakHold(peaks, std::vector<double>{-20});
    assert((peaks == std::vector<double>{-20}));
    accumulatePeakHold(peaks, std::vector<double>{std::numeric_limits<double>::quiet_NaN()});
    assert(peaks[0] == -20);

    WaterfallHistory history(300);
    assert(!history.append(0, {1}));
    assert(!history.append(1, {}));
    for (std::uint64_t i = 1; i <= 650; ++i) {
        assert(history.append(i, {static_cast<double>(i), -50}));
        assert(!history.append(i, {999, 999}));
        assert(history.row(0)[0] == static_cast<double>(i));
        assert(history.size() == (i < 300 ? i : 300));
    }
    assert(history.row(299)[0] == 351);
    for (std::size_t i = 0; i < history.size(); ++i)
        assert(history.row(i)[0] == 650.0 - i);
    assert(!history.append(649, {1, 2}));
    history.clear(); // Hardware/simulator change.
    assert(history.size() == 0);
    assert(history.append(651, {-90, -80}));
    assert(history.size() == 1 && history.row(0)[0] == -90);
    assert(history.append(652, {5})); // FFT size change clears incompatible rows.
    assert(history.size() == 1);
    std::cout << "PASS: peak transient retention/reset and 650 chronological waterfall rows, duplicate suppression, source reset\n";
}
