#pragma once

#include <stddef.h>
#include <stdint.h>

// Relative RAW8 level, sampling complete Bayer cells in the central 75% of
// the image so all four colour-filter positions contribute equally.
static inline int brightness_percent(const uint8_t *frame, size_t length,
                                     unsigned width, unsigned height, unsigned stride)
{
    if (!frame || width < 4 || height < 4 || stride < width || height > length / stride) {
        return -1;
    }
    uint64_t sum = 0;
    unsigned count = 0;
    const unsigned x_start = (width / 8) & ~1U;
    const unsigned y_start = (height / 8) & ~1U;
    for (unsigned y = y_start; y + 1 < height - y_start; y += 16) {
        for (unsigned x = x_start; x + 1 < width - x_start; x += 16) {
            const uint8_t *p = frame + (size_t)y * stride + x;
            sum += p[0] + p[1] + p[stride] + p[stride + 1];
            count += 4;
        }
    }
    return count ? (int)((sum * 100 + (uint64_t)count * 255 / 2) / ((uint64_t)count * 255)) : -1;
}
