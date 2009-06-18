// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "views/window/window.h"

#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/size.h"
#include "base/string_util.h"

namespace views {

// static
int Window::GetLocalizedContentsWidth(int col_resource_id) {
  double chars = 0;
  StringToDouble(WideToUTF8(l10n_util::GetString(col_resource_id)), &chars);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font font = rb.GetFont(ResourceBundle::BaseFont);
  int width = font.GetExpectedTextWidth(static_cast<int>(chars));
  DCHECK(width > 0);
  return width;
}

// static
int Window::GetLocalizedContentsHeight(int row_resource_id) {
  double lines = 0;
  StringToDouble(WideToUTF8(l10n_util::GetString(row_resource_id)), &lines);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font font = rb.GetFont(ResourceBundle::BaseFont);
  int height = static_cast<int>(font.height() * lines);
  DCHECK(height > 0);
  return height;
}

// static
gfx::Size Window::GetLocalizedContentsSize(int col_resource_id,
                                           int row_resource_id) {
  return gfx::Size(GetLocalizedContentsWidth(col_resource_id),
                   GetLocalizedContentsHeight(row_resource_id));
}

}  // namespace views
