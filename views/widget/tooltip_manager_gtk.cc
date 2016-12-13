// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/tooltip_manager_gtk.h"

#include "app/gfx/font.h"
#include "base/logging.h"

namespace views {

// static
int TooltipManager::GetTooltipHeight() {
  NOTIMPLEMENTED();
  return 0;
}

// static
gfx::Font TooltipManager::GetDefaultFont() {
  NOTIMPLEMENTED();
  return gfx::Font();
}

// static
const std::wstring& TooltipManager::GetLineSeparator() {
  static std::wstring* line_separator = NULL;
  if (!line_separator)
    line_separator = new std::wstring(L"\n");
  return *line_separator;
}

TooltipManagerGtk::TooltipManagerGtk(Widget* widget) : widget_(widget) {
}

void TooltipManagerGtk::UpdateTooltip() {
  NOTIMPLEMENTED();
}

void TooltipManagerGtk::TooltipTextChanged(View* view) {
  NOTIMPLEMENTED();
}

void TooltipManagerGtk::ShowKeyboardTooltip(View* view) {
  NOTIMPLEMENTED();
}

void TooltipManagerGtk::HideKeyboardTooltip() {
  NOTIMPLEMENTED();
}

}  // namespace views
