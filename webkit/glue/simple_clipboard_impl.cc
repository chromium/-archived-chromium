// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

