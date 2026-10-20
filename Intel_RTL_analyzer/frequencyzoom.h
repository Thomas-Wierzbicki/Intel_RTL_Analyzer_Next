#ifndef FREQUENCYZOOM_H
#define FREQUENCYZOOM_H
#include <algorithm>
#include <cmath>
struct FrequencyView { double lower; double upper; };
inline FrequencyView zoomFrequency(FrequencyView view, double fullLower,
                                   double fullUpper, double fraction, double steps)
{
    const double fullWidth = fullUpper - fullLower;
    if (!(fullWidth > 0) || !std::isfinite(steps)) return view;
    fraction = (std::max)(0.0, (std::min)(1.0, fraction));
    steps = (std::max)(-20.0, (std::min)(20.0, steps));
    const double oldWidth = view.upper - view.lower;
    const double width = (std::max)(fullWidth / 512.0,
        (std::min)(fullWidth, oldWidth * std::pow(0.85, steps)));
    const double anchor = view.lower + fraction * oldWidth;
    double lower = anchor - fraction * width;
    lower = (std::max)(fullLower, (std::min)(fullUpper - width, lower));
    return {lower, lower + width};
}
inline FrequencyView panFrequency(FrequencyView view, double fullLower,
                                  double fullUpper, double fraction)
{
    const double width = view.upper - view.lower;
    const double fullWidth = fullUpper - fullLower;
    if (!(width > 0) || !(fullWidth > 0) || !std::isfinite(fraction)) return view;
    if (width >= fullWidth) return {fullLower, fullUpper};
    double lower = view.lower + width * fraction;
    lower = (std::max)(fullLower, (std::min)(fullUpper - width, lower));
    return {lower, lower + width};
}
#endif

