// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tab_strip_gtk.h"

#include "base/gfx/gtk_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

static const int kLeftPadding = 16;
static const int kTopPadding = 6;
static const int kRightPadding = 15;
static const int kBottomPadding = 5;
static const int kFavIconTitleSpacing = 4;
static const int kTitleCloseButtonSpacing = 5;
static const int kStandardTitleWidth = 175;
static const int kFavIconSize = 16;
static const int kNewTabButtonHOffset = -5;
static const int kNewTabButtonVOffset = 5;
static const GdkColor kSelectedTitleColor = GDK_COLOR_RGB(0, 0, 0);
static const GdkColor kUnselectedTitleColor = GDK_COLOR_RGB(64, 64, 64);

// The horizontal offset from one tab to the next,
// which results in overlapping tabs.
static const int kTabHOffset = -16;

// The vertical and horizontal offset used to position the close button
// in the tab. TODO(jhawkins): Ask pkasting what the Fuzz is about.
static const int kCloseButtonVertFuzz = 0;
static const int kCloseButtonHorzFuzz = 5;

// TODO(jhawkins): Move this code into ChromeFont and allow pulling out a
// GdkFont*.
static GdkFont* load_default_font() {
  GtkSettings* settings = gtk_settings_get_default();

  GValue value = { 0 };
  g_value_init(&value, G_TYPE_STRING);
  g_object_get_property(G_OBJECT(settings), "gtk-font-name", &value);

  gchar* font_name = g_strdup_value_contents(&value);
  PangoFontDescription* font_description = pango_font_description_from_string(
      font_name);

  GdkFont* font = gdk_font_from_description(font_description);
  g_free(font_name);
  return font;
}

}  // namespace

bool TabStripGtk::initialized_ = false;
TabStripGtk::TabImage TabStripGtk::tab_active_ = {0};
TabStripGtk::TabImage TabStripGtk::tab_inactive_ = {0};
TabStripGtk::TabImage TabStripGtk::tab_inactive_otr_ = {0};
TabStripGtk::TabImage TabStripGtk::tab_hover_ = {0};
TabStripGtk::ButtonImage TabStripGtk::close_button_ = {0};
TabStripGtk::ButtonImage TabStripGtk::newtab_button_ = {0};
GdkFont* TabStripGtk::title_font_ = NULL;
int TabStripGtk::title_font_height_ = 0;
GdkPixbuf* TabStripGtk::download_icon_ = NULL;
int TabStripGtk::download_icon_width_ = 0;
int TabStripGtk::download_icon_height_ = 0;

static inline int Round(double x) {
  return static_cast<int>(floor(x + 0.5));
}

////////////////////////////////////////////////////////////////////////////////
// TabStripGtk, public:

TabStripGtk::TabStripGtk(TabStripModel* model)
    : model_(model) {

}

TabStripGtk::~TabStripGtk() {
  model_->RemoveObserver(this);
  tabstrip_.Destroy();
}

void TabStripGtk::Init() {
  model_->AddObserver(this);

  InitResources();

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* tab_center = rb.LoadPixbuf(IDR_TAB_ACTIVE_CENTER);

  tabstrip_.Own(gtk_drawing_area_new());
  gtk_widget_set_size_request(tabstrip_.get(), -1,
                              gdk_pixbuf_get_height(tab_center));
  gtk_widget_set_app_paintable(tabstrip_.get(), TRUE);
  g_signal_connect(G_OBJECT(tabstrip_.get()), "expose-event",
                   G_CALLBACK(OnExpose), this);
  gtk_widget_show_all(tabstrip_.get());
}

void TabStripGtk::AddTabStripToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), tabstrip_.get(), FALSE, FALSE, 0);
}

////////////////////////////////////////////////////////////////////////////////
// TabStripGtk, TabStripModelObserver implementation:

void TabStripGtk::TabInsertedAt(TabContents* contents,
                                int index,
                                bool foreground) {
  TabData tabdata;
  UpdateTabData(contents, &tabdata);
  tab_data_.push_back(tabdata);

  gtk_widget_queue_draw(tabstrip_.get());
}

