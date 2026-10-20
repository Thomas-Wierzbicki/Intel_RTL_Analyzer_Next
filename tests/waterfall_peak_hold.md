# Waterfall and peak hold

Peak hold (yellow) accumulates the maximum in each frequency bin from every
completed FFT, including FFTs between GUI refreshes. The checkbox only controls
visibility; collection continues while hidden. The reset button starts a new
collection. Switching hardware/simulator clears peaks and waterfall history when
the first FFT from the new source is published. Moving the simulator frequency
slider intentionally retains peaks to demonstrate the frequency sweep.

The waterfall shows the latest available FFT once per new sequence number at its
45 ms polling interval. New rows appear at the top, older rows move downward,
and only the newest 300 display rows are retained. The axis counts display rows,
not seconds or all acquired FFT blocks. It is not a lossless FFT recording.
No new data means no new row. A QImage stores 300 pre-coloured rows; an update
only colours the newest row and moves a ring index. Painting uses at most two
image draws in chronological order. All FFT bins contribute to at most 1024
waterfall display columns via maximum pooling, preserving narrow peaks within
each group. This display reduction does not change FFT or peak-hold resolution.
Painting still costs work proportional to the displayed image area; this is not
an O(1) rendering claim. Hidden/minimized waterfall windows suspend updates.
GUI snapshots use a non-blocking lock attempt; contention defers the display
refresh, not the FFT processing. Windows performance still needs measurement.

## Automated regression test (no Qt, IPP or hardware required)

From the repository root:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -pedantic tests/spectrumhistory_test.cpp -o spectrumhistory_test
./spectrumhistory_test
```

MSVC Developer Command Prompt:

```bat
cl /EHsc /W4 tests\spectrumhistory_test.cpp /Fe:spectrumhistory_test.exe
spectrumhistory_test.exe
```

Keep assertions enabled. Tests cover a transient peak between display updates,
peak reset, size changes, invalid values, more than two waterfall history wraps,
chronological ordering, duplicate/stale sequence suppression, and source reset.

## Windows GUI check

1. Build Debug/x64 and start the application.
2. Enable the FM simulator and move the frequency slider. The yellow line should
   retain earlier signal maxima while the blue trace follows the current signal.
3. Hide/show Peak Hold and use the reset button to start collection again.
4. Watch new waterfall rows appear at the top; after more than 300 updates,
   chronological scrolling must continue without a circular overwrite boundary.
5. Switch back to hardware: simulator peaks/history must not remain.
6. Observe responsiveness and CPU load. A full Qt/IPP build and a hardware
   throughput test have not been run in the Linux review environment.


## Raster regression test

```sh
g++ -std=c++11 -Wall -Wextra -Werror -pedantic tests/waterfallraster_test.cpp -o waterfallraster_test
./waterfallraster_test
```

MSVC: `cl /EHsc /W4 tests\waterfallraster_test.cpp /Fe:waterfallraster_test.exe`.
The test covers 950 insertions, blank initial rows, both image spans, reset,
first/last-bin peaks and unequal frequency-bin grouping. The ring image insert
uses at most 1024 pixel writes, replacing 4096 * 300 QCPColorMap cell writes
per refresh. This operation-count comparison is not a measured Windows speedup.
