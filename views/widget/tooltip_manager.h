// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_TOOLTIP_MANAGER_H_
#define VIEWS_WIDGET_TOOLTIP_MANAGER_H_

#include <string>

#include "base/basictypes.h"

namespace gfx {
class Font;
}  // namespace gfx

namespace views {

class View;

// TooltipManager takes care of the wiring to support tooltips for Views. You
// almost never need to interact directly with TooltipManager, rather look to
// the various tooltip methods on View.
class TooltipManager {
 public:
  // Returns the height of tooltips. This should only be invoked from within
  // GetTooltipTextOrigin.
  static int GetTooltipHeight();

  // Returns the default font used by tooltips.
  static gfx::Font GetDefaultFont();

  // Returns the separator for lines of text in a tooltip.
  static const std::wstring& GetLineSeparator();

  TooltipManager() {}
  virtual ~TooltipManager() {}

  // Notification that the view hierarchy has changed in some way.
  virtual void UpdateTooltip() = 0;

  // Invoked when the tooltip text changes for the specified views.
  virtual void TooltipTextChanged(View* view) = 0;

  // Invoked when toolbar icon gets focus.
  virtual void ShowKeyboardTooltip(View* view) = 0;

  // Invoked when toolbar loses focus.
  virtual void HideKeyboardTooltip() = 0;
};

}  // namespace views

#endif  // VIEWS_WIDGET_TOOLTIP_MANAGER_H_
