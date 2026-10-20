#include "../Intel_RTL_analyzer/waterfallraster.h"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
    WaterfallRowRing ring(300);
    std::vector<int> pixels(300, 0);
    for (int sequence = 1; sequence <= 950; ++sequence) {
        pixels[ring.advance()] = sequence;
        // This is exactly the order of the two image spans in paintEvent.
        std::vector<int> display(pixels.begin() + ring.head(), pixels.end());
        display.insert(display.end(), pixels.begin(), pixels.begin() + ring.head());
        assert(ring.firstSpan() + ring.head() == 300);
        for (int age = 0; age < 300; ++age)
            assert(display[age] == (sequence > age ? sequence - age : 0));
    }
    ring.clear();
    assert(ring.head() == 0 && ring.firstSpan() == 300);
    assert(ring.advance() == 299);
    std::vector<double> data(4096, -90);
    for (std::size_t index : {std::size_t(0), std::size_t(3), std::size_t(2048), std::size_t(4095)}) {
        data[index] = 20;
        const auto row = waterfallDisplayRow(data, 1024);
        assert(row.size() == 1024 && row[index / 4] == 20);
        data[index] = -90;
    }
    assert((waterfallDisplayRow(std::vector<double>{1, 2, 3, 4, 5}, 2) == std::vector<double>{2, 5}));
    assert((waterfallDisplayRow(std::vector<double>{1, 2}, 1024) == std::vector<double>{1, 2}));
    assert(waterfallDisplayRow(data, 0).empty());
    assert(waterfallDisplayRow(std::vector<double>{}, 1024).empty());
    assert(waterfallDisplayRow(std::vector<double>{std::numeric_limits<double>::quiet_NaN()}, 1)[0] == -200);
    std::cout << "PASS: 950 raster insertions, blank rows, chronology, reset, narrow-peak pooling, edge bins\n";
}
