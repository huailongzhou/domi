#include "domi/material_io.h"

#include <cstdio>
#include <cstring>

namespace domi {

namespace {

const char kMagic[4] = { 'D', 'M', 'A', 'T' };

int bytesPerPixel(PixelFormat format) {
    switch (format) {
        case PixelFormat::ARGB8888:  return 4;
        case PixelFormat::RGB565:    return 2;
        case PixelFormat::LUT8:      return 1;
        case PixelFormat::AlphaMask: return 1;
    }
    return 0;
}

uint8_t toByte(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

} // namespace

bool saveMaterialFile(const char* path, const Material& material) {
    if (!path || material.width <= 0 || material.height <= 0) return false;
    const int bpp = bytesPerPixel(material.format);
    const size_t pixelBytes =
        static_cast<size_t>(material.width) * material.height * bpp;
    if (bpp == 0 || material.pixels.size() < pixelBytes) return false;

    FILE* f = fopen(path, "wb");
    if (!f) return false;

    const int32_t w = material.width;
    const int32_t h = material.height;
    const int32_t format = static_cast<int32_t>(material.format);
    const int32_t paletteSize = static_cast<int32_t>(
        material.format == PixelFormat::LUT8 ? material.palette.size() : 0);

    bool ok = fwrite(kMagic, 1, 4, f) == 4 &&
              fwrite(&w, sizeof(w), 1, f) == 1 &&
              fwrite(&h, sizeof(h), 1, f) == 1 &&
              fwrite(&format, sizeof(format), 1, f) == 1 &&
              fwrite(&paletteSize, sizeof(paletteSize), 1, f) == 1;
    for (int32_t i = 0; ok && i < paletteSize; ++i) {
        const Color& c = material.palette[i];
        const uint8_t entry[4] = { toByte(c.r), toByte(c.g), toByte(c.b), toByte(c.a) };
        ok = fwrite(entry, 1, 4, f) == 4;
    }
    if (ok) {
        ok = fwrite(material.pixels.data(), 1, pixelBytes, f) == pixelBytes;
    }

    fclose(f);
    return ok;
}

bool loadMaterialFile(const char* path, Material& out) {
    if (!path) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    char magic[4];
    int32_t w = 0, h = 0, format = 0, paletteSize = 0;
    bool ok = fread(magic, 1, 4, f) == 4 && memcmp(magic, kMagic, 4) == 0 &&
              fread(&w, sizeof(w), 1, f) == 1 &&
              fread(&h, sizeof(h), 1, f) == 1 &&
              fread(&format, sizeof(format), 1, f) == 1 &&
              fread(&paletteSize, sizeof(paletteSize), 1, f) == 1;
    if (ok && (w <= 0 || h <= 0 || format < 0 || format > 3 ||
               paletteSize < 0 || paletteSize > 256)) {
        ok = false;
    }

    Material material;
    if (ok) {
        material.width = w;
        material.height = h;
        material.format = static_cast<PixelFormat>(format);
        material.palette.reserve(paletteSize);
        for (int32_t i = 0; ok && i < paletteSize; ++i) {
            uint8_t entry[4];
            ok = fread(entry, 1, 4, f) == 4;
            if (ok) {
                material.palette.push_back(Color(
                    entry[0] / 255.0f, entry[1] / 255.0f,
                    entry[2] / 255.0f, entry[3] / 255.0f));
            }
        }
        const size_t pixelBytes =
            static_cast<size_t>(w) * h * bytesPerPixel(material.format);
        if (ok) {
            material.pixels.resize(pixelBytes);
            ok = fread(material.pixels.data(), 1, pixelBytes, f) == pixelBytes;
        }
    }

    fclose(f);
    if (!ok) return false;
    out = material;
    return true;
}

} // namespace domi
