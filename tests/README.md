# I/Q block assembly regression test

This test uses the same header as `SDRWorker` and needs neither Qt, Intel IPP
nor an RTL-SDR. Run from the repository root with assertions enabled:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -pedantic tests/iqblockassembler_test.cpp -o iqblockassembler_test
./iqblockassembler_test
```

With MSVC, use a Developer Command Prompt:

```bat
cl /EHsc /W4 tests\iqblockassembler_test.cpp /Fe:iqblockassembler_test.exe
iqblockassembler_test.exe
```

The test checks all 32 blocks of a 262144-byte USB read, exact byte order,
short and odd-sized reads, empty reads, trailing partial blocks, reset after
a discontinuity, and a synthetic signal confined to the last USB block.

The worker executes the FFT pipeline once per completed block. The GUI still
polls the latest spectrum at its existing refresh rate; this does not make
the GUI a lossless history of every FFT or add peak hold. A Windows application
build and an RTL-SDR hardware/throughput test are separate validation steps.
