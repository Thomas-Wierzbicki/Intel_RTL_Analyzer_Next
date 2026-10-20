# Spectrum rendering performance

The spectrum uses cosmetic one-pixel strokes without plottable antialiasing,
QCustomPlot fast polylines and at most 800 points per curve, bounded by plot
width. Each display group selects its strongest bin at that bin's actual
frequency. This is an upper-envelope display reduction: weaker bins within the
same group are not drawn, but FFT and peak-hold storage keep all 4096 bins.

The base display interval is 80 ms (12.5 FPS), increasing up to 500 ms when
measured QCustomPlot replot time is high. Hidden/minimized spectrum windows
skip periodic display work. Acquisition and peak accumulation are unaffected.

The status bar shows `Linienbild: ... ms | Intervall: ... ms`. The first number
measures beforeReplot-to-afterReplot duration, not end-to-end latency or the
Windows compositor. Record these values with both windows open at normal size,
and compare responsiveness with the spectrum minimized.

Regression test (assertions enabled):

```sh
g++ -std=c++11 -Wall -Wextra -Werror -pedantic tests/spectrumdisplay_test.cpp -o spectrumdisplay_test
./spectrumdisplay_test
```

MSVC Developer Command Prompt:

```bat
cl /EHsc /W4 tests\spectrumdisplay_test.cpp /Fe:spectrumdisplay_test.exe
spectrumdisplay_test.exe
```

Tests cover an isolated peak at each of 4096 positions, monotonic selected
frequencies, the 800-point budget, unequal groups and empty inputs. Linux
AddressSanitizer/UndefinedBehaviorSanitizer tests passed; the new Windows Qt/IPP
build and actual rendering performance still require validation on the target PC.
