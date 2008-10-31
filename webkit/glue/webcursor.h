// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBCURSOR_H__
#define WEBCURSOR_H__

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <CoreGraphics/CoreGraphics.h>
#else
#include "skia/include/SkBitmap.h"
#endif

// Use CGImage on the Mac and SkBitmap on other platforms.  WebCursorBitmapPtr
// is a corresponding pointer type: because CGImageRef is already a pointer
// type, we can just use that directly in the constructor, but we need an
// SkBitmap* in the Skia case.
//
// TODO(port): We should use a bitmap abstraction container.
#if defined(OS_MACOSX)
typedef CGImageRef WebCursorBitmap;
typedef WebCursorBitmap WebCursorBitmapPtr;
#else
typedef SkBitmap WebCursorBitmap;
typedef SkBitmap* WebCursorBitmapPtr;
#endif

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
  WebCursor(const WebCursorBitmapPtr bitmap, int hotspot_x, int hotspot_y);
  ~WebCursor();

  WebCursor(const WebCursor& other);
  WebCursor& operator = (const WebCursor&);

  Type type() const { return type_; };
  int hotspot_x() const { return hotspot_x_; }
  int hotspot_y() const { return hotspot_y_; }
  const WebCursorBitmap& bitmap() const { return bitmap_; }

  void set_type(Type cursor_type) {
    type_ = cursor_type;
  }

  void set_bitmap(const WebCursorBitmap& bitmap) {
#if defined(OS_MACOSX)
    WebCursorBitmap old_bitmap = bitmap_;
    CGImageRetain(bitmap_ = bitmap);
    CGImageRelease(old_bitmap);
#else
    bitmap_ = bitmap;
#endif
  }

  void set_hotspot(int hotspot_x, int hotspot_y) {
    hotspot_x_ = hotspot_x;
    hotspot_y_ = hotspot_y;
  }

#if defined(OS_WIN)
  // Returns the cursor handle. If the cursor type is a win32 or safari
  // cursor, we use LoadCursor to load the cursor. 
  // Returns NULL on error. 
  HCURSOR GetCursor(HINSTANCE module_handle) const;
  // If the underlying cursor type is a custom cursor, this function converts
  // the WebCursorBitmap to a cursor and returns the same. The responsiblity of
  // freeing the cursor handle lies with the caller.
  // Returns NULL on error.
  HCURSOR GetCustomCursor() const;
#endif

// TODO(port): Comparing CGImageRefs can be a heavyweight operation on the
// Mac.  Don't do it if it's not needed.  Maybe we can avoid this type of
// comparison on other platforms too.
#if !defined(OS_MACOSX)
  // Returns true if the passed in WebCursorBitmap is the same as the the 
  // current WebCursorBitmap. We use memcmp to compare the two bitmaps.
  bool IsSameBitmap(const WebCursorBitmap& bitmap) const;

  // Returns true if the current cursor object contains the same
  // cursor as the cursor object passed in. If the current cursor
  // is a custom cursor, we also compare the bitmaps to verify
  // whether they are equal.
  bool IsEqual(const WebCursor& other) const;
#endif

protected:
  Type type_;
  int hotspot_x_;
  int hotspot_y_;
  WebCursorBitmap bitmap_;
};

#endif  // WEBCURSOR_H__
