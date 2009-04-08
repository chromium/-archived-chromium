// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/infobar_gtk.h"

#include <gtk/gtk.h>

#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/infobar_container_gtk.h"

namespace {

// TODO(estade): The background should be a gradient. For now we just use this
// solid color.
const GdkColor kBackgroundColor = GDK_COLOR_RGB(250, 230, 145);

// Border color (the top pixel of the infobar).
const GdkColor kBorderColor = GDK_COLOR_RGB(0xbe, 0xc8, 0xd4);

// The total height of the info bar.
const int kInfoBarHeight = 37;

// Pixels between infobar elements.
const int kElementPadding = 5;

// Extra padding on either end of info bar.
const int kLeftPadding = 5;
const int kRightPadding = 5;

}

InfoBar::InfoBar(InfoBarDelegate* delegate)
    : container_(NULL),
      delegate_(delegate) {
  // Create |hbox_| and pad the sides.
  hbox_ = gtk_hbox_new(FALSE, kElementPadding);
  GtkWidget* padding = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(padding),
      0, 0, kLeftPadding, kRightPadding);

  GtkWidget* bg_box = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(padding), hbox_);
  gtk_container_add(GTK_CONTAINER(bg_box), padding);

  // Set the top border and background color.
  gtk_widget_modify_bg(bg_box, GTK_STATE_NORMAL, &kBackgroundColor);
  widget_.Own(gfx::CreateGtkBorderBin(bg_box, &kBorderColor,
                                      1, 0, 0, 0));
  gtk_widget_set_size_request(widget_.get(), -1, kInfoBarHeight);

  close_button_.reset(CustomDrawButton::AddBarCloseButton(hbox_));
  g_signal_connect(G_OBJECT(close_button_->widget()), "clicked",
                   G_CALLBACK(OnCloseButton), this);

  g_object_set_data(G_OBJECT(widget_.get()), "info-bar", this);
}

InfoBar::~InfoBar() {
  widget_.Destroy();
}

void InfoBar::AnimateOpen() {
  // TODO(port): add animations. In the meantime just Open().
  NOTIMPLEMENTED();
  Open();
}

void InfoBar::Open() {
  gtk_widget_show_all(widget_.get());
}

void InfoBar::AnimateClose() {
  // TODO(port): add animations. In the meantime just Close().
  NOTIMPLEMENTED();
  Close();
}

void InfoBar::Close() {
  gtk_widget_hide(widget_.get());
}

void InfoBar::RemoveInfoBar() const {
  container_->RemoveDelegate(delegate_);
}

// static
void InfoBar::OnCloseButton(GtkWidget* button, InfoBar* info_bar) {
  info_bar->AnimateClose();
}

// AlertInfoBar ----------------------------------------------------------------

class AlertInfoBar : public InfoBar {
 public:
  AlertInfoBar(AlertInfoBarDelegate* delegate)
      : InfoBar(delegate) {
    std::wstring text = delegate->GetMessageText();
    GtkWidget* label = gtk_label_new(WideToUTF8(text).c_str());

    SkBitmap* icon = delegate->GetIcon();
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(icon);
    GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
    gdk_pixbuf_unref(pixbuf);

    gtk_box_pack_start(GTK_BOX(hbox_), image, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_), label, FALSE, FALSE, 0);
  }
};

// LinkInfoBar -----------------------------------------------------------------

class LinkInfoBar : public InfoBar {
 public:
  LinkInfoBar(LinkInfoBarDelegate* delegate)
      : InfoBar(delegate) {
    // TODO(estade): remove these lines.
    NOTIMPLEMENTED();
    GtkWidget* label = gtk_label_new("LinkInfoBar not yet implemented. "
                                     "Check back later.");
    gtk_box_pack_start(GTK_BOX(hbox_), label, FALSE, FALSE, 10);
  }
};

// ConfirmInfoBar --------------------------------------------------------------

class ConfirmInfoBar : public AlertInfoBar {
 public:
  ConfirmInfoBar(ConfirmInfoBarDelegate* delegate)
      : AlertInfoBar(delegate) {
    NOTIMPLEMENTED();
  }
};

// AlertInfoBarDelegate, InfoBarDelegate overrides: ----------------------------

InfoBar* AlertInfoBarDelegate::CreateInfoBar() {
  return new AlertInfoBar(this);
}

// LinkInfoBarDelegate, InfoBarDelegate overrides: -----------------------------

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  return new LinkInfoBar(this);
}

// ConfirmInfoBarDelegate, InfoBarDelegate overrides: --------------------------

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  return new ConfirmInfoBar(this);
}
