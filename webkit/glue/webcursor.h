// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBCURSOR_H_
#define WEBKIT_GLUE_WEBCURSOR_H_

#include "base/basictypes.h"
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
#elif defined(OS_MACOSX)
#ifdef __OBJC__
@class NSCursor;
#else
class NSCursor;
#endif
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
  ~WebCursor();

  // Copy constructor/assignment operator combine.
  WebCursor(const WebCursor& other);
  const WebCursor& operator=(const WebCursor& other);

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
  // Returns a HCURSOR representing the current WebCursor instance.
  // The ownership of the HCURSOR (does not apply to external cursors) remains
  // with the WebCursor instance.
  HCURSOR GetCursor(HINSTANCE module_handle);

  // Initialize this from the given Windows cursor. The caller must ensure that
  // the HCURSOR remains valid by not invoking the DestroyCursor/DestroyIcon
  // APIs on it.
  void InitFromExternalCursor(HCURSOR handle);

#elif defined(OS_LINUX)
  // Return the stock GdkCursorType for this cursor, or GDK_CURSOR_IS_PIXMAP
  // if it's a custom cursor. Return GDK_LAST_CURSOR to indicate that the cursor
  // should be set to the system default.
  GdkCursorType GetCursorType() const;

  // Return a new GdkCursor* for this cursor.  Only valid if GetCursorType
  // returns GDK_CURSOR_IS_PIXMAP.
  GdkCursor* GetCustomCursor() const;
#elif defined(OS_MACOSX)
  NSCursor* GetCursor() const;
#endif

 private:
  // Copies the contents of the WebCursor instance passed in.
  void Copy(const WebCursor& other);

  // Cleans up the WebCursor instance.
  void Clear();

  // Platform specific initialization goes here.
  void InitPlatformData();

  // Platform specific Serialization / De-serialization
  bool SerializePlatformData(Pickle* pickle) const;
  bool DeserializePlatformData(const Pickle* pickle, void** iter);

  // Returns true if the platform data in the current cursor object
  // matches that of the cursor passed in.
  bool IsPlatformDataEqual(const WebCursor& other) const ;

  // Copies platform specific data from the WebCursor instance passed in.
  void CopyPlatformData(const WebCursor& other);

  // Platform specific cleanup.
  void CleanupPlatformData();

  void SetCustomData(WebCore::Image* image);

  // WebCore::PlatformCursor type.
  int type_;

  gfx::Point hotspot_;

  // Custom cursor data, as 32-bit RGBA.
  // Platform-inspecific because it can be serialized.
  gfx::Size custom_size_;
  std::vector<char> custom_data_;

#if defined(OS_WIN)
  // An externally generated HCURSOR. We assume that it remains valid, i.e we
  // don't attempt to copy the HCURSOR.
  HCURSOR external_cursor_;
  // A custom cursor created from custom bitmap data by Webkit.
  HCURSOR custom_cursor_;
#endif  // OS_WIN
};

#endif  // WEBKIT_GLUE_WEBCURSOR_H_
