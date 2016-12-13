// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_shelf_gtk.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/gtk_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/download_item_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/slide_animator_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// The height of the download items.
const int kDownloadItemHeight = download_util::kSmallProgressIconSize;

// Padding between the download widgets.
const int kDownloadItemPadding = 10;

// Padding between the top/bottom of the download widgets and the edge of the
// shelf.
const int kTopBottomPadding = 4;

// Padding between the left side of the shelf and the first download item.
const int kLeftPadding = 2;

// Padding between the right side of the shelf and the close button.
const int kRightPadding = 10;

// Border color (the top pixel of the shelf).
const GdkColor kBorderColor = GDK_COLOR_RGB(214, 214, 214);

// Speed of the shelf show/hide animation.
const int kShelfAnimationDurationMs = 120;

}  // namespace

DownloadShelfGtk::DownloadShelfGtk(Browser* browser, GtkWidget* parent)
    : DownloadShelf(browser),
      is_showing_(false) {
  // Logically, the shelf is a vbox that contains two children: a one pixel
  // tall event box, which serves as the top border, and an hbox, which holds
  // the download items and other shelf widgets (close button, show-all-
  // downloads link).
  // To make things pretty, we have to add a few more widgets. To get padding
  // right, we stick the hbox in an alignment. We put that alignment in an
  // event box so we can color the background.

  // Create the top border.
  GtkWidget* top_border = gtk_event_box_new();
  gtk_widget_set_size_request(GTK_WIDGET(top_border), 0, 1);
  gtk_widget_modify_bg(top_border, GTK_STATE_NORMAL, &kBorderColor);

  // Create |hbox_|.
  hbox_.Own(gtk_hbox_new(FALSE, kDownloadItemPadding));
  gtk_widget_set_size_request(hbox_.get(), -1, kDownloadItemHeight);

  // Get the padding and background color for |hbox_| right.
  GtkWidget* padding = gtk_alignment_new(0, 0, 1, 1);
  // Subtract 1 from top spacing to account for top border.
  gtk_alignment_set_padding(GTK_ALIGNMENT(padding),
      kTopBottomPadding - 1, kTopBottomPadding, kLeftPadding, kRightPadding);
  padding_bg_ = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(padding_bg_), padding);
  gtk_container_add(GTK_CONTAINER(padding), hbox_.get());

  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), top_border, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), padding_bg_, FALSE, FALSE, 0);

  // Put the shelf in an event box so it gets its own window, which makes it
  // easier to get z-ordering right.
  shelf_.Own(gtk_event_box_new());
  gtk_container_add(GTK_CONTAINER(shelf_.get()), vbox);

  // Create and pack the close button.
  close_button_.reset(CustomDrawButton::CloseButton());
  gtk_util::CenterWidgetInHBox(hbox_.get(), close_button_->widget(), true, 0);
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);

  // Create the "Show all downloads..." link and connect to the click event.
  std::string link_text =
      l10n_util::GetStringUTF8(IDS_SHOW_ALL_DOWNLOADS);
  GtkWidget* link_button = gtk_chrome_link_button_new(link_text.c_str());
  g_signal_connect(link_button, "clicked",
                   G_CALLBACK(OnButtonClick), this);
  // Until we switch to vector graphics, force the font size.
  // 13.4px == 10pt @ 96dpi
  gtk_util::ForceFontSizePixels(GTK_CHROME_LINK_BUTTON(link_button)->label,
                                13.4);

  // Make the download arrow icon.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* download_pixbuf = rb.GetPixbufNamed(IDR_DOWNLOADS_FAVICON);
  GtkWidget* download_image = gtk_image_new_from_pixbuf(download_pixbuf);

  // Pack the link and the icon in an hbox.
  link_hbox_ = gtk_hbox_new(FALSE, 5);
  gtk_util::CenterWidgetInHBox(link_hbox_, download_image, false, 0);
  gtk_util::CenterWidgetInHBox(link_hbox_, link_button, false, 0);
  gtk_box_pack_end(GTK_BOX(hbox_.get()), link_hbox_, FALSE, FALSE, 0);

  slide_widget_.reset(new SlideAnimatorGtk(shelf_.get(),
                                           SlideAnimatorGtk::UP,
                                           kShelfAnimationDurationMs,
                                           false, NULL));

  GtkThemeProperties properties(browser->profile());
  UserChangedTheme(&properties);

  gtk_widget_show_all(shelf_.get());

  // Stick ourselves at the bottom of the parent browser.
  gtk_box_pack_end(GTK_BOX(parent), slide_widget_->widget(),
                   FALSE, FALSE, 0);
  slide_widget_->Open();
}

DownloadShelfGtk::~DownloadShelfGtk() {
  for (std::vector<DownloadItemGtk*>::iterator iter = download_items_.begin();
       iter != download_items_.end(); ++iter) {
    delete *iter;
  }

  shelf_.Destroy();
  hbox_.Destroy();
}

void DownloadShelfGtk::AddDownload(BaseDownloadItemModel* download_model_) {
  download_items_.push_back(new DownloadItemGtk(this, download_model_));
  Show();
}

bool DownloadShelfGtk::IsShowing() const {
  return slide_widget_->IsShowing();
}

bool DownloadShelfGtk::IsClosing() const {
  return slide_widget_->IsClosing();
}

void DownloadShelfGtk::Show() {
  slide_widget_->Open();
}

void DownloadShelfGtk::Close() {
  // When we are closing, we can vertically overlap the render view. Make sure
  // we are on top.
  gdk_window_raise(shelf_.get()->window);
  slide_widget_->Close();

  // TODO(estade): Remove. The status bubble should query its window instead.
  browser_->UpdateDownloadShelfVisibility(false);
}

int DownloadShelfGtk::GetHeight() const {
  return slide_widget_->widget()->allocation.height;
}

void DownloadShelfGtk::UserChangedTheme(GtkThemeProperties* properties) {
  GdkColor color = properties->GetGdkColor(BrowserThemeProvider::COLOR_TOOLBAR);
  gtk_widget_modify_bg(padding_bg_, GTK_STATE_NORMAL, &color);

  for (std::vector<DownloadItemGtk*>::iterator it = download_items_.begin();
       it != download_items_.end(); ++it) {
    (*it)->UserChangedTheme(properties);
  }
}

void DownloadShelfGtk::RemoveDownloadItem(DownloadItemGtk* download_item) {
  DCHECK(download_item);
  std::vector<DownloadItemGtk*>::iterator i =
      find(download_items_.begin(), download_items_.end(), download_item);
  DCHECK(i != download_items_.end());
  download_items_.erase(i);
  delete download_item;
  if (download_items_.empty()) {
    slide_widget_->CloseWithoutAnimation();

    // TODO(estade): Remove. The status bubble should query its window instead.
    browser_->UpdateDownloadShelfVisibility(false);
  }
}

GtkWidget* DownloadShelfGtk::GetRightBoundingWidget() const {
  return link_hbox_;
}

GtkWidget* DownloadShelfGtk::GetHBox() const {
  return hbox_.get();
}

// static
void DownloadShelfGtk::OnButtonClick(GtkWidget* button,
                                     DownloadShelfGtk* shelf) {
  if (button == shelf->close_button_->widget()) {
    shelf->Close();
  } else {
    // The link button was clicked.
    shelf->ShowAllDownloads();
  }
}
