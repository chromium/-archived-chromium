// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/default_theme_provider.h"

#include "app/resource_bundle.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

namespace views {

SkBitmap* DefaultThemeProvider::GetBitmapNamed(int id) {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(id);
}

SkColor DefaultThemeProvider::GetColor(int id)  {
  // Return debugging-blue.
  return 0xff0000ff;
}

bool DefaultThemeProvider::GetDisplayProperty(int id, int* result) {
  return false;
}
bool DefaultThemeProvider::ShouldUseNativeFrame() {
#if defined(OS_WIN)
  return win_util::ShouldUseVistaFrame();
#else
  return false;
#endif
}

bool DefaultThemeProvider::HasCustomImage(int id) {
  return false;
}
}  // namespace views
