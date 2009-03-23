// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TAB_STRIP_GTK_H_
#define CHROME_BROWSER_GTK_TAB_STRIP_GTK_H_

#include <gtk/gtk.h>

#include "base/gfx/rect.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/owned_widget_gtk.h"
#include "skia/include/SkBitmap.h"

class TabStripGtk : public TabStripModelObserver {
 public:
  TabStripGtk(TabStripModel* model);
  virtual ~TabStripGtk();

  // Initialize and load the TabStrip into a container.
  void Init();
  void AddTabStripToBox(GtkWidget* box);

  TabStripModel* model() const { return model_; }

  // Sets the bounds of the tabs.
  void Layout();

  // Paints the tab at |index|.
  void PaintTab(int index, bool selected);

 protected:
  // TabStripModelObserver implementation:
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContents* contents, int from_index, int to_index);
  virtual void TabChangedAt(TabContents* contents, int index);

 private:
  // expose-event handler that redraws the tabstrip
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           TabStripGtk* tabstrip);

  // Gets the number of Tabs in the collection.
  size_t GetTabCount() const;

  // Cleans up the tab from the TabStrip at the specified |index|.
  void RemoveTabAt(int index);

  struct TabData;
  // Sets the bounds for each tab based on that TabStrip state.
  void LayoutTab(TabData* tab);
  void GenerateIdealBounds();

  // Returns the largest of the favicon, title text, and the close button.
  static int GetContentHeight();

  // Tab layout helpers.
  static gfx::Size GetMinimumUnselectedSize();
  static gfx::Size GetMinimumSelectedSize();
  static gfx::Size GetStandardSize();
  void GetDesiredTabWidths(size_t tab_count,
                           double* unselected_width,
                           double* selected_width) const;

  // Updates the state of |tab| based on the current state of |contents|.
  void UpdateTabData(TabContents* contents, TabData* tab);

  // Loads the images and font used by the TabStrip.
  static void LoadTabImages();
  static void InitResources();
  static bool initialized_;

  // Draws |pixbuf| at |x|,|y| in the TabStrip.
  void DrawImageInt(GdkPixbuf* pixbuf, int x, int y);

  // Tiles |pixbuf| at |x|,|y| in the TabStrip, filling up |w|x|h| area.
  void TileImageInt(GdkPixbuf* pixbuf,
                    int x, int y, int w, int h);

  // TODO(jhawkins): Move this data to TabRendererGtk.
  struct TabImage {
    GdkPixbuf* image_l;
    GdkPixbuf* image_c;
    GdkPixbuf* image_r;
    int l_width;
    int r_width;
  };
  static TabImage tab_active_;
  static TabImage tab_inactive_;
  static TabImage tab_inactive_otr_;
  static TabImage tab_hover_;

  struct ButtonImage {
    GdkPixbuf* normal;
    GdkPixbuf* hot;
    GdkPixbuf* pushed;
    int width;
    int height;
  };
  static ButtonImage close_button_;
  static ButtonImage newtab_button_;

  static GdkFont* title_font_;
  static int title_font_height_;

  static GdkPixbuf* download_icon_;
  static int download_icon_width_;
  static int download_icon_height_;

  // Model data. We store this here so that we don't need to ask the underlying
  // model, which is tricky since instances of this object can outlive the
  // corresponding objects in the underlying model.
  // TODO(jhawkins): Move this data into TabGtk/TabRendererGtk.
  struct TabData {
    SkBitmap favicon;
    bool show_icon;
    bool show_download_icon;
    std::wstring title;
    bool loading;
    gfx::Rect bounds;
    gfx::Rect close_button_bounds;
    gfx::Rect download_icon_bounds;
    gfx::Rect favicon_bounds;
    gfx::Rect title_bounds;
  };
  std::vector<TabData> tab_data_;

  // The drawing area widget.
  OwnedWidgetGtk tabstrip_;

  // Our model.
  TabStripModel* model_;
};

#endif  // CHROME_BROWSER_GTK_TAB_STRIP_GTK_H_
