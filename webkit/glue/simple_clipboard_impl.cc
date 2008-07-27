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

#include "webkit/glue/webkit_glue.h"

#include <string>

#include "base/clipboard.h"
#include "googleurl/src/gurl.h"

#include "SkBitmap.h"

// Clipboard glue
// Basically just proxy the calls off to the clipboard

namespace webkit_glue {

Clipboard clipboard;

void webkit_glue::ClipboardClear() {
  clipboard.Clear();
}

void webkit_glue::ClipboardWriteText(const std::wstring& text) {
  clipboard.WriteText(text);
}

void webkit_glue::ClipboardWriteHTML(const std::wstring& html,
                                     const GURL& url) {
  clipboard.WriteHTML(html, url.spec());
}

void webkit_glue::ClipboardWriteBookmark(const std::wstring& title,
                                         const GURL& url) {
  clipboard.WriteBookmark(title, url.spec());
}

void webkit_glue::ClipboardWriteBitmap(const SkBitmap& bitmap) {
  SkAutoLockPixels bitmap_lock(bitmap); 
  clipboard.WriteBitmap(bitmap.getPixels(),
                        gfx::Size(bitmap.width(), bitmap.height()));
}

void webkit_glue::ClipboardWriteWebSmartPaste() {
  clipboard.WriteWebSmartPaste();
}

bool webkit_glue::ClipboardIsFormatAvailable(unsigned int format) {
  return clipboard.IsFormatAvailable(format);
}

void webkit_glue::ClipboardReadText(std::wstring* result) {
  clipboard.ReadText(result);
}

void webkit_glue::ClipboardReadAsciiText(std::string* result) {
  clipboard.ReadAsciiText(result);
}

void webkit_glue::ClipboardReadHTML(std::wstring* markup, GURL* url) {
  std::string url_str;
  clipboard.ReadHTML(markup, url ? &url_str : NULL);
  if (url)
    *url = GURL(url_str);
}

}  // namespace webkit_glue
