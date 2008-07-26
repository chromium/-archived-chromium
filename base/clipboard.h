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

#ifndef BASE_CLIPBOARD_H__
#define BASE_CLIPBOARD_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/size.h"
#include "base/shared_memory.h"

class Clipboard {
 public:
  Clipboard();
  ~Clipboard();

  // Clears the clipboard.  It is usually a good idea to clear the clipboard
  // before writing content to the clipboard.
  void Clear() const;

  // Adds UNICODE and ASCII text to the clipboard.
  void WriteText(const std::wstring& text) const;

  // Adds HTML to the clipboard.  The url parameter is optional, but especially
  // useful if the HTML fragment contains relative links
  void WriteHTML(const std::wstring& markup, const std::string& src_url) const;

  // Adds a bookmark to the clipboard
  void WriteBookmark(const std::wstring& title, const std::string& url) const;

  // Adds both a bookmark and an HTML hyperlink to the clipboard.  It is a
  // convenience wrapper around WriteBookmark and WriteHTML.
  void WriteHyperlink(const std::wstring& title, const std::string& url) const;

  // Adds a bitmap to the clipboard
  // This is the slowest way to copy a bitmap to the clipboard as we must first
  // memcpy the pixels into GDI and the blit the bitmap to the clipboard.
  // Pixel format is assumed to be 32-bit BI_RGB.
  void WriteBitmap(const void* pixels, const gfx::Size& size) const;

  // Adds a bitmap to the clipboard
  // This function requires read and write access to the bitmap, but does not
  // actually modify the shared memory region.
  // Pixel format is assumed to be 32-bit BI_RGB.
  void WriteBitmapFromSharedMemory(const SharedMemory& bitmap,
                                   const gfx::Size& size) const;

  // Adds a bitmap to the clipboard
  // This is the fastest way to copy a bitmap to the clipboard.  The HBITMAP
  // may either be device-dependent or device-independent.
  void WriteBitmapFromHandle(HBITMAP hbitmap, const gfx::Size& size) const;

  // Used by WebKit to determine whether WebKit wrote the clipboard last
  void WriteWebSmartPaste() const;

  // Adds a file or group of files to the clipboard.
  void WriteFile(const std::wstring& file) const;
  void WriteFiles(const std::vector<std::wstring>& files) const;

  // Tests whether the clipboard contains a certain format
  bool IsFormatAvailable(unsigned int format) const;

  // Reads UNICODE text from the clipboard, if available.
  void ReadText(std::wstring* result) const;

  // Reads ASCII text from the clipboard, if available.
  void ReadAsciiText(std::string* result) const;

  // Reads HTML from the clipboard, if available.
  void ReadHTML(std::wstring* markup, std::string* src_url) const;

  // Reads a bookmark from the clipboard, if available.
  void ReadBookmark(std::wstring* title, std::string* url) const;

  // Reads a file or group of files from the clipboard, if available, into the
  // out paramter.
  void ReadFile(std::wstring* file) const;
  void ReadFiles(std::vector<std::wstring>* files) const;

 private:
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

  DISALLOW_EVIL_CONSTRUCTORS(Clipboard);
};

#endif  // BASE_CLIPBOARD_H__
