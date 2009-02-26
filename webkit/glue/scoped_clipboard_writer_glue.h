// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SCOPED_CLIPBOARD_WRITER_GLUE_H_
#define SCOPED_CLIPBOARD_WRITER_GLUE_H_

#include "base/scoped_clipboard_writer.h"

class SkBitmap;

namespace base {
class SharedMemory;
}

class ScopedClipboardWriterGlue : public ScopedClipboardWriter {
 public:
  ScopedClipboardWriterGlue(Clipboard* clipboard)
      : ScopedClipboardWriter(clipboard),
        shared_buf_(NULL) {
  }

  ~ScopedClipboardWriterGlue();

#if defined(OS_WIN)
  void WriteBitmapFromPixels(const void* pixels, const gfx::Size& size);
#endif

 private:
  base::SharedMemory* shared_buf_;
  DISALLOW_COPY_AND_ASSIGN(ScopedClipboardWriterGlue);
};

#endif  // SCOPED_CLIPBOARD_WRITER_GLUE_H_