void TabStripGtk::TabDetachedAt(TabContents* contents, int index) {
  RemoveTabAt(index);
  gtk_widget_queue_draw(tabstrip_.get());
}

void TabStripGtk::TabSelectedAt(TabContents* old_contents,
                                TabContents* new_contents,
                                int index,
                                bool user_gesture) {
  gtk_widget_queue_draw(tabstrip_.get());
}

void TabStripGtk::TabMoved(TabContents* contents,
                           int from_index,
                           int to_index) {
  gtk_widget_queue_draw(tabstrip_.get());
}

void TabStripGtk::TabChangedAt(TabContents* contents, int index) {
  UpdateTabData(contents, &tab_data_.at(index));

  gtk_widget_queue_draw(tabstrip_.get());
}

void TabStripGtk::LayoutTab(TabData* tab) {
  gfx::Rect bounds = tab->bounds;
  if (bounds.IsEmpty())
    return;

  bounds.Inset(kLeftPadding, kTopPadding, kRightPadding, kBottomPadding);

  // Figure out who is tallest.
  int content_height = GetContentHeight();

  // Size the FavIcon.
  if (tab->show_icon) {
    int favicon_top = kTopPadding + (content_height - kFavIconSize) / 2;
    tab->favicon_bounds.SetRect(bounds.x(), favicon_top,
                                kFavIconSize, kFavIconSize);
  } else {
    tab->favicon_bounds.SetRect(bounds.x(), bounds.y(), 0, 0);
  }

  // Size the download icon.
  if (tab->show_download_icon) {
    int icon_top = kTopPadding + (content_height - download_icon_height_) / 2;
    tab->download_icon_bounds.SetRect(bounds.width() - download_icon_width_,
                                      icon_top, download_icon_width_,
                                      download_icon_height_);
  }

  int close_button_top = kTopPadding + kCloseButtonVertFuzz +
      (content_height - close_button_.height) / 2;
  tab->close_button_bounds.SetRect(tab->bounds.x() + bounds.width() +
      kCloseButtonHorzFuzz, close_button_top, close_button_.width,
      close_button_.height);

  // Size the title text to fill the remaining space.
  int title_left = tab->favicon_bounds.right() + kFavIconTitleSpacing;
  int title_top = kTopPadding + (content_height - title_font_height_) / 2;

  // If the user has big fonts, the title will appear rendered too far down on
  // the y-axis if we use the regular top padding, so we need to adjust it so
  // that the text appears centered.
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  int text_height = title_top + title_font_height_ + kBottomPadding;
  if (text_height > minimum_size.height())
    title_top -= (text_height - minimum_size.height()) / 2;

  int title_width = std::max(tab->close_button_bounds.x() -
                             kTitleCloseButtonSpacing - title_left, 0);
  if (tab->show_download_icon)
    title_width = std::max(title_width - download_icon_width_, 0);
  title_top = tab->bounds.height() - title_top;
  tab->title_bounds.SetRect(title_left, title_top,
                            title_width, title_font_height_);
}

void TabStripGtk::Layout() {
  GenerateIdealBounds();

  for (std::vector<TabData>::iterator iter = tab_data_.begin();
       iter != tab_data_.end(); ++iter) {
    LayoutTab(&*iter);
  }
}

void TabStripGtk::DrawImageInt(GdkPixbuf* pixbuf, int x, int y) {
  GdkGC* gc = tabstrip_.get()->style->fg_gc[GTK_WIDGET_STATE(tabstrip_.get())];

  gdk_draw_pixbuf(tabstrip_.get()->window,  // The destination drawable.
                  gc,  // Graphics context.
                  pixbuf,  // Source image.
                  0, 0,  // Source x, y.
                  x, y, -1, -1,  // Destination x, y, width, height.
                  GDK_RGB_DITHER_NONE, 0, 0);  // Dithering mode, x,y offsets.
}

