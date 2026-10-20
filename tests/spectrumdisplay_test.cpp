#include "../Intel_RTL_analyzer/spectrumdisplay.h"
#include <cassert>
#include <iostream>
int main()
{
    std::vector<double> values(4096, -90);
    // Check every possible single-bin transient, including the final bin.
    for (std::size_t peak = 0; peak < values.size(); ++peak) {
        values[peak] = 40;
        const auto bins = spectrumDisplayBins(values.data(), values.size(), 800);
        assert(bins.size() == 800);
        assert(std::find(bins.begin(), bins.end(), peak) != bins.end());
        for (std::size_t i = 1; i < bins.size(); ++i) assert(bins[i] > bins[i - 1]);
        values[peak] = -90;
    }
    assert(spectrumDisplayBins(values.data(), 4096, 0).empty());
    assert(spectrumDisplayBins(nullptr, 0, 800).empty());
    const auto full = spectrumDisplayBins(values.data(), 3, 800);
    assert((full == std::vector<std::size_t>{0, 1, 2}));
    const double uneven[] = {-5, -1, -3, -2, -4};
    assert((spectrumDisplayBins(uneven, 5, 2) == std::vector<std::size_t>{1, 3}));
    std::cout << "PASS: all 4096 single-bin peaks retained, ordered frequencies, point cap and edge cases\n";
}
