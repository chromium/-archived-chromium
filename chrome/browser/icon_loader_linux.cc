// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include "base/file_util.h"
#include "base/gfx/png_decoder.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/mime_util.h"
#include "base/thread.h"
#include "base/string_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

void IconLoader::ReadIcon() {
  size_t size = 48;
  if (icon_size_ == NORMAL)
    size = 32;
  else if (icon_size_ == SMALL)
    size = 16;

  FilePath filename = mime_util::GetMimeIcon(group_, size);
  if (LowerCaseEqualsASCII(mime_util::GetFileMimeType(filename), "image/png")) {
    std::string file_contents;
    file_util::ReadFileToString(filename, &file_contents);

    std::vector<unsigned char> pixels;
    int width, height;
    if (PNGDecoder::Decode(
        reinterpret_cast<const unsigned char*>(file_contents.c_str()),
        file_contents.length(), PNGDecoder::FORMAT_BGRA,
        &pixels, &width, &height)) {
      bitmap_ = PNGDecoder::CreateSkBitmapFromBGRAFormat(pixels, width, height);
    }
  } else {
    // TODO(estade): support other file types.
  }

  target_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IconLoader::NotifyDelegate));
}
