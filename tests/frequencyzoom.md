# Mouse-wheel frequency zoom

Both windows zoom horizontally around the mouse pointer while it is inside the
plot. Positive wheel motion zooms in, negative motion zooms out. A left-button
double-click restores the full band. The two views zoom independently; power
and waterfall row-age axes remain unchanged. Zoom is display-only: no tuning,
sample-rate changes, peak reset or waterfall-history loss occurs.

The visible range is bounded by the captured band, with a minimum width of
1/512 of the full span (8 bins at the current 4096-point FFT). The line display
selects its at-most-800 points from the visible FFT bins, preserving selected
peak frequencies. Numeric axis ticks replace the old fixed labels.

Waterfall storage now retains all 4096 coloured bins per row so zoom can reveal
full FFT resolution. Only one row is written per update; the ring is still drawn
with two image calls, cropping the source horizontally. Storage is about 4.7 MiB
at 4096 * 300 * 4 bytes. At overview scale the raster is scaled to the window.
This supersedes the former 1024-column storage described in the earlier notes.

Regression test:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -pedantic tests/frequencyzoom_test.cpp -o frequencyzoom_test
./frequencyzoom_test
```

MSVC Developer Command Prompt:

```bat
cl /EHsc /W4 tests\frequencyzoom_test.cpp /Fe:frequencyzoom_test.exe
frequencyzoom_test.exe
```

Checks cursor-anchor preservation, zoom-in/out reversal, repeated wheel motion
at the range limits, visible-bin selection and empty ranges. Assertions must
remain enabled. C++11 AddressSanitizer/UndefinedBehaviorSanitizer test passed
in Linux (LeakSanitizer disabled due to environment limitations).

Windows validation: compile Debug/x64, start the simulator, point at the FM peak
and scroll in each window. Confirm four-decimal MHz labels, continued scrolling
in the waterfall, bounded zoom and double-click reset. Check responsiveness.
The full Qt/IPP application build and mouse interaction need target-PC testing.
