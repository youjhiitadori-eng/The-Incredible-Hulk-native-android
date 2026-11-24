// Decoder tekstur DDS (DXT1/DXT3/DXT5 dan RGB 32-bit) -> RGBA8. Dipakai entry TEXTURES.ARC.
// Entry tekstur = pembungkus "TXFL<nama>\0 <header>" lalu file DDS utuh (dicari lewat magic "DDS ").
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace hulk {

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;  // w*h*4
};

namespace dds_detail {
inline uint32_t rd32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }

inline void unpack565(uint16_t c, uint8_t* o) {
    o[0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
    o[1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
    o[2] = (uint8_t)((c & 31) * 255 / 31);
}

inline void colors(const uint8_t* b, uint8_t col[4][4], bool dxt1) {
    uint16_t c0 = (uint16_t)(b[0] | (b[1] << 8)), c1 = (uint16_t)(b[2] | (b[3] << 8));
    unpack565(c0, col[0]); unpack565(c1, col[1]);
    col[0][3] = col[1][3] = col[2][3] = 255;
    if (c0 > c1 || !dxt1) {
        for (int k = 0; k < 3; ++k) {
            col[2][k] = (uint8_t)((2 * col[0][k] + col[1][k]) / 3);
            col[3][k] = (uint8_t)((col[0][k] + 2 * col[1][k]) / 3);
        }
        col[3][3] = 255;
    } else {
        for (int k = 0; k < 3; ++k) {
            col[2][k] = (uint8_t)((col[0][k] + col[1][k]) / 2);
            col[3][k] = 0;
        }
        col[3][3] = 0;  // transparan
    }
}

enum Kind { BC1 = 1, BC2, BC3, RGB32 };

inline bool decodeBlocks(Kind kind, const uint8_t* d, size_t avail, int w, int h, uint8_t* out) {
    int bw = (w + 3) / 4, bh = (h + 3) / 4;
    size_t bs = (kind == BC1) ? 8 : 16;
    if (avail < (size_t)bw * bh * bs) return false;
    for (int by = 0; by < bh; ++by) {
        for (int bx = 0; bx < bw; ++bx) {
            const uint8_t* p = d + ((size_t)by * bw + bx) * bs;
            const uint8_t* cb = (kind == BC1) ? p : p + 8;
            uint8_t col[4][4];
            colors(cb, col, kind == BC1);
            uint32_t idx = rd32(cb + 4);
            uint8_t al[8];
            uint64_t abits = 0;
            if (kind == BC3) {
                al[0] = p[0]; al[1] = p[1];
                if (al[0] > al[1]) {
                    for (int i = 2; i < 8; ++i) al[i] = (uint8_t)(((8 - i) * al[0] + (i - 1) * al[1]) / 7);
                } else {
                    for (int i = 2; i < 6; ++i) al[i] = (uint8_t)(((6 - i) * al[0] + (i - 1) * al[1]) / 5);
                    al[6] = 0; al[7] = 255;
                }
                for (int i = 0; i < 6; ++i) abits |= (uint64_t)p[2 + i] << (8 * i);
            }
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    int x = bx * 4 + px, y = by * 4 + py;
                    if (x >= w || y >= h) continue;
                    int pi = py * 4 + px;
                    const uint8_t* c = col[(idx >> (2 * pi)) & 3];
                    uint8_t* o = out + ((size_t)y * w + x) * 4;
                    o[0] = c[0]; o[1] = c[1]; o[2] = c[2];
                    if (kind == BC1) o[3] = c[3];
                    else if (kind == BC2) {
                        uint8_t nib = (uint8_t)((p[pi / 2] >> ((pi & 1) * 4)) & 15);
                        o[3] = (uint8_t)(nib * 17);
                    } else o[3] = al[(abits >> (3 * pi)) & 7];
                }
            }
        }
    }
    return true;
}

inline uint8_t extract(uint32_t v, uint32_t mask) {
    if (!mask) return 255;
    int sh = __builtin_ctz(mask), bits = __builtin_popcount(mask);
    uint32_t val = (v & mask) >> sh;
    return (uint8_t)(val * 255 / ((1u << bits) - 1));
}
}  // namespace dds_detail

// maxDim membatasi sisi terpanjang (pilih mip lebih kecil) -> hemat RAM di HP kentang.
inline bool decodeTexture(const uint8_t* d, size_t n, int maxDim, Image& img) {
    using namespace dds_detail;
    size_t i = 0, lim = std::min<size_t>(n, 256);
    for (; i + 4 <= lim; ++i) if (!std::memcmp(d + i, "DDS ", 4)) break;
    if (i + 4 > lim || n < i + 128) return false;
    const uint8_t* h = d + i;
    int H = (int)rd32(h + 12), W = (int)rd32(h + 16);
    int mips = (int)rd32(h + 28);
    if (mips < 1) mips = 1;
    uint32_t pf = rd32(h + 80), bits = rd32(h + 88);
    uint32_t rm = rd32(h + 92), gm = rd32(h + 96), bm = rd32(h + 100), am = rd32(h + 104);
    if (W <= 0 || H <= 0 || W > 8192 || H > 8192) return false;

    Kind kind;
    if (pf & 4) {  // FOURCC
        if (!std::memcmp(h + 84, "DXT1", 4)) kind = BC1;
        else if (!std::memcmp(h + 84, "DXT3", 4)) kind = BC2;
        else if (!std::memcmp(h + 84, "DXT5", 4)) kind = BC3;
        else return false;
    } else if ((pf & 0x40) && bits == 32) kind = RGB32;
    else return false;

    auto levelSize = [&](int lw, int lh) -> size_t {
        if (kind == RGB32) return (size_t)lw * lh * 4;
        return (size_t)((lw + 3) / 4) * ((lh + 3) / 4) * (kind == BC1 ? 8 : 16);
    };
    int level = 0;
    while (level + 1 < mips && std::max(W >> level, H >> level) > maxDim) ++level;
    size_t off = 128;
    for (int l = 0; l < level; ++l) off += levelSize(std::max(1, W >> l), std::max(1, H >> l));
    int lw = std::max(1, W >> level), lh = std::max(1, H >> level);
    if (i + off > n) return false;
    const uint8_t* data = h + off;
    size_t avail = n - i - off;

    img.w = lw; img.h = lh;
    img.rgba.assign((size_t)lw * lh * 4, 255);
    if (kind == RGB32) {
        if (avail < (size_t)lw * lh * 4) return false;
        for (int p = 0; p < lw * lh; ++p) {
            uint32_t v = rd32(data + 4 * p);
            img.rgba[4 * p + 0] = extract(v, rm);
            img.rgba[4 * p + 1] = extract(v, gm);
            img.rgba[4 * p + 2] = extract(v, bm);
            img.rgba[4 * p + 3] = extract(v, am);
        }
        return true;
    }
    return decodeBlocks(kind, data, avail, lw, lh, img.rgba.data());
}

}  // namespace hulk
