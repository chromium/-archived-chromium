// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dock_info.h"

#include <gtk/gtk.h>

#include "base/gfx/native_widget_types.h"
#include "base/logging.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"

// static
DockInfo DockInfo::GetDockInfoAtPoint(const gfx::Point& screen_point,
                                      const std::set<GtkWidget*>& ignore) {
  if (factory_)
    return factory_->GetDockInfoAtPoint(screen_point, ignore);

  NOTIMPLEMENTED();
  return DockInfo();
}

GtkWindow* DockInfo::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<GtkWidget*>& ignore) {
  if (factory_)
    return factory_->GetLocalProcessWindowAtPoint(screen_point, ignore);

  // Iterate over the browserlist to find the window at the specified point.
  // This is technically not correct as it doesn't take into account other
  // windows that may be on top of a browser window at the same point, but is
  // good enough for now.
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    Browser* browser = *i;
    GtkWindow* window = browser->window()->GetNativeHandle();
    if (ignore.count(GTK_WIDGET(window)) == 0) {
      int x, y, w, h;
      gtk_window_get_position(window, &x, &y);
      gtk_window_get_size(window, &w, &h);
      if (gfx::Rect(x, y, w, h).Contains(screen_point))
        return window;
    }
  }
  return NULL;
}

bool DockInfo::GetWindowBounds(gfx::Rect* bounds) const {
  if (!window())
    return false;

  int x, y, w, h;
  gtk_window_get_position(window(), &x, &y);
  gtk_window_get_size(window(), &w, &h);
  bounds->SetRect(x, y, w, h);
  return true;
}

void DockInfo::SizeOtherWindowTo(const gfx::Rect& bounds) const {
  gtk_window_move(window(), bounds.x(), bounds.y());
  gtk_window_resize(window(), bounds.width(), bounds.height());
}
