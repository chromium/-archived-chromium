// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_theme_source.h"

#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/gfx/png_encoder.h"
#include "base/message_loop.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/theme_resources_util.h"
#include "chrome/common/url_constants.h"
#include "grit/theme_resources.h"

DOMUIThemeSource::DOMUIThemeSource(Profile* profile)
  : DataSource(chrome::kChromeUIThemePath, MessageLoop::current()),
    profile_(profile) {
}

void DOMUIThemeSource::StartDataRequest(const std::string& path,
                                            int request_id) {
  ThemeProvider* tp = profile_->GetThemeProvider();
  if (tp) {
    int id = ThemeResourcesUtil::GetId(path);
    if (id != -1) {
      SkBitmap* image = tp->GetBitmapNamed(id);
      if (image) {
        std::vector<unsigned char> png_bytes;
        PNGEncoder::EncodeBGRASkBitmap(*image, false, &png_bytes);

        scoped_refptr<RefCountedBytes> image_data = new RefCountedBytes(png_bytes);
        SendResponse(request_id, image_data);
      }
    }
  }
}
