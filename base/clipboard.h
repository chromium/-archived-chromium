// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CLIPBOARD_H_
#define BASE_CLIPBOARD_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/size.h"
#include "base/shared_memory.h"

#if defined(OS_MACOSX)
@class NSString;
#endif

class Clipboard {
 public:
#if defined(OS_WIN)
  typedef unsigned int FormatType;
#elif defined(OS_MACOSX)
  typedef NSString *FormatType;
#endif
  
  Clipboard();
  ~Clipboard();

  // Clears the clipboard.  It is usually a good idea to clear the clipboard
  // before writing content to the clipboard.
  void Clear();

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

#if defined(OS_WIN)
  // Adds a bitmap to the clipboard
  // This is the slowest way to copy a bitmap to the clipboard as we must first
  // memcpy the pixels into GDI and the blit the bitmap to the clipboard.
  // Pixel format is assumed to be 32-bit BI_RGB.
  void WriteBitmap(const void* pixels, const gfx::Size& size);

  // Adds a bitmap to the clipboard
  // This function requires read and write access to the bitmap, but does not
  // actually modify the shared memory region.
  // Pixel format is assumed to be 32-bit BI_RGB.
  void WriteBitmapFromSharedMemory(const SharedMemory& bitmap,
                                   const gfx::Size& size);

  // Adds a bitmap to the clipboard
  // This is the fastest way to copy a bitmap to the clipboard.  The HBITMAP
  // may either be device-dependent or device-independent.
  void WriteBitmapFromHandle(HBITMAP hbitmap, const gfx::Size& size);

  // Used by WebKit to determine whether WebKit wrote the clipboard last
  void WriteWebSmartPaste();
#endif

  // Adds a file or group of files to the clipboard.
  void WriteFile(const std::wstring& file);
  void WriteFiles(const std::vector<std::wstring>& files);

  // Tests whether the clipboard contains a certain format
  bool IsFormatAvailable(FormatType format) const;

  // Reads UNICODE text from the clipboard, if available.
  void ReadText(std::wstring* result) const;

  // Reads ASCII text from the clipboard, if available.
  void ReadAsciiText(std::string* result) const;

  // Reads HTML from the clipboard, if available.
  void ReadHTML(std::wstring* markup, std::string* src_url) const;

  // Reads a bookmark from the clipboard, if available.
  void ReadBookmark(std::wstring* title, std::string* url) const;

  // Reads a file or group of files from the clipboard, if available, into the
  // out parameter.
  void ReadFile(std::wstring* file) const;
  void ReadFiles(std::vector<std::wstring>* files) const;

 private:
#if defined(OS_WIN)
  static void MarkupToHTMLClipboardFormat(const std::wstring& markup,
                                          const std::string& src_url,
                                          std::string* html_fragment);

  static void ParseHTMLClipboardFormat(const std::string& html_fragment,
                                       std::wstring* markup,
                                       std::string* src_url);

  static void ParseBookmarkClipboardFormat(const std::wstring& bookmark,
                                           std::wstring* title,
                                           std::string* url);

  HWND clipboard_owner_;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(Clipboard);
};

#endif  // BASE_CLIPBOARD_H_

