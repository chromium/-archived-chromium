// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/back_forward_menu_model_views.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/menu/menu_2.h"
#include "views/widget/widget.h"

////////////////////////////////////////////////////////////////////////////////
// BackForwardMenuModelViews, public:

BackForwardMenuModelViews::BackForwardMenuModelViews(Browser* browser,
                                                     ModelType model_type,
                                                     views::Widget* frame)
    : BackForwardMenuModel(browser, model_type),
      frame_(frame) {
}

////////////////////////////////////////////////////////////////////////////////
// BackForwardMenuModelViews, views::Menu2Model implementation:

bool BackForwardMenuModelViews::HasIcons() const {
  return true;
}

int BackForwardMenuModelViews::GetItemCount() const {
  return GetTotalItemCount();
}

views::Menu2Model::ItemType BackForwardMenuModelViews::GetTypeAt(
    int index) const {
  if (IsSeparator(GetCommandIdAt(index)))
    return views::Menu2Model::TYPE_SEPARATOR;
  return views::Menu2Model::TYPE_COMMAND;
}

int BackForwardMenuModelViews::GetCommandIdAt(int index) const {
  return index + 1;
}

string16 BackForwardMenuModelViews::GetLabelAt(int index) const {
  return GetItemLabel(GetCommandIdAt(index));
}

bool BackForwardMenuModelViews::IsLabelDynamicAt(int index) const {
  return false;
}

bool BackForwardMenuModelViews::GetAcceleratorAt(
    int index,
    views::Accelerator* accelerator) const {
  // Look up the history accelerator.
  if (GetCommandIdAt(index) == GetTotalItemCount())
    return frame_->GetAccelerator(IDC_SHOW_HISTORY, accelerator);
  return false;
}

bool BackForwardMenuModelViews::IsItemCheckedAt(int index) const {
  return false;
}

int BackForwardMenuModelViews::GetGroupIdAt(int index) const {
  return -1;
}

bool BackForwardMenuModelViews::GetIconAt(int index, SkBitmap* icon) const {
  if (ItemHasIcon(GetCommandIdAt(index))) {
    *icon = GetItemIcon(GetCommandIdAt(index));
    return true;
  }
  return false;
}

bool BackForwardMenuModelViews::IsEnabledAt(int index) const {
  return true;
}

views::Menu2Model* BackForwardMenuModelViews::GetSubmenuModelAt(
    int index) const {
  return NULL;
}

void BackForwardMenuModelViews::HighlightChangedTo(int index) {
}

void BackForwardMenuModelViews::ActivatedAt(int index) {
  ExecuteCommandById(GetCommandIdAt(index));
}

void BackForwardMenuModelViews::MenuWillShow() {
  UserMetrics::RecordComputedAction(BuildActionName(L"Popup", -1),
                                    browser_->profile());
}
