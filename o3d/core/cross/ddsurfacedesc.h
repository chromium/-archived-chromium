/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the structure definintions necessary for loading a DDS
// image file (DirectDrawSurface). Using ths definition will prevent apps
// from having to including "ddraw.h" from the DirectX SDK.

#ifndef O3D_CORE_CROSS_GL_DDSURFACEDESC_H_
#define O3D_CORE_CROSS_GL_DDSURFACEDESC_H_

#include <build/build_config.h>

#ifndef OS_WIN
typedef uint16 WORD;
typedef uint32 DWORD;
typedef int8 BYTE;
typedef int32 LONG;
typedef void* LPVOID;
#endif

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                        \
  ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |           \
  ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#define FOURCC_DXT1  (MAKEFOURCC('D', 'X', 'T', '1'))
#define FOURCC_DXT2  (MAKEFOURCC('D', 'X', 'T', '2'))
#define FOURCC_DXT3  (MAKEFOURCC('D', 'X', 'T', '3'))
#define FOURCC_DXT4  (MAKEFOURCC('D', 'X', 'T', '4'))
#define FOURCC_DXT5  (MAKEFOURCC('D', 'X', 'T', '5'))

/*
 * The surface will accept pixel data in the format specified
 * and compress it during the write.
 */
#define DDPF_ALPHAPIXELS                        0x00000001l
#define DDPF_FOURCC                             0x00000004L
#define DDPF_RGB                                0x00000040L
#define DDPF_COMPRESSED                         0x00000080L
#define DDSCAPS_COMPLEX                         0x00000008L
#define DDSCAPS_MIPMAP                          0x00400000L
#define DDSCAPS_TEXTURE                         0x00001000L
#define DDSCAPS2_CUBEMAP                        0x00000200L

/*
 * These flags preform two functions:
 * - At CreateSurface time, they define which of the six cube faces are
 *   required by the application.
 * - After creation, each face in the cubemap will have exactly one of these
 *   bits set.
 */
#define DDSCAPS2_CUBEMAP_POSITIVEX              0x00000400L
#define DDSCAPS2_CUBEMAP_NEGATIVEX              0x00000800L
#define DDSCAPS2_CUBEMAP_POSITIVEY              0x00001000L
#define DDSCAPS2_CUBEMAP_NEGATIVEY              0x00002000L
#define DDSCAPS2_CUBEMAP_POSITIVEZ              0x00004000L
#define DDSCAPS2_CUBEMAP_NEGATIVEZ              0x00008000L

/*
 * This macro may be used to specify all faces of a cube map at
 * CreateSurface time.
 */
#define DDSCAPS2_CUBEMAP_ALLFACES (DDSCAPS2_CUBEMAP_POSITIVEX |  \
                                   DDSCAPS2_CUBEMAP_NEGATIVEX |  \
                                   DDSCAPS2_CUBEMAP_POSITIVEY |  \
                                   DDSCAPS2_CUBEMAP_NEGATIVEY |  \
                                   DDSCAPS2_CUBEMAP_POSITIVEZ |  \
                                   DDSCAPS2_CUBEMAP_NEGATIVEZ)

