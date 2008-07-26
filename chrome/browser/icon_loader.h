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

#ifndef CHROME_BROWSER_ICON_LOADER_H__
#define CHROME_BROWSER_ICON_LOADER_H__

#include "base/logging.h"
#include <string>
#include <windows.h>

namespace {
class IconLoaderProcessor;
}

class SkBitmap;

////////////////////////////////////////////////////////////////////////////////
//
// A facility to read a file containing an icon asynchronously in the IO
// thread. Clients has the option to get the icon in the form of two HICON
// handles (one for a small icon size and one for a large icon) or in the form
// of an SkBitmap.
//
// This class currently only supports reading .ico files. Please extend as
// needed.
//
////////////////////////////////////////////////////////////////////////////////
class IconLoader {
 public:
  enum IconType {
    SK_BITMAP = 0,
    WINDOWS_HICON
  };

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
    //
    // This method is only called when GetIconType() above returns SK_BITMAP.
    virtual bool OnSkBitmapLoaded(IconLoader* source, SkBitmap* result) = 0;

    // Invoked when the small and the large HICONS have been read. |source| is
    // the IconLoader. If the small icon has been successfully loaded,
    // small_icon is non-null. The same applies to the large_icon. This method
    // must return true if it is taking ownership of the returned icon handles.
    //
    // This method is only called when GetIconType() above returns
    // WINDOWS_HICON.
    virtual bool OnHICONLoaded(IconLoader* source,
                               HICON small_icon,
                               HICON large_icon) = 0;
  };

  // Create a new IconLoader that loads the icon from the data at contained in
  // the file at |path|.  |icon_type| specifies which format to generate and
  // which method is invoked on the |delegate| once the icon was loaded.
  static IconLoader* CreateIconLoaderForFile(const std::wstring& path,
                                             IconType icon_type,
                                             Delegate* delegate);

  // Create a new IconLoader that loads the icon in the resource of the file
  // at |path|.  This is used with .exe/.dll files.
  // Note that this generates a SkBitmap (and consequently OnSkBitmapLoaded is
  // invoked on the delegate once the load has completed).
  static IconLoader* CreateIconLoaderForFileResource(const std::wstring& path,
                                                     IconSize size,
                                                     Delegate* delegate);

  ~IconLoader();

  // Start the read operation
  void Start();

  // Cancel the read operation. The delegate will no longer be contacted. Call
  // this method if you need to delete the delegate.
  void Cancel();

 private:
  friend class IconLoaderProcessor;

  // Use the factory methods CreateIconLoader* instead of using this constructor
  IconLoader(const std::wstring& path,
             IconType type,
             bool from_resource,
             IconSize size,
             Delegate* delegate);

  // Invoked by the processor when the file has been read and an SkBitmap
  // object was requested.
  bool OnLoadComplete(SkBitmap* result);

  // Invoked by the processor when the file has been read and HICON handles
  // (small and large) were requested.
  bool OnLoadComplete(HICON small_icon, HICON large_icon);

  // The path.
  std::wstring path_;

  // The delegate.
  Delegate* delegate_;

  // Whether we are loading the icon from the resource in the file (if false,
  // the icon is simply loaded from the file content).
  bool loading_from_resource_;

  // The size of the icon that should be loaded from the file resource.
  // Not used if loading_from_resource_ is false.
  IconSize icon_size_;

  // The type of icon that should be generated.
  // Not used if loading_from_resource_ is true.
  IconType icon_type_;

  // The underlying object performing the read.
  IconLoaderProcessor* processor_;

  DISALLOW_EVIL_CONSTRUCTORS(IconLoader);
};

#endif  // CHROME_BROWSER_ICON_LOADER_H__
