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
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_resources.h"

WebCursor::WebCursor()
  : type_(ARROW),
    hotspot_x_(0),
    hotspot_y_(0) {
  memset(&bitmap_, 0, sizeof(bitmap_));
}

WebCursor::WebCursor(Type cursor_type)
  : type_(cursor_type),
    hotspot_x_(0),
    hotspot_y_(0) {
  memset(&bitmap_, 0, sizeof(bitmap_));
}

WebCursor::WebCursor(const SkBitmap* bitmap, int hotspot_x, int hotspot_y)
  : type_(CUSTOM) {
  hotspot_x_ = hotspot_x;
  hotspot_y_ = hotspot_y;
  bitmap_ = *bitmap;
}

WebCursor::~WebCursor() {
}

WebCursor::WebCursor(const WebCursor& other) {
  type_ = other.type_;
  hotspot_x_ = other.hotspot_x_;
  hotspot_y_ = other.hotspot_y_;
  bitmap_ = other.bitmap_;
}

WebCursor& WebCursor::operator=(const WebCursor& other) {
  if (this != &other) {
    type_ = other.type_;
    hotspot_x_ = other.hotspot_x_;
    hotspot_y_ = other.hotspot_y_;
    bitmap_ = other.bitmap_;
  }
  return *this;
}

HCURSOR WebCursor::GetCursor(HINSTANCE module_handle) const {
  if (type_ == CUSTOM) 
    return NULL;

  static LPCWSTR cursor_resources[] = {
    IDC_ARROW,
    IDC_IBEAM,
    IDC_WAIT,
    IDC_CROSS,
    IDC_UPARROW,
    IDC_SIZE,
    IDC_ICON,
    IDC_SIZENWSE,
    IDC_SIZENESW,
    IDC_SIZEWE,
    IDC_SIZENS,
    IDC_SIZEALL,
    IDC_NO,
    IDC_HAND,
    IDC_APPSTARTING,
    IDC_HELP,
    // webkit resources
    MAKEINTRESOURCE(IDC_ALIAS),
    MAKEINTRESOURCE(IDC_CELL),
    MAKEINTRESOURCE(IDC_COLRESIZE),
    MAKEINTRESOURCE(IDC_COPYCUR),
    MAKEINTRESOURCE(IDC_ROWRESIZE),
    MAKEINTRESOURCE(IDC_VERTICALTEXT),
    MAKEINTRESOURCE(IDC_ZOOMIN),
    MAKEINTRESOURCE(IDC_ZOOMOUT)
  };

  HINSTANCE instance_to_use = NULL;
  if (type_ > HELP)
    instance_to_use = module_handle;

  HCURSOR cursor_handle = LoadCursor(instance_to_use, 
                                     cursor_resources[type_]);
  return cursor_handle;
}

HCURSOR WebCursor::GetCustomCursor() const {
  if (type_ != CUSTOM)
    return NULL;

  BITMAPINFO cursor_bitmap_info = {0};
  gfx::CreateBitmapHeader(bitmap_.width(), bitmap_.height(), 
                          reinterpret_cast<BITMAPINFOHEADER*>(&cursor_bitmap_info));
  HDC dc = ::GetDC(0);
  HDC workingDC = CreateCompatibleDC(dc);
  HBITMAP bitmap_handle = CreateDIBSection(dc, &cursor_bitmap_info, 
                                           DIB_RGB_COLORS, 0, 0, 0);
  SkAutoLockPixels bitmap_lock(bitmap_); 
  SetDIBits(0, bitmap_handle, 0, bitmap_.width(), 
            bitmap_.getPixels(), &cursor_bitmap_info, DIB_RGB_COLORS);

  HBITMAP old_bitmap = reinterpret_cast<HBITMAP>(SelectObject(workingDC, 
                                                              bitmap_handle));
  SetBkMode(workingDC, TRANSPARENT);
  SelectObject(workingDC, old_bitmap);

  HBITMAP mask = CreateBitmap(bitmap_.width(), bitmap_.height(),
                              1, 1, NULL);
  ICONINFO ii = {0};
  ii.fIcon = FALSE;
  ii.xHotspot = hotspot_x_;
  ii.yHotspot = hotspot_y_;
  ii.hbmMask = mask;
  ii.hbmColor = bitmap_handle;

  HCURSOR cursor_handle = CreateIconIndirect(&ii);

  DeleteObject(mask); 
  DeleteObject(bitmap_handle); 
  DeleteDC(workingDC);
  ::ReleaseDC(0, dc);
  return cursor_handle;
}

bool WebCursor::IsSameBitmap(const SkBitmap& bitmap) const {
  SkAutoLockPixels new_bitmap_lock(bitmap);
  SkAutoLockPixels bitmap_lock(bitmap_); 
  return (memcmp(bitmap_.getPixels(), bitmap.getPixels(), 
                 bitmap_.getSize()) == 0);
}

bool WebCursor::IsEqual(const WebCursor& other) const {
  if (type_ != other.type_)
    return false;

  if(type_ == CUSTOM)
    return IsSameBitmap(other.bitmap_);
  return true;
}
