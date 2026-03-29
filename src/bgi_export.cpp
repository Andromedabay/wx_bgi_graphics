/**
 * @file bgi_export.cpp
 * @brief PNG framebuffer export — wxbgi_export_png / wxbgi_export_png_window /
 *        wxbgi_export_png_camera_view.
 *
 * Encoding uses a self-contained minimal PNG writer:
 *  - CRC-32  (IEEE 802.3)   for PNG chunk integrity
 *  - Adler-32               for the zlib stream checksum
 *  - Deflate stored blocks  (BTYPE=00, no compression) for a valid zlib payload
 *
 * No external image libraries are required.  The resulting files are valid PNG
 * images that every browser, viewer, and document tool can open.  File size is
 * proportional to the pixel count (no lossless compression), which is fine for
 * documentation screenshots.
 */

#include "wx_bgi_ext.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <vector>

#include "bgi_camera.h"
#include "bgi_dds.h"
#include "bgi_draw.h"
#include "bgi_state.h"
#include "wx_bgi_dds.h"

// =============================================================================
// Anonymous namespace — PNG encoding helpers
// =============================================================================

namespace
{

// ---------------------------------------------------------------------------
// CRC-32 (IEEE 802.3 / PKZIP)
// ---------------------------------------------------------------------------

static uint32_t sCrcTable[256];

void initCrc32()
{
    for (uint32_t n = 0; n < 256; ++n)
    {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k)
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        sCrcTable[n] = c;
    }
}

