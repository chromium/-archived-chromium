// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_shelf_gtk.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/gtk_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/download_item_gtk.h"
#include "chrome/browser/gtk/link_button_gtk.h"
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

// The background color of the shelf.
const GdkColor kBackgroundColor = GDK_COLOR_RGB(230, 237, 244);

// Border color (the top pixel of the shelf).
const GdkColor kBorderColor = GDK_COLOR_RGB(214, 214, 214);

// Speed of the shelf show/hide animation.
const int kShelfAnimationDurationMs = 120;

}  // namespace

// static
DownloadShelf* DownloadShelf::Create(TabContents* tab_contents) {
  return new DownloadShelfGtk(tab_contents);
}

DownloadShelfGtk::DownloadShelfGtk(TabContents* tab_contents)
    : DownloadShelf(tab_contents),
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
  hbox_ = gtk_hbox_new(FALSE, kDownloadItemPadding);
  gtk_widget_set_size_request(hbox_, -1, kDownloadItemHeight);

  // Get the padding and background color for |hbox_| right.
  GtkWidget* padding = gtk_alignment_new(0, 0, 1, 1);
  // Subtract 1 from top spacing to account for top border.
  gtk_alignment_set_padding(GTK_ALIGNMENT(padding),
      kTopBottomPadding - 1, kTopBottomPadding, kLeftPadding, kRightPadding);
  GtkWidget* padding_bg = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(padding_bg), padding);
  gtk_container_add(GTK_CONTAINER(padding), hbox_);
  gtk_widget_modify_bg(padding_bg, GTK_STATE_NORMAL, &kBackgroundColor);

  shelf_.Own(gtk_vbox_new(FALSE, 0));
  gtk_box_pack_start(GTK_BOX(shelf_.get()), top_border, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(shelf_.get()), padding_bg, FALSE, FALSE, 0);

  // Create and pack the close button.
  close_button_.reset(CustomDrawButton::CloseButton());
  gtk_util::CenterWidgetInHBox(hbox_, close_button_->widget(), true, 0);
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);

  // Create the "Show all downloads..." link and connect to the click event.
  std::string link_text =
      l10n_util::GetStringUTF8(IDS_SHOW_ALL_DOWNLOADS);
  link_button_.reset(new LinkButtonGtk(link_text.c_str()));
  g_signal_connect(link_button_->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);
  // Until we switch to vector graphics, force the font size.
  // 13.4px == 10pt @ 96dpi
  gtk_util::ForceFontSizePixels(link_button_->label(), 13.4);

  // Make the download arrow icon.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* download_pixbuf = rb.GetPixbufNamed(IDR_DOWNLOADS_FAVICON);
  GtkWidget* download_image = gtk_image_new_from_pixbuf(download_pixbuf);

  // Pack the link and the icon in an hbox.
  link_hbox_ = gtk_hbox_new(FALSE, 5);
  gtk_util::CenterWidgetInHBox(link_hbox_, download_image, false, 0);
  gtk_util::CenterWidgetInHBox(link_hbox_, link_button_->widget(), false, 0);
  gtk_box_pack_end(GTK_BOX(hbox_), link_hbox_, FALSE, FALSE, 0);

  slide_widget_.reset(new SlideAnimatorGtk(shelf_.get(),
                                           SlideAnimatorGtk::UP,
                                           kShelfAnimationDurationMs,
                                           false, NULL));
  gtk_widget_show_all(shelf_.get());
  // Stick ourselves at the bottom of the parent tab contents.
  GtkWidget* parent_contents = tab_contents->GetNativeView();
  gtk_box_pack_end(GTK_BOX(parent_contents), slide_widget_->widget(),
                   FALSE, FALSE, 0);
  slide_widget_->Open();
}

DownloadShelfGtk::~DownloadShelfGtk() {
  for (std::vector<DownloadItemGtk*>::iterator iter = download_items_.begin();
       iter != download_items_.end(); ++iter) {
    delete *iter;
  }

  shelf_.Destroy();
}

void DownloadShelfGtk::AddDownload(BaseDownloadItemModel* download_model_) {
  download_items_.push_back(new DownloadItemGtk(this, download_model_));
  slide_widget_->Open();
}

bool DownloadShelfGtk::IsShowing() const {
  return slide_widget_->IsShowing();
}

bool DownloadShelfGtk::IsClosing() const {
  return slide_widget_->IsClosing();
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
    tab_contents_->SetDownloadShelfVisible(false);
  }
}

GtkWidget* DownloadShelfGtk::GetRightBoundingWidget() const {
  return link_hbox_;
}

GtkWidget* DownloadShelfGtk::GetHBox() const {
  return hbox_;
}

// static
void DownloadShelfGtk::OnButtonClick(GtkWidget* button,
                                     DownloadShelfGtk* shelf) {
  if (button == shelf->close_button_->widget()) {
    shelf->slide_widget_->Close();
    shelf->tab_contents_->SetDownloadShelfVisible(false);
  } else {
    // The link button was clicked.
    shelf->ShowAllDownloads();
  }
}
