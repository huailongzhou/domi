#ifndef DOMI_MATERIAL_IO_H
#define DOMI_MATERIAL_IO_H

#include "domi/material.h"

namespace domi {

// Raw material file (.tex): a small header followed by the unencoded pixel
// bytes (no compression, no image encoding).
//
// Layout (little-endian, all integers int32):
//   char[4]  magic = "DMAT"
//   int32    width
//   int32    height
//   int32    format (PixelFormat)
//   int32    paletteSize (0 unless format == LUT8)
//   paletteSize * 4 bytes: palette entries (r, g, b, a as 0-255)
//   width * height * bytesPerPixel: pixel data, row-major
bool saveMaterialFile(const char* path, const Material& material);
bool loadMaterialFile(const char* path, Material& out);

} // namespace domi

#endif // DOMI_MATERIAL_IO_H
