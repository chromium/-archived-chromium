// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBCURSOR_H_
#define WEBKIT_GLUE_WEBCURSOR_H_

#include "base/gfx/point.h"
#include "base/gfx/size.h"

#include <vector>

#if defined(OS_WIN)
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HICON__* HICON;
typedef HICON HCURSOR;
#elif defined(OS_LINUX)
// GdkCursorType is an enum, which we can't forward-declare.  :(
#include <gdk/gdkcursor.h>
#endif

class Pickle;

namespace WebCore {
class Image;
class PlatformCursor;
}

// This class encapsulates a cross-platform description of a cursor.  Platform
// specific methods are provided to translate the cross-platform cursor into a
// platform specific cursor.  It is also possible to serialize / de-serialize a
// WebCursor.
class WebCursor {
 public:
  WebCursor();
  explicit WebCursor(const WebCore::PlatformCursor& platform_cursor);

  // Serialization / De-serialization
  bool Deserialize(const Pickle* pickle, void** iter);
  bool Serialize(Pickle* pickle) const;

  // Returns true if GetCustomCursor should be used to allocate a platform
  // specific cursor object.  Otherwise GetCursor should be used.
  bool IsCustom() const;

  // Returns true if the current cursor object contains the same cursor as the
  // cursor object passed in. If the current cursor is a custom cursor, we also
  // compare the bitmaps to verify whether they are equal.
  bool IsEqual(const WebCursor& other) const;

#if defined(OS_WIN)
  // If the underlying cursor type is not a custom cursor, this functions uses
  // the LoadCursor API to load the cursor and returns it.  The caller SHOULD
  // NOT pass the resulting handling to DestroyCursor.  Returns NULL on error.
  HCURSOR GetCursor(HINSTANCE module_handle) const;

  // If the underlying cursor type is a custom cursor, this function generates
  // a cursor and returns it.  The responsiblity of freeing the cursor handle
  // lies with the caller.  Returns NULL on error.
  HCURSOR GetCustomCursor() const;

  // Initialize this from the given Windows cursor.
  void InitFromCursor(HCURSOR handle);
#elif defined(OS_LINUX)
  // Return the stock GdkCursorType for this cursor, or GDK_CURSOR_IS_PIXMAP
  // if it's a custom cursor.
  GdkCursorType GetCursorType() const;
#endif

 private:
  void SetCustomData(WebCore::Image* image);

  int type_;
  gfx::Point hotspot_;
  gfx::Size custom_size_;
  std::vector<char> custom_data_;
};

#endif  // WEBKIT_GLUE_WEBCURSOR_H_