void TabStripGtk::TileImageInt(GdkPixbuf* pixbuf,
                               int x, int y, int w, int h) {
  GdkGC* gc = tabstrip_.get()->style->fg_gc[GTK_WIDGET_STATE(tabstrip_.get())];
  int image_width = gdk_pixbuf_get_width(pixbuf);
  int slices = w / image_width;
  int remaining = w - slices * image_width;

  for (int i = 0; i < slices; i++) {
    gdk_draw_pixbuf(tabstrip_.get()->window, gc,
                    pixbuf, 0, 0, x + image_width * i, y, -1, -1,
                    GDK_RGB_DITHER_NONE, 0, 0);
  }

  if (remaining) {
    gdk_draw_pixbuf(tabstrip_.get()->window, gc,
                    pixbuf, 0, 0, x + image_width * slices, y, remaining, -1,
                    GDK_RGB_DITHER_NONE, 0, 0);
  }
}

void TabStripGtk::PaintTab(int index, bool selected) {
  GdkGC* gc = tabstrip_.get()->style->fg_gc[GTK_WIDGET_STATE(tabstrip_.get())];
  TabImage& image = (selected) ? tab_active_ : tab_inactive_;
  TabData& data = tab_data_.at(index);
  gfx::Rect bounds = data.bounds;

  DrawImageInt(image.image_l, bounds.x(), bounds.y());

  TileImageInt(image.image_c, bounds.x() + image.l_width, bounds.y(),
               bounds.width() - image.l_width - image.r_width,
               gdk_pixbuf_get_height(image.image_c));

  DrawImageInt(image.image_r,
               bounds.x() + bounds.width() - image.r_width, bounds.y());

  if (data.show_icon && !data.favicon.empty()) {
    GdkPixbuf* favicon = gfx::GdkPixbufFromSkBitmap(&data.favicon);
    DrawImageInt(favicon, bounds.x() + image.l_width, kTopPadding);
  }

  std::wstring title = data.title;
  if (title.empty()) {
    if (data.loading) {
      title = l10n_util::GetString(IDS_TAB_LOADING_TITLE);
    } else {
      title = l10n_util::GetString(IDS_TAB_UNTITLED_TITLE);
    }
  } else {
    Browser::FormatTitleForDisplay(&title);
  }

  if (selected) {
    gdk_gc_set_rgb_fg_color(gc, &kSelectedTitleColor);
  } else {
    gdk_gc_set_rgb_fg_color(gc, &kUnselectedTitleColor);
  }

  // TODO(jhawkins): Clip the title.
  gdk_draw_text(tabstrip_.get()->window, title_font_, gc,
                data.title_bounds.x(), data.title_bounds.y(),
                WideToUTF8(title).c_str(), title.length());

  DrawImageInt(close_button_.normal,
               data.close_button_bounds.x(), data.close_button_bounds.y());
}

////////////////////////////////////////////////////////////////////////////////
// TabStrip, private:

size_t TabStripGtk::GetTabCount() const {
  return tab_data_.size();
}

void TabStripGtk::RemoveTabAt(int index) {
  tab_data_.erase(tab_data_.begin() + index);
}

void TabStripGtk::GenerateIdealBounds() {
  size_t tab_count = GetTabCount();
  double unselected, selected;
  GetDesiredTabWidths(tab_count, &unselected, &selected);

  int tab_height = GetStandardSize().height();
  double tab_x = 0;
  int selected_index = model_->selected_index();

  std::vector<TabData>::iterator iter = tab_data_.begin();
  for (int i = 0; iter != tab_data_.end(); ++iter, ++i) {
    double tab_width = unselected;
    if (i == selected_index)
      tab_width = selected;
    double end_of_tab = tab_x + tab_width;
    int rounded_tab_x = Round(tab_x);
    gfx::Rect state(rounded_tab_x, 0, Round(end_of_tab) - rounded_tab_x,
                    tab_height);
    iter->bounds = state;
    tab_x = end_of_tab + kTabHOffset;
  }
}

// static
int TabStripGtk::GetContentHeight() {
  // The height of the content of the Tab is the largest of the favicon,
  // the title text and the close button graphic.
  int content_height = std::max(kFavIconSize, title_font_height_);
  return std::max(content_height, close_button_.height);
}