uint32_t crc32(const uint8_t *buf, std::size_t len,
               uint32_t crc = 0xFFFFFFFFu)
{
    static bool init = false;
    if (!init) { initCrc32(); init = true; }
    for (std::size_t i = 0; i < len; ++i)
        crc = sCrcTable[(crc ^ buf[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// Adler-32  (used by the zlib framing around deflate data)
// ---------------------------------------------------------------------------

uint32_t adler32(const uint8_t *buf, std::size_t len)
{
    uint32_t s1 = 1, s2 = 0;
    for (std::size_t i = 0; i < len; ++i)
    {
        s1 = (s1 + buf[i]) % 65521u;
        s2 = (s2 + s1)     % 65521u;
    }
    return (s2 << 16) | s1;
}

// ---------------------------------------------------------------------------
// Write helpers
// ---------------------------------------------------------------------------

void pushU32BE(std::vector<uint8_t> &v, uint32_t val)
{
    v.push_back(static_cast<uint8_t>(val >> 24));
    v.push_back(static_cast<uint8_t>(val >> 16));
    v.push_back(static_cast<uint8_t>(val >>  8));
    v.push_back(static_cast<uint8_t>(val      ));
}

void pushU16LE(std::vector<uint8_t> &v, uint16_t val)
{
    v.push_back(static_cast<uint8_t>(val      ));
    v.push_back(static_cast<uint8_t>(val >> 8 ));
}

/** Write a PNG chunk: 4-byte length, 4-byte type, data, 4-byte CRC. */
void writeChunk(std::vector<uint8_t> &out,
                const char type[4],
                const uint8_t *data, std::size_t len)
{
    pushU32BE(out, static_cast<uint32_t>(len));

    const std::size_t typeStart = out.size();
    out.insert(out.end(), type, type + 4);
    if (data && len > 0)
        out.insert(out.end(), data, data + len);

    const uint32_t chunkCrc = crc32(out.data() + typeStart, 4 + len);
    pushU32BE(out, chunkCrc);
}

// ---------------------------------------------------------------------------
// Minimal PNG encoder — deflate stored blocks (valid, no compression)
// ---------------------------------------------------------------------------

/**
 * Encode an RGBA pixel buffer (top-down, no stride) as a PNG byte stream.
 *
 * Algorithm:
 *  1. Prepend a "None" filter byte (0x00) to every scanline.
 *  2. Wrap the filtered bytes in zlib framing:
 *       CMF=0x78 FLG=0x01   (window=32K, check bits divisible by 31)
 *       deflate stored blocks (BTYPE=00)
 *       Adler-32 of the uncompressed data (big-endian)
 *  3. Emit IHDR, IDAT, IEND chunks.
 */
std::vector<uint8_t> encodePng(const uint8_t *rgba, int width, int height)
{
    // 1. Filter: None (prepend 0x00 per row)
    const std::size_t rowBytes      = static_cast<std::size_t>(width) * 4;
    const std::size_t filteredLen   = static_cast<std::size_t>(height) * (1 + rowBytes);
    std::vector<uint8_t> filtered(filteredLen);
    for (int y = 0; y < height; ++y)
    {
        filtered[static_cast<std::size_t>(y) * (1 + rowBytes)] = 0x00;
        std::memcpy(&filtered[static_cast<std::size_t>(y) * (1 + rowBytes) + 1],
                    rgba + static_cast<std::size_t>(y) * rowBytes,
                    rowBytes);
    }

    // 2. zlib stream with stored deflate blocks
    // zlib header: CMF=0x78, FLG=0x01  →  0x7801 = 30721 = 31×991  ✓
    std::vector<uint8_t> zlib;
    zlib.reserve(filteredLen + 64);
    zlib.push_back(0x78);
    zlib.push_back(0x01);

    const uint8_t *src     = filtered.data();
    std::size_t    remain  = filteredLen;

    while (remain > 0)
    {
        const std::size_t blockLen = std::min(remain, std::size_t{65535});
        const bool        isFinal  = (blockLen == remain);

        // Block header: BFINAL | (BTYPE=00 << 1)
        zlib.push_back(isFinal ? 0x01u : 0x00u);

        const auto blen  = static_cast<uint16_t>(blockLen);
        const auto nblen = static_cast<uint16_t>(~blen);
        pushU16LE(zlib, blen);
        pushU16LE(zlib, nblen);

        zlib.insert(zlib.end(), src, src + blockLen);
        src    += blockLen;
        remain -= blockLen;
    }

    // Adler-32 of the uncompressed (filtered) data — big-endian
    pushU32BE(zlib, adler32(filtered.data(), filteredLen));

    // 3. Assemble PNG
    std::vector<uint8_t> png;
    png.reserve(zlib.size() + 256);

    // Signature
    const uint8_t sig[] = {0x89,'P','N','G','\r','\n',0x1A,'\n'};
    png.insert(png.end(), sig, sig + 8);

    // IHDR (13 bytes)
    uint8_t ihdr[13] = {};
    ihdr[0]  = static_cast<uint8_t>(width  >> 24);
    ihdr[1]  = static_cast<uint8_t>(width  >> 16);
    ihdr[2]  = static_cast<uint8_t>(width  >>  8);
    ihdr[3]  = static_cast<uint8_t>(width       );
    ihdr[4]  = static_cast<uint8_t>(height >> 24);
    ihdr[5]  = static_cast<uint8_t>(height >> 16);
    ihdr[6]  = static_cast<uint8_t>(height >>  8);
    ihdr[7]  = static_cast<uint8_t>(height      );
    ihdr[8]  = 8;   // bit depth
    ihdr[9]  = 6;   // color type: RGBA
    ihdr[10] = 0;   // compression method (deflate)
    ihdr[11] = 0;   // filter method
    ihdr[12] = 0;   // interlace: none
    writeChunk(png, "IHDR", ihdr, 13);

    // IDAT
    writeChunk(png, "IDAT", zlib.data(), zlib.size());

    // IEND
    writeChunk(png, "IEND", nullptr, 0);

    return png;
}

// ---------------------------------------------------------------------------
// Page buffer → RGBA helper (BGI color index → 8-bit RGBA)
// ---------------------------------------------------------------------------

std::vector<uint8_t> pageToRGBA(const std::vector<uint8_t> &buf, int w, int h)
{
    std::vector<uint8_t> rgba(static_cast<std::size_t>(w * h * 4));
    for (int i = 0; i < w * h; ++i)
    {
        const bgi::ColorRGB c = bgi::colorToRGB(buf[static_cast<std::size_t>(i)]);
        const std::size_t   s = static_cast<std::size_t>(i) * 4;
        rgba[s    ] = c.r;
        rgba[s + 1] = c.g;
        rgba[s + 2] = c.b;
        rgba[s + 3] = 255;
    }
    return rgba;
}

/** Extract a rectangular sub-region from the full page buffer. */
std::vector<uint8_t> subregionToRGBA(const std::vector<uint8_t> &buf,
                                      int fullW, int fullH,
                                      int vpx, int vpy, int vpw, int vph)
{
    std::vector<uint8_t> rgba(static_cast<std::size_t>(vpw * vph * 4), 0);
    for (int y = 0; y < vph; ++y)
    {
        for (int x = 0; x < vpw; ++x)
        {
            const int px = vpx + x;
            const int py = vpy + y;
            if (px < 0 || py < 0 || px >= fullW || py >= fullH)
                continue;
            const bgi::ColorRGB c =
                bgi::colorToRGB(buf[static_cast<std::size_t>(py * fullW + px)]);
            const std::size_t s = static_cast<std::size_t>((y * vpw + x) * 4);
            rgba[s    ] = c.r;
            rgba[s + 1] = c.g;
            rgba[s + 2] = c.b;
            rgba[s + 3] = 255;
        }
    }
    return rgba;
}

bool writePngFile(const char *filePath,
                  const uint8_t *rgba, int w, int h)
{
    if (!filePath || w <= 0 || h <= 0) return false;
    auto png = encodePng(rgba, w, h);
    std::ofstream f(filePath, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char *>(png.data()),
            static_cast<std::streamsize>(png.size()));
    return f.good();
}

} // namespace

// =============================================================================
// Public C API
// =============================================================================

BGI_API int BGI_CALL wxbgi_export_png(const char *filePath)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady()) return -1;

    const auto &buf = bgi::visualPageBuffer();
    const int   w   = bgi::gState.width;
    const int   h   = bgi::gState.height;

    const auto rgba = pageToRGBA(buf, w, h);
    return writePngFile(filePath, rgba.data(), w, h) ? 0 : -1;
}

BGI_API int BGI_CALL wxbgi_export_png_window(const char *filePath)
{
    // For a pure BGI context the visual page contains all rendered content.
    // Provided as a distinct entry point for callers that conceptually want
    // "everything displayed in the window" rather than just one page.
    return wxbgi_export_png(filePath);
}

BGI_API int BGI_CALL wxbgi_export_png_camera_view(const char *camName,
                                                   const char *filePath)
{
    // Render the DDS scene through the named camera into the active page.
    // wxbgi_render_dds acquires/releases gMutex internally.
    wxbgi_render_dds(camName);

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady()) return -1;

    const int fullW = bgi::gState.width;
    const int fullH = bgi::gState.height;

    // Resolve camera viewport (defaults to full window when not found).
    int vpx = 0, vpy = 0, vpw = fullW, vph = fullH;
    const std::string name = camName ? camName : bgi::gState.activeCamera;
    auto it = bgi::gState.cameras.find(name);
    if (it != bgi::gState.cameras.end())
    {
        bgi::cameraEffectiveViewport(it->second->camera, fullW, fullH,
                                     vpx, vpy, vpw, vph);
    }

    const auto &buf  = bgi::activePageBuffer();
    const auto  rgba = subregionToRGBA(buf, fullW, fullH, vpx, vpy, vpw, vph);

    return writePngFile(filePath, rgba.data(), vpw, vph) ? 0 : -1;
}
