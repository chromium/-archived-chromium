// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_BITMAP_HEADER_H__
#define BASE_GFX_BITMAP_HEADER_H__

#include <windows.h>

namespace gfx {

// Creates a BITMAPINFOHEADER structure given the bitmap's size.
void CreateBitmapHeader(int width, int height, BITMAPINFOHEADER* hdr);

// Creates a BITMAPINFOHEADER structure given the bitmap's size and
// color depth in bits per pixel.
void CreateBitmapHeaderWithColorDepth(int width, int height, int color_depth,
                                      BITMAPINFOHEADER* hdr);

// Creates a BITMAPV4HEADER structure given the bitmap's size.  You probably
// only need to use BMP V4 if you need transparency (alpha channel). This
// function sets the AlphaMask to 0xff000000.
void CreateBitmapV4Header(int width, int height, BITMAPV4HEADER* hdr);

// Creates a monochrome bitmap header.
void CreateMonochromeBitmapHeader(int width, int height, BITMAPINFOHEADER* hdr);


}  // namespace gfx

#endif // BASE_GFX_BITMAP_HEADER_H__