// static
gfx::Size TabStripGtk::GetMinimumUnselectedSize() {
  InitResources();

  gfx::Size minimum_size;
  minimum_size.set_width(kLeftPadding + kRightPadding);
  // Since we use bitmap images, the real minimum height of the image is
  // defined most accurately by the height of the end cap images.
  minimum_size.set_height(gdk_pixbuf_get_height(tab_active_.image_l));
  return minimum_size;
}

// static
gfx::Size TabStripGtk::GetMinimumSelectedSize() {
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  minimum_size.set_width(kLeftPadding + kFavIconSize + kRightPadding);
  return minimum_size;
}

// static
gfx::Size TabStripGtk::GetStandardSize() {
  gfx::Size standard_size = GetMinimumUnselectedSize();
  standard_size.set_width(
      standard_size.width() + kFavIconTitleSpacing + kStandardTitleWidth);
  return standard_size;
}

void TabStripGtk::GetDesiredTabWidths(size_t tab_count,
                                      double* unselected_width,
                                      double* selected_width) const {
  const double min_unselected_width = GetMinimumUnselectedSize().width();
  const double min_selected_width = GetMinimumSelectedSize().width();
  if (tab_count == 0) {
    // Return immediately to avoid divide-by-zero below.
    *unselected_width = min_unselected_width;
    *selected_width = min_selected_width;
    return;
  }

  // Determine how much space we can actually allocate to tabs.
  int available_width = tabstrip_.get()->allocation.width;
  available_width -= (kNewTabButtonHOffset + newtab_button_.width);

  // Calculate the desired tab widths by dividing the available space into equal
  // portions.  Don't let tabs get larger than the "standard width" or smaller
  // than the minimum width for each type, respectively.
  const int total_offset = kTabHOffset * (tab_count - 1);
  const double desired_tab_width = std::min((static_cast<double>(
      available_width - total_offset) / static_cast<double>(tab_count)),
      static_cast<double>(GetStandardSize().width()));
  *unselected_width = std::max(desired_tab_width, min_unselected_width);
  *selected_width = std::max(desired_tab_width, min_selected_width);

  // When there are multiple tabs, we'll have one selected and some unselected
  // tabs.  If the desired width was between the minimum sizes of these types,
  // try to shrink the tabs with the smaller minimum.  For example, if we have
  // a strip of width 10 with 4 tabs, the desired width per tab will be 2.5.  If
  // selected tabs have a minimum width of 4 and unselected tabs have a minimum
  // width of 1, the above code would set *unselected_width = 2.5,
  // *selected_width = 4, which results in a total width of 11.5.  Instead, we
  // want to set *unselected_width = 2, *selected_width = 4, for a total width
  // of 10.
  if (tab_count > 1) {
    if ((min_unselected_width < min_selected_width) &&
        (desired_tab_width < min_selected_width)) {
      *unselected_width = std::max(static_cast<double>(
          available_width - total_offset - min_selected_width) /
          static_cast<double>(tab_count - 1), min_unselected_width);
    } else if ((min_unselected_width > min_selected_width) &&
               (desired_tab_width < min_unselected_width)) {
      *selected_width = std::max(available_width - total_offset -
          (min_unselected_width * (tab_count - 1)), min_selected_width);
    }
  }
}

void TabStripGtk::UpdateTabData(TabContents* contents, TabData* tab) {
  tab->favicon = contents->GetFavIcon();
  tab->show_icon = contents->ShouldDisplayFavIcon();
  tab->show_download_icon = contents->IsDownloadShelfVisible();
  tab->title = UTF16ToWideHack(contents->GetTitle());
  tab->loading = contents->is_loading();
}

