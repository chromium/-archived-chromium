// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/yuv_row.h"

// Enable bilinear filtering by turning on the following macro.
//  #define MEDIA_BILINEAR_FILTER 1

namespace media {

#define RGBY(i) { \
  static_cast<int16>(1.164 * 64 * (i - 16) + 0.5), \
  static_cast<int16>(1.164 * 64 * (i - 16) + 0.5), \
  static_cast<int16>(1.164 * 64 * (i - 16) + 0.5), \
  0 \
}

#define RGBU(i) { \
  static_cast<int16>(2.018 * 64 * (i - 128) + 0.5), \
  static_cast<int16>(-0.391 * 64 * (i - 128) + 0.5), \
  0, \
  static_cast<int16>(256 * 64 - 1) \
}

#define RGBV(i) { \
  0, \
  static_cast<int16>(-0.813 * 64 * (i - 128) + 0.5), \
  static_cast<int16>(1.596 * 64 * (i - 128) + 0.5), \
  0 \
}

#define MMX_ALIGNED(var) __declspec(align(16)) var

extern "C" {
MMX_ALIGNED(int16 coefficients_RGB_Y[256][4]) = {
  RGBY(0x00), RGBY(0x01), RGBY(0x02), RGBY(0x03),
  RGBY(0x04), RGBY(0x05), RGBY(0x06), RGBY(0x07),
  RGBY(0x08), RGBY(0x09), RGBY(0x0A), RGBY(0x0B),
  RGBY(0x0C), RGBY(0x0D), RGBY(0x0E), RGBY(0x0F),
  RGBY(0x10), RGBY(0x11), RGBY(0x12), RGBY(0x13),
  RGBY(0x14), RGBY(0x15), RGBY(0x16), RGBY(0x17),
  RGBY(0x18), RGBY(0x19), RGBY(0x1A), RGBY(0x1B),
  RGBY(0x1C), RGBY(0x1D), RGBY(0x1E), RGBY(0x1F),
  RGBY(0x20), RGBY(0x21), RGBY(0x22), RGBY(0x23),
  RGBY(0x24), RGBY(0x25), RGBY(0x26), RGBY(0x27),
  RGBY(0x28), RGBY(0x29), RGBY(0x2A), RGBY(0x2B),
  RGBY(0x2C), RGBY(0x2D), RGBY(0x2E), RGBY(0x2F),
  RGBY(0x30), RGBY(0x31), RGBY(0x32), RGBY(0x33),
  RGBY(0x34), RGBY(0x35), RGBY(0x36), RGBY(0x37),
  RGBY(0x38), RGBY(0x39), RGBY(0x3A), RGBY(0x3B),
  RGBY(0x3C), RGBY(0x3D), RGBY(0x3E), RGBY(0x3F),
  RGBY(0x40), RGBY(0x41), RGBY(0x42), RGBY(0x43),
  RGBY(0x44), RGBY(0x45), RGBY(0x46), RGBY(0x47),
  RGBY(0x48), RGBY(0x49), RGBY(0x4A), RGBY(0x4B),
  RGBY(0x4C), RGBY(0x4D), RGBY(0x4E), RGBY(0x4F),
  RGBY(0x50), RGBY(0x51), RGBY(0x52), RGBY(0x53),
  RGBY(0x54), RGBY(0x55), RGBY(0x56), RGBY(0x57),
  RGBY(0x58), RGBY(0x59), RGBY(0x5A), RGBY(0x5B),
  RGBY(0x5C), RGBY(0x5D), RGBY(0x5E), RGBY(0x5F),
  RGBY(0x60), RGBY(0x61), RGBY(0x62), RGBY(0x63),
  RGBY(0x64), RGBY(0x65), RGBY(0x66), RGBY(0x67),
  RGBY(0x68), RGBY(0x69), RGBY(0x6A), RGBY(0x6B),
  RGBY(0x6C), RGBY(0x6D), RGBY(0x6E), RGBY(0x6F),
  RGBY(0x70), RGBY(0x71), RGBY(0x72), RGBY(0x73),
  RGBY(0x74), RGBY(0x75), RGBY(0x76), RGBY(0x77),
  RGBY(0x78), RGBY(0x79), RGBY(0x7A), RGBY(0x7B),
  RGBY(0x7C), RGBY(0x7D), RGBY(0x7E), RGBY(0x7F),
  RGBY(0x80), RGBY(0x81), RGBY(0x82), RGBY(0x83),
  RGBY(0x84), RGBY(0x85), RGBY(0x86), RGBY(0x87),
  RGBY(0x88), RGBY(0x89), RGBY(0x8A), RGBY(0x8B),
  RGBY(0x8C), RGBY(0x8D), RGBY(0x8E), RGBY(0x8F),
  RGBY(0x90), RGBY(0x91), RGBY(0x92), RGBY(0x93),
  RGBY(0x94), RGBY(0x95), RGBY(0x96), RGBY(0x97),
  RGBY(0x98), RGBY(0x99), RGBY(0x9A), RGBY(0x9B),
  RGBY(0x9C), RGBY(0x9D), RGBY(0x9E), RGBY(0x9F),
  RGBY(0xA0), RGBY(0xA1), RGBY(0xA2), RGBY(0xA3),
  RGBY(0xA4), RGBY(0xA5), RGBY(0xA6), RGBY(0xA7),
  RGBY(0xA8), RGBY(0xA9), RGBY(0xAA), RGBY(0xAB),
  RGBY(0xAC), RGBY(0xAD), RGBY(0xAE), RGBY(0xAF),
  RGBY(0xB0), RGBY(0xB1), RGBY(0xB2), RGBY(0xB3),
  RGBY(0xB4), RGBY(0xB5), RGBY(0xB6), RGBY(0xB7),
  RGBY(0xB8), RGBY(0xB9), RGBY(0xBA), RGBY(0xBB),
  RGBY(0xBC), RGBY(0xBD), RGBY(0xBE), RGBY(0xBF),
  RGBY(0xC0), RGBY(0xC1), RGBY(0xC2), RGBY(0xC3),
  RGBY(0xC4), RGBY(0xC5), RGBY(0xC6), RGBY(0xC7),
  RGBY(0xC8), RGBY(0xC9), RGBY(0xCA), RGBY(0xCB),
  RGBY(0xCC), RGBY(0xCD), RGBY(0xCE), RGBY(0xCF),
  RGBY(0xD0), RGBY(0xD1), RGBY(0xD2), RGBY(0xD3),
  RGBY(0xD4), RGBY(0xD5), RGBY(0xD6), RGBY(0xD7),
  RGBY(0xD8), RGBY(0xD9), RGBY(0xDA), RGBY(0xDB),
  RGBY(0xDC), RGBY(0xDD), RGBY(0xDE), RGBY(0xDF),
  RGBY(0xE0), RGBY(0xE1), RGBY(0xE2), RGBY(0xE3),
  RGBY(0xE4), RGBY(0xE5), RGBY(0xE6), RGBY(0xE7),
  RGBY(0xE8), RGBY(0xE9), RGBY(0xEA), RGBY(0xEB),
  RGBY(0xEC), RGBY(0xED), RGBY(0xEE), RGBY(0xEF),
  RGBY(0xF0), RGBY(0xF1), RGBY(0xF2), RGBY(0xF3),
  RGBY(0xF4), RGBY(0xF5), RGBY(0xF6), RGBY(0xF7),
  RGBY(0xF8), RGBY(0xF9), RGBY(0xFA), RGBY(0xFB),
  RGBY(0xFC), RGBY(0xFD), RGBY(0xFE), RGBY(0xFF),
};

MMX_ALIGNED(int16 coefficients_RGB_U[256][4]) = {
  RGBU(0x00), RGBU(0x01), RGBU(0x02), RGBU(0x03),
  RGBU(0x04), RGBU(0x05), RGBU(0x06), RGBU(0x07),
  RGBU(0x08), RGBU(0x09), RGBU(0x0A), RGBU(0x0B),
  RGBU(0x0C), RGBU(0x0D), RGBU(0x0E), RGBU(0x0F),
  RGBU(0x10), RGBU(0x11), RGBU(0x12), RGBU(0x13),
  RGBU(0x14), RGBU(0x15), RGBU(0x16), RGBU(0x17),
  RGBU(0x18), RGBU(0x19), RGBU(0x1A), RGBU(0x1B),
  RGBU(0x1C), RGBU(0x1D), RGBU(0x1E), RGBU(0x1F),
  RGBU(0x20), RGBU(0x21), RGBU(0x22), RGBU(0x23),
  RGBU(0x24), RGBU(0x25), RGBU(0x26), RGBU(0x27),
  RGBU(0x28), RGBU(0x29), RGBU(0x2A), RGBU(0x2B),
  RGBU(0x2C), RGBU(0x2D), RGBU(0x2E), RGBU(0x2F),
  RGBU(0x30), RGBU(0x31), RGBU(0x32), RGBU(0x33),
  RGBU(0x34), RGBU(0x35), RGBU(0x36), RGBU(0x37),
  RGBU(0x38), RGBU(0x39), RGBU(0x3A), RGBU(0x3B),
  RGBU(0x3C), RGBU(0x3D), RGBU(0x3E), RGBU(0x3F),
  RGBU(0x40), RGBU(0x41), RGBU(0x42), RGBU(0x43),
  RGBU(0x44), RGBU(0x45), RGBU(0x46), RGBU(0x47),
  RGBU(0x48), RGBU(0x49), RGBU(0x4A), RGBU(0x4B),
  RGBU(0x4C), RGBU(0x4D), RGBU(0x4E), RGBU(0x4F),
  RGBU(0x50), RGBU(0x51), RGBU(0x52), RGBU(0x53),
  RGBU(0x54), RGBU(0x55), RGBU(0x56), RGBU(0x57),
  RGBU(0x58), RGBU(0x59), RGBU(0x5A), RGBU(0x5B),
  RGBU(0x5C), RGBU(0x5D), RGBU(0x5E), RGBU(0x5F),
  RGBU(0x60), RGBU(0x61), RGBU(0x62), RGBU(0x63),
  RGBU(0x64), RGBU(0x65), RGBU(0x66), RGBU(0x67),
  RGBU(0x68), RGBU(0x69), RGBU(0x6A), RGBU(0x6B),
  RGBU(0x6C), RGBU(0x6D), RGBU(0x6E), RGBU(0x6F),
  RGBU(0x70), RGBU(0x71), RGBU(0x72), RGBU(0x73),
  RGBU(0x74), RGBU(0x75), RGBU(0x76), RGBU(0x77),
  RGBU(0x78), RGBU(0x79), RGBU(0x7A), RGBU(0x7B),
  RGBU(0x7C), RGBU(0x7D), RGBU(0x7E), RGBU(0x7F),
  RGBU(0x80), RGBU(0x81), RGBU(0x82), RGBU(0x83),
  RGBU(0x84), RGBU(0x85), RGBU(0x86), RGBU(0x87),
  RGBU(0x88), RGBU(0x89), RGBU(0x8A), RGBU(0x8B),
  RGBU(0x8C), RGBU(0x8D), RGBU(0x8E), RGBU(0x8F),
  RGBU(0x90), RGBU(0x91), RGBU(0x92), RGBU(0x93),
  RGBU(0x94), RGBU(0x95), RGBU(0x96), RGBU(0x97),
  RGBU(0x98), RGBU(0x99), RGBU(0x9A), RGBU(0x9B),
  RGBU(0x9C), RGBU(0x9D), RGBU(0x9E), RGBU(0x9F),
  RGBU(0xA0), RGBU(0xA1), RGBU(0xA2), RGBU(0xA3),
  RGBU(0xA4), RGBU(0xA5), RGBU(0xA6), RGBU(0xA7),
  RGBU(0xA8), RGBU(0xA9), RGBU(0xAA), RGBU(0xAB),
  RGBU(0xAC), RGBU(0xAD), RGBU(0xAE), RGBU(0xAF),
  RGBU(0xB0), RGBU(0xB1), RGBU(0xB2), RGBU(0xB3),
  RGBU(0xB4), RGBU(0xB5), RGBU(0xB6), RGBU(0xB7),
  RGBU(0xB8), RGBU(0xB9), RGBU(0xBA), RGBU(0xBB),
  RGBU(0xBC), RGBU(0xBD), RGBU(0xBE), RGBU(0xBF),
  RGBU(0xC0), RGBU(0xC1), RGBU(0xC2), RGBU(0xC3),
  RGBU(0xC4), RGBU(0xC5), RGBU(0xC6), RGBU(0xC7),
  RGBU(0xC8), RGBU(0xC9), RGBU(0xCA), RGBU(0xCB),
  RGBU(0xCC), RGBU(0xCD), RGBU(0xCE), RGBU(0xCF),
  RGBU(0xD0), RGBU(0xD1), RGBU(0xD2), RGBU(0xD3),
  RGBU(0xD4), RGBU(0xD5), RGBU(0xD6), RGBU(0xD7),
  RGBU(0xD8), RGBU(0xD9), RGBU(0xDA), RGBU(0xDB),
  RGBU(0xDC), RGBU(0xDD), RGBU(0xDE), RGBU(0xDF),
  RGBU(0xE0), RGBU(0xE1), RGBU(0xE2), RGBU(0xE3),
  RGBU(0xE4), RGBU(0xE5), RGBU(0xE6), RGBU(0xE7),
  RGBU(0xE8), RGBU(0xE9), RGBU(0xEA), RGBU(0xEB),
  RGBU(0xEC), RGBU(0xED), RGBU(0xEE), RGBU(0xEF),
  RGBU(0xF0), RGBU(0xF1), RGBU(0xF2), RGBU(0xF3),
  RGBU(0xF4), RGBU(0xF5), RGBU(0xF6), RGBU(0xF7),
  RGBU(0xF8), RGBU(0xF9), RGBU(0xFA), RGBU(0xFB),
  RGBU(0xFC), RGBU(0xFD), RGBU(0xFE), RGBU(0xFF),
};

MMX_ALIGNED(int16 coefficients_RGB_V[256][4]) = {
  RGBV(0x00), RGBV(0x01), RGBV(0x02), RGBV(0x03),
  RGBV(0x04), RGBV(0x05), RGBV(0x06), RGBV(0x07),
  RGBV(0x08), RGBV(0x09), RGBV(0x0A), RGBV(0x0B),
  RGBV(0x0C), RGBV(0x0D), RGBV(0x0E), RGBV(0x0F),
  RGBV(0x10), RGBV(0x11), RGBV(0x12), RGBV(0x13),
  RGBV(0x14), RGBV(0x15), RGBV(0x16), RGBV(0x17),
  RGBV(0x18), RGBV(0x19), RGBV(0x1A), RGBV(0x1B),
  RGBV(0x1C), RGBV(0x1D), RGBV(0x1E), RGBV(0x1F),
  RGBV(0x20), RGBV(0x21), RGBV(0x22), RGBV(0x23),
  RGBV(0x24), RGBV(0x25), RGBV(0x26), RGBV(0x27),
  RGBV(0x28), RGBV(0x29), RGBV(0x2A), RGBV(0x2B),
  RGBV(0x2C), RGBV(0x2D), RGBV(0x2E), RGBV(0x2F),
  RGBV(0x30), RGBV(0x31), RGBV(0x32), RGBV(0x33),
  RGBV(0x34), RGBV(0x35), RGBV(0x36), RGBV(0x37),
  RGBV(0x38), RGBV(0x39), RGBV(0x3A), RGBV(0x3B),
  RGBV(0x3C), RGBV(0x3D), RGBV(0x3E), RGBV(0x3F),
  RGBV(0x40), RGBV(0x41), RGBV(0x42), RGBV(0x43),
  RGBV(0x44), RGBV(0x45), RGBV(0x46), RGBV(0x47),
  RGBV(0x48), RGBV(0x49), RGBV(0x4A), RGBV(0x4B),
  RGBV(0x4C), RGBV(0x4D), RGBV(0x4E), RGBV(0x4F),
  RGBV(0x50), RGBV(0x51), RGBV(0x52), RGBV(0x53),
  RGBV(0x54), RGBV(0x55), RGBV(0x56), RGBV(0x57),
  RGBV(0x58), RGBV(0x59), RGBV(0x5A), RGBV(0x5B),
  RGBV(0x5C), RGBV(0x5D), RGBV(0x5E), RGBV(0x5F),
  RGBV(0x60), RGBV(0x61), RGBV(0x62), RGBV(0x63),
  RGBV(0x64), RGBV(0x65), RGBV(0x66), RGBV(0x67),
  RGBV(0x68), RGBV(0x69), RGBV(0x6A), RGBV(0x6B),
  RGBV(0x6C), RGBV(0x6D), RGBV(0x6E), RGBV(0x6F),
  RGBV(0x70), RGBV(0x71), RGBV(0x72), RGBV(0x73),
  RGBV(0x74), RGBV(0x75), RGBV(0x76), RGBV(0x77),
  RGBV(0x78), RGBV(0x79), RGBV(0x7A), RGBV(0x7B),
  RGBV(0x7C), RGBV(0x7D), RGBV(0x7E), RGBV(0x7F),
  RGBV(0x80), RGBV(0x81), RGBV(0x82), RGBV(0x83),
  RGBV(0x84), RGBV(0x85), RGBV(0x86), RGBV(0x87),
  RGBV(0x88), RGBV(0x89), RGBV(0x8A), RGBV(0x8B),
  RGBV(0x8C), RGBV(0x8D), RGBV(0x8E), RGBV(0x8F),
  RGBV(0x90), RGBV(0x91), RGBV(0x92), RGBV(0x93),
  RGBV(0x94), RGBV(0x95), RGBV(0x96), RGBV(0x97),
  RGBV(0x98), RGBV(0x99), RGBV(0x9A), RGBV(0x9B),
  RGBV(0x9C), RGBV(0x9D), RGBV(0x9E), RGBV(0x9F),
  RGBV(0xA0), RGBV(0xA1), RGBV(0xA2), RGBV(0xA3),
  RGBV(0xA4), RGBV(0xA5), RGBV(0xA6), RGBV(0xA7),
  RGBV(0xA8), RGBV(0xA9), RGBV(0xAA), RGBV(0xAB),
  RGBV(0xAC), RGBV(0xAD), RGBV(0xAE), RGBV(0xAF),
  RGBV(0xB0), RGBV(0xB1), RGBV(0xB2), RGBV(0xB3),
  RGBV(0xB4), RGBV(0xB5), RGBV(0xB6), RGBV(0xB7),
  RGBV(0xB8), RGBV(0xB9), RGBV(0xBA), RGBV(0xBB),
  RGBV(0xBC), RGBV(0xBD), RGBV(0xBE), RGBV(0xBF),
  RGBV(0xC0), RGBV(0xC1), RGBV(0xC2), RGBV(0xC3),
  RGBV(0xC4), RGBV(0xC5), RGBV(0xC6), RGBV(0xC7),
  RGBV(0xC8), RGBV(0xC9), RGBV(0xCA), RGBV(0xCB),
  RGBV(0xCC), RGBV(0xCD), RGBV(0xCE), RGBV(0xCF),
  RGBV(0xD0), RGBV(0xD1), RGBV(0xD2), RGBV(0xD3),
  RGBV(0xD4), RGBV(0xD5), RGBV(0xD6), RGBV(0xD7),
  RGBV(0xD8), RGBV(0xD9), RGBV(0xDA), RGBV(0xDB),
  RGBV(0xDC), RGBV(0xDD), RGBV(0xDE), RGBV(0xDF),
  RGBV(0xE0), RGBV(0xE1), RGBV(0xE2), RGBV(0xE3),
  RGBV(0xE4), RGBV(0xE5), RGBV(0xE6), RGBV(0xE7),
  RGBV(0xE8), RGBV(0xE9), RGBV(0xEA), RGBV(0xEB),
  RGBV(0xEC), RGBV(0xED), RGBV(0xEE), RGBV(0xEF),
  RGBV(0xF0), RGBV(0xF1), RGBV(0xF2), RGBV(0xF3),
  RGBV(0xF4), RGBV(0xF5), RGBV(0xF6), RGBV(0xF7),
  RGBV(0xF8), RGBV(0xF9), RGBV(0xFA), RGBV(0xFB),
  RGBV(0xFC), RGBV(0xFD), RGBV(0xFE), RGBV(0xFF),
};
}  // extern "C"

