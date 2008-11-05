// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include <string>

#include "base/clipboard.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"
#include "googleurl/src/gurl.h"

#include "SkBitmap.h"

// Clipboard glue

#if defined(OS_WIN)
// The call is being made within the current process.
void ScopedClipboardWriterGlue::WriteBitmap(const SkBitmap& bitmap) {
  SkAutoLockPixels bitmap_lock(bitmap);
  WriteBitmapFromPixels(bitmap.getPixels(), gfx::Size(bitmap.width(),
                        bitmap.height()));
}
#endif  // defined(OS_WIN)

ScopedClipboardWriterGlue::~ScopedClipboardWriterGlue() {
}

namespace webkit_glue {

Clipboard clipboard;

Clipboard* ClipboardGetClipboard() {
  return &clipboard;
}

bool ClipboardIsFormatAvailable(Clipboard::FormatType format) {
  return clipboard.IsFormatAvailable(format);
}

void ClipboardReadText(std::wstring* result) {
  clipboard.ReadText(result);
}

void ClipboardReadAsciiText(std::string* result) {
  clipboard.ReadAsciiText(result);
}

void ClipboardReadHTML(std::wstring* markup, GURL* url) {
  std::string url_str;
  clipboard.ReadHTML(markup, url ? &url_str : NULL);
  if (url)
    *url = GURL(url_str);
}

}  // namespace webkit_glue
