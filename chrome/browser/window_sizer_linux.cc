// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/window_sizer.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "chrome/browser/browser.h"

// How much horizontal and vertical offset there is between newly
// opened windows. We don't care on Linux since the window manager generally
// does a good job of window placement.
const int WindowSizer::kWindowTilePixels = 0;

// An implementation of WindowSizer::MonitorInfoProvider that gets the actual
// monitor information from X via GDK.
class DefaultMonitorInfoProvider : public WindowSizer::MonitorInfoProvider {
 public:
  DefaultMonitorInfoProvider() { }

  virtual gfx::Rect GetPrimaryMonitorWorkArea() const {
    gfx::Rect rect = GetScreenWorkArea();
    return rect.Intersect(GetPrimaryMonitorBounds());
  }

  virtual gfx::Rect GetPrimaryMonitorBounds() const {
    GdkScreen* screen = gdk_screen_get_default();
    GdkRectangle rect;
    gdk_screen_get_monitor_geometry(screen, 0, &rect);
    return gfx::Rect(rect);
  }

  virtual gfx::Rect GetMonitorWorkAreaMatching(
      const gfx::Rect& match_rect) const {
    // TODO(thestig) Implement multi-monitor support.
    return GetPrimaryMonitorWorkArea();
  }

  virtual gfx::Point GetBoundsOffsetMatching(
      const gfx::Rect& match_rect) const {
    // TODO(thestig) Implement multi-monitor support.
    return GetPrimaryMonitorWorkArea().origin();
  }

  void UpdateWorkAreas() {
    // TODO(thestig) Implement multi-monitor support.
    work_areas_.clear();
    work_areas_.push_back(GetPrimaryMonitorBounds());
  }

 private:
  gfx::Rect GetScreenWorkArea() const {
    gboolean r;
    glong *data;
    gint data_len;
    r = gdk_property_get(gdk_get_default_root_window(),  // a gdk window
                         gdk_atom_intern("_NET_WORKAREA", FALSE),  // property
                         gdk_atom_intern("CARDINAL", FALSE),  // property type
                         0,  // byte offset into property
                         0xff,  // property length to retrieve
                         false,  // delete property after retrieval?
                         NULL,  // returned property type
                         NULL,  // returned data format
                         &data_len,  // returned data len
                         reinterpret_cast<guchar**>(&data));  // returned data
    CHECK(r);
    CHECK(data_len >= 16);
    gint x = data[0];
    gint y = data[1];
    gint width = data[0] + data[2];
    gint height = data[1] + data[3];
    g_free(data);
    return gfx::Rect(x, y, width, height);
  }

  DISALLOW_COPY_AND_ASSIGN(DefaultMonitorInfoProvider);
};

// static
WindowSizer::MonitorInfoProvider*
WindowSizer::CreateDefaultMonitorInfoProvider() {
  return new DefaultMonitorInfoProvider();
}
