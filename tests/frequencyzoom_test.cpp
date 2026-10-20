#include "../Intel_RTL_analyzer/frequencyzoom.h"
#include "../Intel_RTL_analyzer/spectrumdisplay.h"
#include <cassert>
#include <iostream>
int main()
{
    const double lo = 432.3, hi = 434.7;
    FrequencyView view{lo, hi};
    const double fraction = 0.27;
    const double anchor = lo + fraction * (hi - lo);
    view = zoomFrequency(view, lo, hi, fraction, 1);
    assert(view.upper - view.lower < hi - lo);
    assert(std::abs(view.lower + fraction * (view.upper - view.lower) - anchor) < 1e-10);
    view = zoomFrequency(view, lo, hi, fraction, -1);
    assert(std::abs(view.lower - lo) < 1e-10 && std::abs(view.upper - hi) < 1e-10);
    for (int i = 0; i < 300; ++i) view = zoomFrequency(view, lo, hi, 0, 1);
    assert(view.lower >= lo && view.upper <= hi);
    assert(std::abs((view.upper-view.lower) - (hi-lo)/512.0) < 1e-10);
    for (int i = 0; i < 300; ++i) view = zoomFrequency(view, lo, hi, 1, -1);
    assert(std::abs(view.lower-lo) < 1e-10 && std::abs(view.upper-hi) < 1e-10);
    const double freqs[] = {10,20,30,40,50,60};
    const double amps[] = {100,2,3,9,5,200};
    assert((spectrumVisibleBins(freqs, amps, 6, 800, 20, 50) == std::vector<std::size_t>{1,2,3,4}));
    assert((spectrumVisibleBins(freqs, amps, 6, 2, 20, 50) == std::vector<std::size_t>{2,3}));
    assert(spectrumVisibleBins(freqs, amps, 6, 2, 70, 80).empty());
    assert(spectrumVisibleBins(nullptr, nullptr, 0, 800, 1, 2).empty());
    std::cout << "PASS: cursor anchor, zoom limits, wheel reversal, visible FFT bin selection\n";
}
