// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_resources.h"

#if PLATFORM(WIN)
#include "base/gfx/gdi_util.h"
#endif

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
    : type_(ARROW),
      hotspot_x_(0),
      hotspot_y_(0) {
  if (bitmap) {
    type_ = CUSTOM;
    hotspot_x_ = hotspot_x;
    hotspot_y_ = hotspot_y;
    bitmap_ = *bitmap;
  } else {
    memset(&bitmap_, 0, sizeof(bitmap_));
  }
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
#if PLATFORM(WIN)
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
#endif
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

