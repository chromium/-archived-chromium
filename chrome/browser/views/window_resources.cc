// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/window_resources.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/resource_bundle.h"

#include "SkBitmap.h"

// static
SkBitmap WindowResources::app_top_left_;
SkBitmap WindowResources::app_top_center_;
SkBitmap WindowResources::app_top_right_;

// static
void WindowResources::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    app_top_left_ = *rb.GetBitmapNamed(IDR_APP_TOP_LEFT);
    app_top_center_ = *rb.GetBitmapNamed(IDR_APP_TOP_CENTER);
    app_top_right_ = *rb.GetBitmapNamed(IDR_APP_TOP_RIGHT);      
    initialized = true;
  }
}
