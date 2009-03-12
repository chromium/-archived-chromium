// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_shelf_gtk.h"

#include "base/gfx/gtk_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/download_item_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// The height of the download items. Should be 28, as that is the height of
// their bitmaps.
const int kDownloadItemHeight = 28;

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
static GdkColor kBackgroundColor = GDK_COLOR_RGB(230, 237, 244);

// Border color (the top pixel of the shelf).
static GdkColor kBorderColor = GDK_COLOR_RGB(214, 214, 214);

const char* kLinkMarkup =
    "<u><span color=\"blue\">%s</span></u>";

gboolean OnLinkExpose(GtkWidget* widget, GdkEventExpose* e, void*) {
  // Draw the link inside the button.
  gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                 gtk_bin_get_child(GTK_BIN(widget)),
                                 e);
  // Don't let the button draw itself, ever.
  return TRUE;
}

// |button| and |box| are out parameters. The caller of this function will want
// to connect to the click event on |button|. |box| will be set to the highest
// level widget.
// TODO(estade): either figure out a way to use GtkLinkButton, or move this
// to base/gfx/gtk_util.cc
void MakeLinkButton(const char* text, GdkColor* background_color,
                    GtkWidget** button, GtkWidget** box) {
  // We put a label in a button so we can connect to the click event. We put the
  // button in an event box so we can attach a cursor to it. We don't let the
  // button draw itself; catch all expose events to the button and pass them
  // through to the label. We stick the event box in an hbox, and to the left of
  // that pack the download icon.
  // TODO(estade): the link should turn red during the user's click.

  GtkWidget* label = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(kLinkMarkup, text);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);

  *button = gtk_button_new();
  gtk_widget_set_app_paintable(GTK_WIDGET(*button), TRUE);
  g_signal_connect(G_OBJECT(*button), "expose-event",
                   G_CALLBACK(OnLinkExpose), NULL);
  gtk_container_add(GTK_CONTAINER(*button), label);

  *box = gtk_event_box_new();
  gtk_widget_modify_bg(*box, GTK_STATE_NORMAL, background_color);
  gtk_container_add(GTK_CONTAINER(*box), *button);
}

// This should be called only after |link_box| has been realized.
void AttachCursorToLinkButton(GtkWidget* link_box) {
  GdkCursor* cursor = gdk_cursor_new(GDK_HAND2);
  gdk_window_set_cursor(link_box->window, cursor);
  gdk_cursor_unref(cursor);
}

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

  shelf_ = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(shelf_), top_border, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(shelf_), padding_bg, FALSE, FALSE, 0);

  // Create and pack the close button.
  close_button_.reset(new CustomDrawButton(IDR_CLOSE_BAR,
                      IDR_CLOSE_BAR_P, IDR_CLOSE_BAR_H, 0));
  g_signal_connect(G_OBJECT(close_button_->widget()), "clicked",
                   G_CALLBACK(OnButtonClick), this);
  GTK_WIDGET_UNSET_FLAGS(close_button_->widget(), GTK_CAN_FOCUS);
  GtkWidget* centering_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(centering_vbox),
                     close_button_->widget(), TRUE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(hbox_), centering_vbox, FALSE, FALSE, 0);

  // Create and pack the "Show all downloads..." link.
  // TODO(estade): there are some pixels above and below the link that
  // can be clicked. I tried to fix this with a vbox (akin to |centering_vbox|
  // above), but no dice.
  GtkWidget* link_box;
  GtkWidget* link_button;
  std::string link_text =
      WideToUTF8(l10n_util::GetString(IDS_SHOW_ALL_DOWNLOADS));
  MakeLinkButton(link_text.c_str(), &kBackgroundColor, &link_button, &link_box);
  g_signal_connect(G_OBJECT(link_button), "clicked",
                   G_CALLBACK(OnButtonClick), this);

  // Make the download arrow icon.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* download_pixbuf = rb.LoadPixbuf(IDR_DOWNLOADS_FAVICON);
  GtkWidget* download_image = gtk_image_new_from_pixbuf(download_pixbuf);
  gdk_pixbuf_unref(download_pixbuf);

  // Pack the link and the icon in an hbox.
  GtkWidget* link_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(link_hbox), download_image, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(link_hbox), link_box, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(hbox_), link_hbox, FALSE, FALSE, 0);

  // Stick ourselves at the bottom of the parent tab contents.
  GtkWidget* parent_contents = tab_contents->GetNativeView();
  gtk_box_pack_end(GTK_BOX(parent_contents), shelf_, FALSE, FALSE, 0);
  Show();

  AttachCursorToLinkButton(link_box);
}

DownloadShelfGtk::~DownloadShelfGtk() {
}

void DownloadShelfGtk::AddDownload(BaseDownloadItemModel* download_model_) {
  // TODO(estade): we need to delete these at some point. There's no explicit
  // mass delete on windows, figure out where they do it.
  download_items_.push_back(new DownloadItemGtk(download_model_, hbox_));
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
void DownloadShelfGtk::OnButtonClick(GtkWidget* button,
                                     DownloadShelfGtk* shelf) {
  if (button == shelf->close_button_->widget()) {
    shelf->Hide();
  } else {
    // The link button was clicked.
    shelf->ShowAllDownloads();
  }
}
