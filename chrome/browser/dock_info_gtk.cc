// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dock_info.h"

#include "base/gfx/native_widget_types.h"
#include "base/logging.h"

// static
DockInfo DockInfo::GetDockInfoAtPoint(const gfx::Point& screen_point,
                                      const std::set<GtkWidget*>& ignore) {
  NOTIMPLEMENTED();
  return DockInfo();
}

GtkWindow* DockInfo::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<GtkWidget*>& ignore) {
  NOTIMPLEMENTED();
  if (factory_)
    return factory_->GetLocalProcessWindowAtPoint(screen_point, ignore);
  return NULL;
}

bool DockInfo::GetWindowBounds(gfx::Rect* bounds) const {
  NOTIMPLEMENTED();
  return true;
}

void DockInfo::SizeOtherWindowTo(const gfx::Rect& bounds) const {
  NOTIMPLEMENTED();
}
