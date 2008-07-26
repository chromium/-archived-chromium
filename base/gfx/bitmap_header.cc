// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/gfx/bitmap_header.h"

namespace gfx {

void CreateBitmapHeader(int width, int height, BITMAPINFOHEADER* hdr) {
  CreateBitmapHeaderWithColorDepth(width, height, 32, hdr);
}

void CreateBitmapHeaderWithColorDepth(int width, int height, int color_depth,
                                      BITMAPINFOHEADER* hdr) {
  // These values are shared with gfx::PlatformDevice
  hdr->biSize = sizeof(BITMAPINFOHEADER);
  hdr->biWidth = width;
  hdr->biHeight = -height;  // minus means top-down bitmap
  hdr->biPlanes = 1;
  hdr->biBitCount = color_depth;
  hdr->biCompression = BI_RGB;  // no compression
  hdr->biSizeImage = 0;
  hdr->biXPelsPerMeter = 1;
  hdr->biYPelsPerMeter = 1;
  hdr->biClrUsed = 0;
  hdr->biClrImportant = 0;
}


void CreateBitmapV4Header(int width, int height, BITMAPV4HEADER* hdr) {
  // Because bmp v4 header is just an extension, we just create a v3 header and
  // copy the bits over to the v4 header.
  BITMAPINFOHEADER header_v3;
  CreateBitmapHeader(width, height, &header_v3);
  memset(hdr, 0, sizeof(BITMAPV4HEADER));
  memcpy(hdr, &header_v3, sizeof(BITMAPINFOHEADER));

  // Correct the size of the header and fill in the mask values.
  hdr->bV4Size = sizeof(BITMAPV4HEADER);
  hdr->bV4RedMask   = 0x00ff0000;
  hdr->bV4GreenMask = 0x0000ff00;
  hdr->bV4BlueMask  = 0x000000ff;
  hdr->bV4AlphaMask = 0xff000000;
}

// Creates a monochrome bitmap header.
void CreateMonochromeBitmapHeader(int width,
                                  int height,
                                  BITMAPINFOHEADER* hdr) {
  hdr->biSize = sizeof(BITMAPINFOHEADER);
  hdr->biWidth = width;
  hdr->biHeight = -height;
  hdr->biPlanes = 1;
  hdr->biBitCount = 1;
  hdr->biCompression = BI_RGB;
  hdr->biSizeImage = 0;
  hdr->biXPelsPerMeter = 1;
  hdr->biYPelsPerMeter = 1;
  hdr->biClrUsed = 0;
  hdr->biClrImportant = 0;
}

}  // namespace gfx