#undef RGBHY
#undef RGBY
#undef RGBU
#undef RGBV
#undef MMX_ALIGNED

// Warning C4799: function has no EMMS instruction.
// EMMS() is slow and should be called by the calling function once per image.
#pragma warning(disable: 4799)

__declspec(naked)
void FastConvertYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    jmp       wend

 wloop :
    movzx     eax, byte ptr [edi]
    add       edi, 1
    movzx     ebx, byte ptr [esi]
    add       esi, 1
    movq      mm0, [coefficients_RGB_U + 8 * eax]
    movzx     eax, byte ptr [edx]
    paddsw    mm0, [coefficients_RGB_V + 8 * ebx]
    movzx     ebx, byte ptr [edx + 1]
    movq      mm1, [coefficients_RGB_Y + 8 * eax]
    add       edx, 2
    movq      mm2, [coefficients_RGB_Y + 8 * ebx]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 wend :
    sub       ecx, 2
    jns       wloop

    and       ecx, 1  // odd number of pixels?
    jz        wdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [coefficients_RGB_U + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [coefficients_RGB_V + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [coefficients_RGB_Y + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1
 wdone :

    popad
    ret
  }
}

__declspec(naked)
void ConvertYUVToRGB32Row(const uint8* y_buf,
                          const uint8* u_buf,
                          const uint8* v_buf,
                          uint8* rgb_buf,
                          int width,
                          int step) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    mov       ebx, [esp + 32 + 24]  // step
    jmp       wend

 wloop :
    movzx     eax, byte ptr [edi]
    add       edi, ebx
    movq      mm0, [coefficients_RGB_U + 8 * eax]
    movzx     eax, byte ptr [esi]
    add       esi, ebx
    paddsw    mm0, [coefficients_RGB_V + 8 * eax]
    movzx     eax, byte ptr [edx]
    add       edx, ebx
    movq      mm1, [coefficients_RGB_Y + 8 * eax]
    movzx     eax, byte ptr [edx]
    add       edx, ebx
    movq      mm2, [coefficients_RGB_Y + 8 * eax]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 wend :
    sub       ecx, 2
    jns       wloop

    and       ecx, 1  // odd number of pixels?
    jz        wdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [coefficients_RGB_U + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [coefficients_RGB_V + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [coefficients_RGB_Y + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1
 wdone :

    popad
    ret
  }
}

__declspec(naked)
void RotateConvertYUVToRGB32Row(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* rgb_buf,
                                int width,
                                int ystep,
                                int uvstep) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    jmp       wend

 wloop :
    movzx     eax, byte ptr [edi]
    mov       ebx, [esp + 32 + 28]  // uvstep
    add       edi, ebx
    movq      mm0, [coefficients_RGB_U + 8 * eax]
    movzx     eax, byte ptr [esi]
    add       esi, ebx
    paddsw    mm0, [coefficients_RGB_V + 8 * eax]
    movzx     eax, byte ptr [edx]
    mov       ebx, [esp + 32 + 24]  // ystep
    add       edx, ebx
    movq      mm1, [coefficients_RGB_Y + 8 * eax]
    movzx     eax, byte ptr [edx]
    add       edx, ebx
    movq      mm2, [coefficients_RGB_Y + 8 * eax]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 wend :
    sub       ecx, 2
    jns       wloop

    and       ecx, 1  // odd number of pixels?
    jz        wdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [coefficients_RGB_U + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [coefficients_RGB_V + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [coefficients_RGB_Y + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1
 wdone :

    popad
    ret
  }
}

__declspec(naked)
void DoubleYUVToRGB32Row(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* rgb_buf,
                         int width) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    jmp       wend

 wloop :
    movzx     eax, byte ptr [edi]
    add       edi, 1
    movzx     ebx, byte ptr [esi]
    add       esi, 1
    movq      mm0, [coefficients_RGB_U + 8 * eax]
    movzx     eax, byte ptr [edx]
    paddsw    mm0, [coefficients_RGB_V + 8 * ebx]
    movq      mm1, [coefficients_RGB_Y + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    punpckldq mm1, mm1
    movntq    [ebp], mm1

    movzx     ebx, byte ptr [edx + 1]
    add       edx, 2
    paddsw    mm0, [coefficients_RGB_Y + 8 * ebx]
    psraw     mm0, 6
    packuswb  mm0, mm0
    punpckldq mm0, mm0
    movntq    [ebp+8], mm0
    add       ebp, 16
 wend :
    sub       ecx, 4
    jns       wloop

    add       ecx, 4
    jz        wdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [coefficients_RGB_U + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [coefficients_RGB_V + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [coefficients_RGB_Y + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    jmp       wend1

 wloop1 :
    movd      [ebp], mm1
    add       ebp, 4
 wend1 :
    sub       ecx, 1
    jns       wloop1
 wdone :
    popad
    ret
  }
}

// This version does general purpose scaling by any amount, up or down.
// The only thing it can not do it rotation by 90 or 270.
// For performance the chroma is under sampled, reducing cost of a 3x
// 1080p scale from 8.4 ms to 5.4 ms.
__declspec(naked)
void ScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int dx) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    xor       ebx, ebx              // x
    jmp       wend

 wloop :
    mov       eax, ebx
    sar       eax, 5
    movzx     eax, byte ptr [edi + eax]
    movq      mm0, [coefficients_RGB_U + 8 * eax]
    mov       eax, ebx
    sar       eax, 5
    movzx     eax, byte ptr [esi + eax]
    paddsw    mm0, [coefficients_RGB_V + 8 * eax]
    mov       eax, ebx
    add       ebx, [esp + 32 + 24]  // x += dx
    sar       eax, 4
    movzx     eax, byte ptr [edx + eax]
    movq      mm1, [coefficients_RGB_Y + 8 * eax]
    mov       eax, ebx
    add       ebx, [esp + 32 + 24]  // x += dx
    sar       eax, 4
    movzx     eax, byte ptr [edx + eax]
    movq      mm2, [coefficients_RGB_Y + 8 * eax]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 wend :
    sub       ecx, 2
    jns       wloop

    and       ecx, 1  // odd number of pixels?
    jz        wdone

    mov       eax, ebx
    sar       eax, 5
    movzx     eax, byte ptr [edi + eax]
    movq      mm0, [coefficients_RGB_U + 8 * eax]
    mov       eax, ebx
    sar       eax, 5
    movzx     eax, byte ptr [esi + eax]
    paddsw    mm0, [coefficients_RGB_V + 8 * eax]
    mov       eax, ebx
    sar       eax, 4
    movzx     eax, byte ptr [edx + eax]
    movq      mm1, [coefficients_RGB_Y + 8 * eax]
    mov       eax, ebx
    sar       eax, 4
    movzx     eax, byte ptr [edx + eax]
    movq      mm2, [coefficients_RGB_Y + 8 * eax]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movd      [ebp], mm1

 wdone :

    popad
    ret
  }
}

}  // namespace media

