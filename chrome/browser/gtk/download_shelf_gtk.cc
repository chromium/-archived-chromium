// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_shelf_gtk.h"

#include "base/logging.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/gtk/download_item_gtk.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

// TODO(port): remove this after tab_contents.h is ported.
#include "chrome/common/temp_scaffolding_stubs.h"

namespace {

// Total height of the shelf.
const int kShelfHeight = 42;

// Padding between the download widgets.
const int kDownloadItemPadding = 10;

// Padding between the top/bottom of the download widgets and the edge of the
// shelf.
const int kTopBottomPadding = 2;

// Padding from right edge and close button/show downloads link.
const int kRightPadding = 10;

}

// static
DownloadShelf* DownloadShelf::Create(TabContents* tab_contents) {
  return new DownloadShelfGtk(tab_contents);
}

DownloadShelfGtk::DownloadShelfGtk(TabContents* tab_contents)
    : DownloadShelf(tab_contents),
      is_showing_(false) {
  shelf_ = gtk_hbox_new(FALSE, 0);
  gtk_widget_set_size_request(shelf_, -1, kShelfHeight);
  gtk_container_set_border_width(GTK_CONTAINER(shelf_), kTopBottomPadding);

  // Create and pack the close button.
  close_button_.reset(new CustomDrawButton(IDR_CLOSE_BAR,
                      IDR_CLOSE_BAR_P, IDR_CLOSE_BAR_H, 0));
  g_signal_connect(G_OBJECT(close_button_->widget()), "clicked",
                   G_CALLBACK(OnCloseButtonClick), this);
  GTK_WIDGET_UNSET_FLAGS(close_button_->widget(), GTK_CAN_FOCUS);
  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), close_button_->widget(), TRUE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(shelf_), vbox, FALSE, FALSE, kRightPadding);

  // Stick ourselves at the bottom of the parent tab contents.
  GtkWidget* parent_contents = tab_contents->GetNativeView();
  gtk_box_pack_end(GTK_BOX(parent_contents), shelf_, FALSE, FALSE, 0);
  Show();
}

void DownloadShelfGtk::AddDownload(BaseDownloadItemModel* download_model_) {
  // TODO(estade): we need to delete these at some point. There's no explicit
  // mass delete on windows, figure out where they do it.
  download_items_.push_back(new DownloadItemGtk(download_model_, shelf_));
  Show();
}

bool DownloadShelfGtk::IsShowing() const {
  return is_showing_;
}

void DownloadShelfGtk::Show() {
  if (is_showing_)
    return;

  gtk_widget_show_all(shelf_);
  is_showing_ = true;
}

void DownloadShelfGtk::Hide() {
  if (!is_showing_)
    return;

  gtk_widget_hide_all(shelf_);
  is_showing_ = false;
}

// static
void DownloadShelfGtk::OnCloseButtonClick(GtkWidget* button,
                                          DownloadShelfGtk* shelf) {
  shelf->Hide();
}

