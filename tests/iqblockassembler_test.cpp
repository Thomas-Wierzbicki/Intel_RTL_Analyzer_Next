#include "../Intel_RTL_analyzer/iqblockassembler.h"
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

namespace {
constexpr std::size_t fftSize = 4096;
constexpr std::size_t blockBytes = fftSize * 2;

std::vector<unsigned char> makeStream(std::size_t size)
{
    std::mt19937 random(433500000);
    std::vector<unsigned char> bytes(size);
    for (auto& byte : bytes) byte = static_cast<unsigned char>(random());
    return bytes;
}

void verifyChunks(const std::vector<unsigned char>& input,
                  const std::vector<std::size_t>& chunks)
{
    IQBlockAssembler assembler(fftSize);
    std::vector<unsigned char> output;
    std::size_t blockCount = 0;
    auto collect = [&](const unsigned char* block) {
        ++blockCount;
        output.insert(output.end(), block, block + blockBytes);
    };
    std::size_t offset = 0;
    for (const auto requested : chunks) {
        const auto count = (std::min)(requested, input.size() - offset);
        assembler.append(input.data() + offset, count, collect);
        offset += count;
        assert(blockCount == offset / blockBytes);
    }
    assert(offset == input.size());
    assert(std::equal(output.begin(), output.end(), input.begin()));

    // Complete a pending block and verify every trailing byte was preserved.
    const auto remainder = offset % blockBytes;
    if (remainder != 0) {
        std::vector<unsigned char> padding(blockBytes - remainder, 0xA5);
        assembler.append(padding.data(), padding.size(), collect);
        auto expected = input;
        expected.insert(expected.end(), padding.begin(), padding.end());
        assert(output == expected);
    }
}
}

int main()
{
    const auto fullUsbBuffer = makeStream(262144);
    verifyChunks(fullUsbBuffer, {262144}); // Exactly 32 callbacks, not one.
    verifyChunks(makeStream(blockBytes * 3 + 17),
                 {0, 1, 8190, 0, 1, 8193, 16384});
    verifyChunks(makeStream(blockBytes + 1),
                 std::vector<std::size_t>(blockBytes + 1, 1));

    const auto longStream = makeStream(262144 * 3 + 7919);
    std::mt19937 random(42);
    std::vector<std::size_t> chunks;
    std::size_t remaining = longStream.size();
    while (remaining > 0) {
        const auto count = (std::min)(remaining,
            static_cast<std::size_t>(1 + random() % 20000));
        chunks.push_back(count);
        remaining -= count;
    }
    verifyChunks(longStream, chunks);

    // A signal entirely in the last USB block must reach the DSP callback.
    IQBlockAssembler assembler(fftSize);
    std::vector<unsigned char> signal(262144, 127);
    std::fill(signal.end() - blockBytes, signal.end(), 255);
    std::size_t blocks = 0;
    std::size_t signalBlocks = 0;
    assembler.append(signal.data(), signal.size(), [&](const unsigned char* block) {
        ++blocks;
        if (block[0] == 255) {
            ++signalBlocks;
            assert(blocks == 32);
        }
    });
    assert(blocks == 32 && signalBlocks == 1);

    // Reset discards a partial block after a source switch or read error.
    const auto fresh = makeStream(blockBytes);
    assembler.append(signal.data(), 17, [](const unsigned char*) { assert(false); });
    assembler.reset();
    blocks = 0;
    assembler.append(fresh.data(), fresh.size(), [&](const unsigned char* block) {
        ++blocks;
        assert(std::equal(fresh.begin(), fresh.end(), block));
    });
    assert(blocks == 1);

    bool rejected = false;
    try { IQBlockAssembler invalid(0); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    std::cout << "PASS: full buffers, fragmented/odd reads, retained tails, "
                 "late signal, empty reads, and reset\n";
}
