#ifndef IQBLOCKASSEMBLER_H
#define IQBLOCKASSEMBLER_H

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vector>

// Collect interleaved unsigned 8-bit I/Q bytes into complete FFT blocks.
// Byte-wise buffering also preserves I/Q alignment across odd-sized reads.
// Used exclusively by the acquisition thread. The callback must consume the
// block immediately; the storage is reused after it returns.
class IQBlockAssembler
{
public:
    explicit IQBlockAssembler(std::size_t samplesPerBlock)
        : m_block(samplesPerBlock * 2), m_used(0)
    {
        if (samplesPerBlock == 0) {
            throw std::invalid_argument("FFT block size must be positive");
        }
    }

    void reset() { m_used = 0; }

    template <typename ProcessBlock>
    void append(const unsigned char* bytes, std::size_t size, ProcessBlock processBlock)
    {
        while (size > 0) {
            const std::size_t count = (std::min)(size, m_block.size() - m_used);
            std::memcpy(m_block.data() + m_used, bytes, count);
            m_used += count;
            bytes += count;
            size -= count;

            if (m_used == m_block.size()) {
                processBlock(m_block.data());
                m_used = 0;
            }
        }
    }

private:
    std::vector<unsigned char> m_block;
    std::size_t m_used;
};

#endif // IQBLOCKASSEMBLER_H
