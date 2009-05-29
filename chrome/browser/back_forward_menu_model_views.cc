// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/back_forward_menu_model_views.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "grit/generated_resources.h"

// static
BackForwardMenuModel* BackForwardMenuModel::Create(Browser* browser,
                                                   ModelType model_type) {
  return new BackForwardMenuModelViews(browser, model_type);
}

BackForwardMenuModelViews::BackForwardMenuModelViews(Browser* browser,
                                                     ModelType model_type) {
  browser_ = browser;
  model_type_ = model_type;
}

std::wstring BackForwardMenuModelViews::GetLabel(int menu_id) const {
  return GetItemLabel(menu_id);
}

const SkBitmap& BackForwardMenuModelViews::GetIcon(int menu_id) const {
  // Return NULL if the item doesn't have an icon
  if (!ItemHasIcon(menu_id))
    return GetEmptyIcon();

  return GetItemIcon(menu_id);
}

bool BackForwardMenuModelViews::IsItemSeparator(int menu_id) const {
  return IsSeparator(menu_id);
}

bool BackForwardMenuModelViews::HasIcon(int menu_id) const {
  return ItemHasIcon(menu_id);
}

bool BackForwardMenuModelViews::SupportsCommand(int menu_id) const {
  return ItemHasCommand(menu_id);
}

bool BackForwardMenuModelViews::IsCommandEnabled(int menu_id) const {
  return ItemHasCommand(menu_id);
}

void BackForwardMenuModelViews::ExecuteCommand(int menu_id) {
  ExecuteCommandById(menu_id);
}

void BackForwardMenuModelViews::MenuWillShow() {
  UserMetrics::RecordComputedAction(BuildActionName(L"Popup", -1),
                                    browser_->profile());
}

int BackForwardMenuModelViews::GetItemCount() const {
  return GetTotalItemCount();
}
