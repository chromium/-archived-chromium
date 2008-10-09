// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBCURSOR_H__
#define WEBCURSOR_H__

#include "skia/include/SkBitmap.h"
#include "build/build_config.h"

// This class provides the functionality of a generic cursor type. The intent
// is to stay away from platform specific code here. We do have win32 specific
// functionality to retreive a HCURSOR from a cursor type. 
// TODO(iyengar) : Win32 specific functionality needs to be taken out of this 
// object
class WebCursor 
{
public:
  // Supported cursor types.
  enum Type {
    ARROW,
    IBEAM,
    WAIT,
    CROSS,
    UPARROW,
    SIZE,
    ICON,
    SIZENWSE,
    SIZENESW,
    SIZEWE,
    SIZENS,
    SIZEALL,
    NO,
    HAND,
    APPSTARTING,
    HELP,
    ALIAS,
    CELL,
    COLRESIZE,
    COPYCUR,
    ROWRESIZE,
    VERTICALTEXT,
    ZOOMIN,
    ZOOMOUT,
    CUSTOM
  };

  WebCursor();
  WebCursor(Type cursor_type);
  WebCursor(const SkBitmap* bitmap, int hotspot_x, int hotspot_y);
  ~WebCursor();

  WebCursor(const WebCursor& other);
  WebCursor& operator = (const WebCursor&);

  Type type() const { return type_; };
  int hotspot_x() const { return hotspot_x_; }
  int hotspot_y() const { return hotspot_y_; }
  const SkBitmap& bitmap() const { return bitmap_; }

  void set_type(Type cursor_type) {
    type_ = cursor_type;
  }

  void set_bitmap(const SkBitmap& bitmap) {
    bitmap_ = bitmap;
  }

  void set_hotspot(int hotspot_x, int hotspot_y) {
    hotspot_x_ = hotspot_x;
    hotspot_y_ = hotspot_x;
  }
#if defined(OS_WIN)
  // Returns the cursor handle. If the cursor type is a win32 or safari
  // cursor, we use LoadCursor to load the cursor. 
  // Returns NULL on error. 
  HCURSOR GetCursor(HINSTANCE module_handle) const;
  // If the underlying cursor type is a custom cursor, this function converts
  // the SkBitmap to a cursor and returns the same. The responsiblity of
  // freeing the cursor handle lies with the caller.
  // Returns NULL on error.
  HCURSOR GetCustomCursor() const;
#endif
  // Returns true if the passed in SkBitmap is the same as the the 
  // current SkBitmap. We use memcmp to compare the two bitmaps.
  bool IsSameBitmap(const SkBitmap& bitmap) const;

  // Returns true if the current cursor object contains the same
  // cursor as the cursor object passed in. If the current cursor
  // is a custom cursor, we also compare the bitmaps to verify
  // whether they are equal.
  bool IsEqual(const WebCursor& other) const;
  
protected:
  Type type_;
  int hotspot_x_;
  int hotspot_y_;
  SkBitmap bitmap_;
};

#endif  // WEBCURSOR_H__

