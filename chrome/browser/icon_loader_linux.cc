// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <gtk/gtk.h>

#include "base/file_util.h"
#include "base/gfx/png_decoder.h"
#include "base/logging.h"
#include "base/linux_util.h"
#include "base/message_loop.h"
#include "base/mime_util.h"
#include "base/thread.h"
#include "base/string_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

void IconLoader::ReadIcon() {
  int size = 48;
  if (icon_size_ == NORMAL)
    size = 32;
  else if (icon_size_ == SMALL)
    size = 16;

  FilePath filename = mime_util::GetMimeIcon(group_, size);
  GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_size(filename.value().c_str(),
                                                       size, size, NULL);
  if (pixbuf) {
    guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    DCHECK_EQ(width, size);
    DCHECK_EQ(height, size);
    int stride = gdk_pixbuf_get_rowstride(pixbuf);

    if (!gdk_pixbuf_get_has_alpha(pixbuf)) {
      LOG(WARNING) << "Got an image with no alpha channel, aborting load.";
    } else {
      uint8_t* BGRA_pixels = base::BGRAToRGBA(pixels, width, height, stride);
      std::vector<unsigned char> pixel_vector;
      pixel_vector.resize(height * stride);
      memcpy(const_cast<unsigned char*>(pixel_vector.data()), BGRA_pixels,
             height * stride);
      bitmap_ = PNGDecoder::CreateSkBitmapFromBGRAFormat(pixel_vector,
                                                         width, height);
      free(BGRA_pixels);
    }

    g_object_unref(pixbuf);
  } else {
    LOG(WARNING) << "Unsupported file type or load error: " << filename.value();
  }

  target_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IconLoader::NotifyDelegate));
}
