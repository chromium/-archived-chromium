// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ICON_LOADER_H_
#define CHROME_BROWSER_ICON_LOADER_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"

class SkBitmap;

////////////////////////////////////////////////////////////////////////////////
//
// A facility to read a file containing an icon asynchronously in the IO
// thread. Returns the icon in the form of an SkBitmap.
//
////////////////////////////////////////////////////////////////////////////////
class IconLoader : public base::RefCountedThreadSafe<IconLoader> {
 public:
  enum IconSize {
    SMALL = 0,  // 16x16
    NORMAL,     // 32x32
    LARGE
  };

  class Delegate {
   public:
    // Invoked when an icon has been read. |source| is the IconLoader. If the
    // icon has been successfully loaded, result is non-null. This method must
    // return true if it is taking ownership of the returned bitmap.
    virtual bool OnBitmapLoaded(IconLoader* source, SkBitmap* result) = 0;
  };

  IconLoader() { }

  virtual ~IconLoader() { }

  // Start reading the icon on the file thread.
  virtual void Start() = 0;

  // Factory method for returning a platform specific IconLoad.
  static IconLoader* Create(const FilePath& path, IconSize size,
                            Delegate* delegate);

 private:
  DISALLOW_COPY_AND_ASSIGN(IconLoader);
};

#endif  // CHROME_BROWSER_ICON_LOADER_H_
