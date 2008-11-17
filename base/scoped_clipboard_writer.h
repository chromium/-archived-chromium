// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SCOPED_CLIPBOARD_WRITER_H_
#define SCOPED_CLIPBOARD_WRITER_H_

#include "base/clipboard.h"

#if defined(OS_WIN)
class SkBitmap;
#endif

// This class is a wrapper for |Clipboard| that handles packing data
// into a Clipboard::ObjectMap.
// NB: You should probably NOT be using this class if you include
// webkit_glue.h. Use ScopedClipboardWriterGlue instead.
class ScopedClipboardWriter {
 public:
  // Create an instance that is a simple wrapper around clipboard.
  ScopedClipboardWriter(Clipboard* clipboard);

  ~ScopedClipboardWriter();

  // Adds UNICODE and ASCII text to the clipboard.
  void WriteText(const std::wstring& text);

  // Adds HTML to the clipboard.  The url parameter is optional, but especially
  // useful if the HTML fragment contains relative links
  void WriteHTML(const std::wstring& markup, const std::string& src_url);

  // Adds a bookmark to the clipboard
  void WriteBookmark(const std::wstring& title, const std::string& url);

  // Adds both a bookmark and an HTML hyperlink to the clipboard.  It is a
  // convenience wrapper around WriteBookmark and WriteHTML.
  void WriteHyperlink(const std::wstring& title, const std::string& url);

  // Adds a file or group of files to the clipboard.
  void WriteFile(const std::wstring& file);
  void WriteFiles(const std::vector<std::wstring>& files);

  // Used by WebKit to determine whether WebKit wrote the clipboard last
  void WriteWebSmartPaste();

#if defined(OS_WIN)
  // Adds a bitmap to the clipboard
  // This is the slowest way to copy a bitmap to the clipboard as we must first
  // memcpy the pixels into GDI and the blit the bitmap to the clipboard.
  // Pixel format is assumed to be 32-bit BI_RGB.
  void WriteBitmapFromPixels(const void* pixels, const gfx::Size& size);
#endif

 protected:
  Clipboard::ObjectMap objects_;
  Clipboard* clipboard_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedClipboardWriter);
};

#endif  // SCOPED_CLIPBOARD_WRITER_H_
