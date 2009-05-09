// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/default_theme_provider.h"

#include "app/resource_bundle.h"

namespace views {

SkBitmap* DefaultThemeProvider::GetBitmapNamed(int id) {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(id);
}

SkColor DefaultThemeProvider::GetColor(int id)  {
  // Return debugging-blue.
  return 0xff0000ff;
}

}  // namespace views
