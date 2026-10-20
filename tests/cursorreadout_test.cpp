#include "../Intel_RTL_analyzer/cursorreadout.h"
#include <cassert>
#include <iostream>
#include <vector>
int main()
{
    FrequencyView view{433.0, 433.2};
    const auto moved = panFrequency(view, 432.3, 434.7, 0.1);
    assert(std::abs(moved.lower - 433.02) < 1e-10);
    assert(std::abs((moved.upper-moved.lower) - 0.2) < 1e-10);
    const auto back = panFrequency(moved, 432.3, 434.7, -0.1);
    assert(std::abs(back.lower-view.lower) < 1e-10);
    for (int i = 0; i < 500; ++i) view = panFrequency(view, 432.3, 434.7, 0.1);
    assert(std::abs(view.upper-434.7) < 1e-10);
    for (int i = 0; i < 500; ++i) view = panFrequency(view, 432.3, 434.7, -0.1);
    assert(std::abs(view.lower-432.3) < 1e-10);
    const auto full = panFrequency({432.3,434.7},432.3,434.7,0.1);
    assert(full.lower == 432.3 && full.upper == 434.7);

    const double f[] = {10,20,30};
    const double db[] = {-60,-20,-40};
    const auto live = pickSpectrumPoint(f,db,3,{10,30},-100,0,200,100,100,20);
    assert(live.valid && live.index == 1 && live.distanceSquared == 0);
    const double peak[] = {-50,-10,-30};
    const auto held = pickSpectrumPoint(f,peak,3,{10,30},-100,0,200,100,100,10);
    const auto liveAtPeak = pickSpectrumPoint(f,db,3,{10,30},-100,0,200,100,100,10);
    assert(held.distanceSquared < liveAtPeak.distanceSquared);
    assert(!pickSpectrumPoint(f,db,3,{40,50},-100,0,200,100,100,10).valid);
    assert(!pickSpectrumPoint(nullptr,nullptr,0,{10,30},-100,0,200,100,100,10).valid);

    constexpr int rows = 300, columns = 8;
    std::vector<float> buffer(rows * columns, std::numeric_limits<float>::quiet_NaN());
    int head = 0;
    for (int sequence = 1; sequence <= 950; ++sequence) {
        head = (head + rows - 1) % rows;
        for (int bin = 0; bin < columns; ++bin) buffer[head*columns+bin] = static_cast<float>(sequence*10+bin);
        for (int age : {0,1,150,299}) {
            const auto pick = pickWaterfallCell(head,columns,rows,{10,18},10,18,0.3,(age+0.5)/rows);
            assert(pick.valid && pick.bin == 2 && pick.age == age);
            const float value = buffer[pick.storageRow*columns+pick.bin];
            if (sequence <= age) assert(std::isnan(value));
            else assert(value == (sequence-age)*10+2);
        }
    }
    const auto edge = pickWaterfallCell(head,8,300,{10,18},10,18,1,1);
    assert(edge.bin == 7 && edge.age == 299);
    const auto zoomed = pickWaterfallCell(head,8,300,{12,14},10,18,0.5,0);
    assert(zoomed.bin == 3);
    assert(!pickWaterfallCell(head,8,300,{10,18},10,18,-0.1,0).valid);
    std::cout << "PASS: pan limits, width preservation, live/peak picking, zoomed waterfall lookup across 950 rows and empty cells\n";
}
