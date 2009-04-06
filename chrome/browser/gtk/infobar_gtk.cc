// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/infobar_gtk.h"

#include <gtk/gtk.h>

#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/infobar_container_gtk.h"

InfoBar::InfoBar(InfoBarDelegate* delegate)
    : widget_(gtk_hbox_new(FALSE, 0)),
      container_(NULL),
      delegate_(delegate) {
  g_object_set_data(G_OBJECT(widget_.get()), "info-bar", this);
  close_button_.reset(CustomDrawButton::AddBarCloseButton(widget_.get()));

  // TODO(estade): remove these lines.
  GtkWidget* label = gtk_label_new("Infobars not yet implemented. "
                                   "Check back later.");
  gtk_box_pack_start(GTK_BOX(widget_.get()), label, FALSE, FALSE, 10);
  gtk_widget_set_size_request(widget_.get(), -1, 40);
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

// AlertInfoBarDelegate, InfoBarDelegate overrides: ----------------------------

InfoBar* AlertInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return new InfoBar(this);
}

// LinkInfoBarDelegate, InfoBarDelegate overrides: -----------------------------

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return new InfoBar(this);
}

// ConfirmInfoBarDelegate, InfoBarDelegate overrides: --------------------------

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return new InfoBar(this);
}
