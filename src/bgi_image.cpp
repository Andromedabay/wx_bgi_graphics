#include "bgi_image.h"

#include "bgi_draw.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace bgi
{

    unsigned imageSizeForRect(int left, int top, int right, int bottom)
    {
        const int width = std::max(0, std::abs(right - left) + 1);
        const int height = std::max(0, std::abs(bottom - top) + 1);
        return static_cast<unsigned>(sizeof(ImageHeader) +
                                     static_cast<unsigned>(width) * static_cast<unsigned>(height));
    }

    bool captureImage(int left, int top, int right, int bottom, void *bitmap)
    {
        if (bitmap == nullptr)
        {
            return false;
        }

        const int minX = std::min(left, right);
        const int maxX = std::max(left, right);
        const int minY = std::min(top, bottom);
        const int maxY = std::max(top, bottom);

        auto *header = static_cast<ImageHeader *>(bitmap);
        header->width = static_cast<std::uint32_t>(maxX - minX + 1);
        header->height = static_cast<std::uint32_t>(maxY - minY + 1);

        auto *pixels = reinterpret_cast<std::uint8_t *>(header + 1);
        std::size_t offset = 0;
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                const int color = getPixel(x, y);
                pixels[offset++] = static_cast<std::uint8_t>(color < 0 ? 0 : color);
            }
        }

        return true;
    }

    bool drawImage(int left, int top, const void *bitmap, int op)
    {
        if (bitmap == nullptr)
        {
            return false;
        }

        const auto *header = static_cast<const ImageHeader *>(bitmap);
        const auto *pixels = reinterpret_cast<const std::uint8_t *>(header + 1);

        for (std::uint32_t y = 0; y < header->height; ++y)
        {
            for (std::uint32_t x = 0; x < header->width; ++x)
            {
                const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(header->width) +
                                          static_cast<std::size_t>(x);
                setPixelWithMode(left + static_cast<int>(x), top + static_cast<int>(y), pixels[index], op);
            }
        }

        return true;
    }

} // namespace bgi