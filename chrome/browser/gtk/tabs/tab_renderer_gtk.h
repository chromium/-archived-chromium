// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_GTK_TABS_TAB_RENDERER_GTK_H_
#define CHROME_BROWSER_VIEWS_GTK_TABS_TAB_RENDERER_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "skia/include/SkBitmap.h"

namespace gfx {
class Size;
}

class TabContents;

class TabRendererGtk {
 public:
  TabRendererGtk();
  virtual ~TabRendererGtk();

  // Updates the data the Tab uses to render itself from the specified
  // TabContents.
  void UpdateData(TabContents* contents);

  // Updates the display to reflect the contents of this TabRenderer's model.
  void UpdateFromModel();

  // Returns true if the Tab is selected, false otherwise.
  virtual bool IsSelected() const;

  // Returns the minimum possible size of a single unselected Tab.
  static gfx::Size GetMinimumUnselectedSize();
  // Returns the minimum possible size of a selected Tab. Selected tabs must
  // always show a close button and have a larger minimum size than unselected
  // tabs.
  static gfx::Size GetMinimumSelectedSize();
  // Returns the preferred size of a single Tab, assuming space is
  // available.
  static gfx::Size GetStandardSize();

  // Loads the images to be used for the tab background.
  static void LoadTabImages();

  // Returns the bounds of the Tab.
  int x() const { return bounds_.x(); }
  int y() const { return bounds_.y(); }
  int width() const { return bounds_.width(); }
  int height() const { return bounds_.height(); }

  // Sets the bounds of the tab.
  void SetBounds(const gfx::Rect& bounds);

  // Paints the tab into |canvas|.
  void Paint(ChromeCanvasPaint* canvas);

 protected:
  const gfx::Rect& title_bounds() const { return title_bounds_; }

  // Returns the title of the Tab.
  std::wstring GetTitle() const;

 private:
  // Generates the bounds for the interior items of the tab.
  void Layout();

  // Returns the largest of the favicon, title text, and the close button.
  static int GetContentHeight();

  // Paint various portions of the Tab
  // TODO(jhawkins): Paint hover tab.
  void PaintTabBackground(ChromeCanvasPaint* canvas);
  void PaintInactiveTabBackground(ChromeCanvasPaint* canvas);
  void PaintActiveTabBackground(ChromeCanvasPaint* canvas);
  void PaintLoadingAnimation(ChromeCanvasPaint* canvas);

  // Returns the number of favicon-size elements that can fit in the tab's
  // current size.
  int IconCapacity() const;

  // Returns whether the Tab should display a favicon.
  bool ShouldShowIcon() const;

  // Returns whether the Tab should display a close button.
  bool ShouldShowCloseBox() const;

  // TODO(jhawkins): Move to TabResources.
  static void InitResources();
  static bool initialized_;

  // The bounds of various sections of the display.
  gfx::Rect favicon_bounds_;
  gfx::Rect download_icon_bounds_;
  gfx::Rect title_bounds_;
  gfx::Rect close_button_bounds_;

  // Model data. We store this here so that we don't need to ask the underlying
  // model, which is tricky since instances of this object can outlive the
  // corresponding objects in the underlying model.
  struct TabData {
    SkBitmap favicon;
    std::wstring title;
    bool loading;
    bool crashed;
    bool off_the_record;
    bool show_icon;
    bool show_download_icon;
  };
  TabData data_;

  // TODO(jhawkins): Move into TabResources class.
  struct TabImage {
    SkBitmap* image_l;
    SkBitmap* image_c;
    SkBitmap* image_r;
    int l_width;
    int r_width;
  };
  static TabImage tab_active_;
  static TabImage tab_inactive_;
  static TabImage tab_inactive_otr_;
  static TabImage tab_hover_;

  struct ButtonImage {
    SkBitmap* normal;
    SkBitmap* hot;
    SkBitmap* pushed;
    int width;
    int height;
  };
  static ButtonImage close_button_;
  static ButtonImage newtab_button_;

  static ChromeFont title_font_;
  static int title_font_height_;

  static SkBitmap* download_icon_;
  static int download_icon_width_;
  static int download_icon_height_;

  // Whether we're showing the icon. It is cached so that we can detect when it
  // changes and layout appropriately.
  bool showing_icon_;

  // Whether we are showing the download icon. Comes from the model.
  bool showing_download_icon_;

  // Whether we are showing the close button. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_close_button_;

  // The offset used to animate the favicon location.
  int fav_icon_hiding_offset_;

  // Set when the crashed favicon should be displayed.
  bool should_display_crashed_favicon_;

  // The bounds of this Tab.
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(TabRendererGtk);
};

#endif  // CHROME_BROWSER_VIEWS_GTK_TABS_TAB_RENDERER_GTK_H_