namespace o3d {

enum DDSD_FLAGS {
  DDSD_CAPS = 0x00000001l,
  DDSD_HEIGHT = 0x00000002l,
  DDSD_WIDTH = 0x00000004l,
  DDSD_PITCH = 0x00000008l,
  DDSD_BACKBUFFERCOUNT = 0x00000020l,
  DDSD_ZBUFFERBITDEPTH = 0x00000040l,
  DDSD_ALPHABITDEPTH = 0x00000080l,
  DDSD_LPSURFACE = 0x00000800l,
  DDSD_PIXELFORMAT = 0x00001000l,
  DDSD_CKDESTOVERLAY = 0x00002000l,
  DDSD_CKDESTBLT = 0x00004000l,
  DDSD_CKSRCOVERLAY = 0x00008000l,
  DDSD_CKSRCBLT = 0x00010000l,
  DDSD_MIPMAPCOUNT = 0x00020000l,
  DDSD_REFRESHRATE = 0x00040000l,
  DDSD_LINEARSIZE = 0x00080000l,
  DDSD_TEXTURESTAGE = 0x00100000l,
  DDSD_FVF = 0x00200000l,
  DDSD_SRCVBHANDLE = 0x00400000l,
  DDSD_DEPTH = 0x00800000l,
};

struct DDCOLORKEY {
  DWORD dwColorSpaceLowValue;   // low boundary of Color Key, inclusive
  DWORD dwColorSpaceHighValue;  // high boundary of Color Key, inclusive
};

struct DDPIXELFORMAT {
  DWORD dwSize;     // size of structure
  DWORD dwFlags;    // pixel format flags
  DWORD dwFourCC;   // (FOURCC code)
  union {
    DWORD dwRGBBitCount;            // how many bits per pixel
    DWORD dwYUVBitCount;            // how many bits per pixel
    DWORD dwZBufferBitDepth;        // how many total bits/pixel in z buffer
                                    // (including any stencil bits)
    DWORD dwAlphaBitDepth;          // how many bits for alpha channels
    DWORD dwLuminanceBitCount;      // how many bits per pixel
    DWORD dwBumpBitCount;           // how many bits per "buxel", total
    DWORD dwPrivateFormatBitCount;  // Bits per pixel of private driver formats.
                                    // Only valid in texture format list and if
                                    // DDPF_D3DFORMAT is set.
  };
  union {
    DWORD dwRBitMask;          // mask for red bit
    DWORD dwYBitMask;          // mask for Y bits
    DWORD dwStencilBitDepth;   // how many stencil bits
                               // (note:dwZBufferBitDepth-dwStencilBitDepth is
                               // total Z-only bits)
    DWORD dwLuminanceBitMask;  // mask for luminance bits
    DWORD dwBumpDuBitMask;     // mask for bump map U delta bits
    DWORD dwOperations;        // DDPF_D3DFORMAT Operations
  };
  union {
    DWORD dwGBitMask;          // mask for green bits
    DWORD dwUBitMask;          // mask for U bits
    DWORD dwZBitMask;          // mask for Z bits
    DWORD dwBumpDvBitMask;     // mask for bump map V delta bits
    struct {
      WORD    wFlipMSTypes;    // Multisample methods supported via flip for
                               // this D3DFORMAT
      WORD    wBltMSTypes;     // Multisample methods supported via blt for
                               // this D3DFORMAT
    } MultiSampleCaps;
  };
  union {
    DWORD dwBBitMask;              // mask for blue bits
    DWORD dwVBitMask;              // mask for V bits
    DWORD dwStencilBitMask;        // mask for stencil bits
    DWORD dwBumpLuminanceBitMask;  // mask for luminance in bump map
  };
  union {
    DWORD dwRGBAlphaBitMask;        // mask for alpha channel
    DWORD dwYUVAlphaBitMask;        // mask for alpha channel
    DWORD dwLuminanceAlphaBitMask;  // mask for alpha channel
    DWORD dwRGBZBitMask;            // mask for Z channel
    DWORD dwYUVZBitMask;            // mask for Z channel
  };
};

struct DDSCAPS2 {
  DWORD dwCaps;  // capabilities of surface wanted
  DWORD dwCaps2;
  DWORD dwCaps3;
  union {
    DWORD dwCaps4;
    DWORD dwVolumeDepth;
  };
};

struct DDSURFACEDESC2 {
  DWORD dwSize;    // size of the DDSURFACEDESC structure
  DWORD dwFlags;   // determines what fields are valid
  DWORD dwHeight;  // height of surface to be created
  DWORD dwWidth;   // width of input surface
  union {
    LONG lPitch;         // distance to start of next line (return value only)
    DWORD dwLinearSize;  // Formless late-allocated optimized surface size
  };
  union {
    DWORD dwBackBufferCount;  // number of back buffers requested
    DWORD dwDepth;            // the depth if this is a volume texture
  };
  union {
    DWORD dwMipMapCount;  // number of mip-map levels requested
                          // dwZBufferBitDepth removed, use ddpfPixelFormat
                          // one instead
    DWORD dwRefreshRate;  // refresh rate (used when display mode is described)
    DWORD dwSrcVBHandle;  // The source used in VB::Optimize
  };
  DWORD dwAlphaBitDepth;  // depth of alpha buffer requested
  DWORD dwReserved;       // reserved
  LPVOID lpSurface;       // pointer to the associated surface memory
  union {
    DDCOLORKEY ddckCKDestOverlay;  // color key for destination overlay
    DWORD dwEmptyFaceColor;        // color for empty cubemap faces
  };
  DDCOLORKEY ddckCKDestBlt;     // color key for destination blt use
  DDCOLORKEY ddckCKSrcOverlay;  // color key for source overlay use
  DDCOLORKEY ddckCKSrcBlt;      // color key for source blt use
  union {
    DDPIXELFORMAT ddpfPixelFormat;  // format of the surface
    DWORD dwFVF;                    // format of vertex buffers
  };
  DDSCAPS2 ddsCaps;      // direct draw surface capabilities
  DWORD dwTextureStage;  // stage in multitexture cascade
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_GL_DDSURFACEDESC_H_
