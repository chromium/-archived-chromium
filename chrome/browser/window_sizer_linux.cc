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
    gfx::Rect rect;
    if (GetScreenWorkArea(&rect))
      return rect.Intersect(GetPrimaryMonitorBounds());

    // Return the best we've got.
    return GetPrimaryMonitorBounds();
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
  // Get the available screen space as a gfx::Rect, or return false if
  // if it's unavailable (i.e. the window manager doesn't support
  // retrieving this).
  bool GetScreenWorkArea(gfx::Rect* out_rect) const {
    gboolean ok;
    guchar* raw_data = NULL;
    gint data_len = 0;
    ok = gdk_property_get(gdk_get_default_root_window(),  // a gdk window
                          gdk_atom_intern("_NET_WORKAREA", FALSE),  // property
                          gdk_atom_intern("CARDINAL", FALSE),  // property type
                          0,  // byte offset into property
                          0xff,  // property length to retrieve
                          false,  // delete property after retrieval?
                          NULL,  // returned property type
                          NULL,  // returned data format
                          &data_len,  // returned data len
                          &raw_data);  // returned data
    if (!ok)
      return false;

    // We expect to get four longs back: x1, y1, x2, y2.
    if (data_len != 4 * sizeof(glong)) {
      NOTREACHED();
      g_free(raw_data);
      return false;
    }

    glong* data = reinterpret_cast<glong*>(raw_data);
    gint x = data[0];
    gint y = data[1];
    gint width = data[0] + data[2];
    gint height = data[1] + data[3];
    g_free(raw_data);

    out_rect->SetRect(x, y, width, height);
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(DefaultMonitorInfoProvider);
};

// static
WindowSizer::MonitorInfoProvider*
WindowSizer::CreateDefaultMonitorInfoProvider() {
  return new DefaultMonitorInfoProvider();
}
