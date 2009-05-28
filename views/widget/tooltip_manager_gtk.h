// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_TOOLTIP_MANAGER_GTK_H_
#define VIEWS_WIDGET_TOOLTIP_MANAGER_GTK_H_

#include "views/widget/tooltip_manager.h"

namespace views {

class Widget;

// TooltipManager takes care of the wiring to support tooltips for Views. You
// almost never need to interact directly with TooltipManager, rather look to
// the various tooltip methods on View.
class TooltipManagerGtk : public TooltipManager {
 public:
  explicit TooltipManagerGtk(Widget* widget);
  virtual ~TooltipManagerGtk() {}

  // TooltipManager.
  virtual void UpdateTooltip();
  virtual void TooltipTextChanged(View* view);
  virtual void ShowKeyboardTooltip(View* view);
  virtual void HideKeyboardTooltip();

 private:
  // Our owner.
  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(TooltipManagerGtk);
};

}  // namespace views

#endif // VIEWS_WIDGET_TOOLTIP_MANAGER_GTK_H_