// static
gboolean TabStripGtk::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                               TabStripGtk* tabstrip) {
  TabStripModel* model = tabstrip->model();
  int selected = model->selected_index();

  // TODO(jhawkins): Move the Layout call out of OnExpose and into methods
  // that actually affect the layout.
  tabstrip->Layout();

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* background = rb.LoadPixbuf(IDR_WINDOW_TOP_CENTER);
  tabstrip->TileImageInt(background, 0, 0,
                         tabstrip->tabstrip_.get()->allocation.width,
                         tabstrip->tabstrip_.get()->allocation.height);

  if (model->count() == 0)
    return TRUE;

  for (int i = 0; i < model->count(); i++) {
    if (i != selected) {
      tabstrip->PaintTab(i, false);
    }
  }

  tabstrip->PaintTab(selected, true);

  return TRUE;
}

// static
void TabStripGtk::LoadTabImages() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  tab_active_.image_l = rb.LoadPixbuf(IDR_TAB_ACTIVE_LEFT);
  tab_active_.image_c = rb.LoadPixbuf(IDR_TAB_ACTIVE_CENTER);
  tab_active_.image_r = rb.LoadPixbuf(IDR_TAB_ACTIVE_RIGHT);
  tab_active_.l_width = gdk_pixbuf_get_width(tab_active_.image_l);
  tab_active_.r_width = gdk_pixbuf_get_width(tab_active_.image_r);

  tab_inactive_.image_l = rb.LoadPixbuf(IDR_TAB_INACTIVE_LEFT);
  tab_inactive_.image_c = rb.LoadPixbuf(IDR_TAB_INACTIVE_CENTER);
  tab_inactive_.image_r = rb.LoadPixbuf(IDR_TAB_INACTIVE_RIGHT);
  tab_inactive_.l_width = gdk_pixbuf_get_width(tab_inactive_.image_l);
  tab_inactive_.r_width = gdk_pixbuf_get_width(tab_inactive_.image_r);

  tab_hover_.image_l = rb.LoadPixbuf(IDR_TAB_HOVER_LEFT);
  tab_hover_.image_c = rb.LoadPixbuf(IDR_TAB_HOVER_CENTER);
  tab_hover_.image_r = rb.LoadPixbuf(IDR_TAB_HOVER_RIGHT);

  tab_inactive_otr_.image_l = rb.LoadPixbuf(IDR_TAB_INACTIVE_LEFT_OTR);
  tab_inactive_otr_.image_c = rb.LoadPixbuf(IDR_TAB_INACTIVE_CENTER_OTR);
  tab_inactive_otr_.image_r = rb.LoadPixbuf(IDR_TAB_INACTIVE_RIGHT_OTR);

  // tab_[hover,inactive_otr] width are not used and are initialized to 0
  // during static initialization.

  close_button_.normal = rb.LoadPixbuf(IDR_TAB_CLOSE);
  close_button_.hot = rb.LoadPixbuf(IDR_TAB_CLOSE_H);
  close_button_.pushed = rb.LoadPixbuf(IDR_TAB_CLOSE_P);
  close_button_.width = gdk_pixbuf_get_width(close_button_.normal);
  close_button_.height = gdk_pixbuf_get_height(close_button_.normal);

  newtab_button_.normal = rb.LoadPixbuf(IDR_NEWTAB_BUTTON);
  newtab_button_.hot = rb.LoadPixbuf(IDR_NEWTAB_BUTTON_H);
  newtab_button_.pushed = rb.LoadPixbuf(IDR_NEWTAB_BUTTON_P);
  newtab_button_.width = gdk_pixbuf_get_width(newtab_button_.normal);
  newtab_button_.height = gdk_pixbuf_get_height(newtab_button_.normal);

  download_icon_ = rb.LoadPixbuf(IDR_DOWNLOAD_ICON);
  download_icon_width_ = gdk_pixbuf_get_width(download_icon_);
  download_icon_height_ = gdk_pixbuf_get_height(download_icon_);
}

// static
void TabStripGtk::InitResources() {
  if (initialized_)
    return;

  LoadTabImages();

  // TODO(jhawkins): Move this into ChromeFont.  Also, my default gtk font
  // is really ugly compared to the other fonts being used in the UI.
  title_font_ = load_default_font();
  DCHECK(title_font_);
  title_font_height_ = gdk_char_height(title_font_, 'X');
  initialized_ = true;
}
