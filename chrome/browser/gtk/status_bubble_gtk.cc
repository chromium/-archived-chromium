// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/status_bubble_gtk.h"

#include "base/string_util.h"
#include "googleurl/src/gurl.h"

// NOTE: this code is probably the wrong approach for the status bubble.
// Talk to evanm before you attempt to fix bugs in it -- we're probably
// better off restructuring it.

StatusBubbleGtk::StatusBubbleGtk(GtkWindow* parent)
    : parent_(parent), window_(NULL) {
}

StatusBubbleGtk::~StatusBubbleGtk() {
  Hide();
}

void StatusBubbleGtk::SetStatus(const std::string& status) {
  if (status.empty()) {
    Hide();
    return;
  }

  if (!window_)
    Create();

  gtk_label_set_text(GTK_LABEL(label_), status.c_str());
  Reposition();
  gtk_widget_show(window_);
}

void StatusBubbleGtk::SetStatus(const std::wstring& status) {
  SetStatus(WideToUTF8(status));
}

void StatusBubbleGtk::SetURL(const GURL& url, const std::wstring& languages) {
  SetStatus(url.spec());
}

void StatusBubbleGtk::Hide() {
  if (!window_)
    return;
  gtk_widget_destroy(window_);
  window_ = NULL;
}

void StatusBubbleGtk::MouseMoved() {
  if (!window_)
    return;
  // We ignore for now.
  // TODO(port): the fancy sliding behavior.
}

void StatusBubbleGtk::Create() {
  if (window_)
    return;

  window_ = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_window_set_transient_for(GTK_WINDOW(window_), parent_);
  gtk_container_set_border_width(GTK_CONTAINER(window_), 2);
  label_ = gtk_label_new("");
  gtk_widget_show(label_);
  gtk_container_add(GTK_CONTAINER(window_), label_);
}

void StatusBubbleGtk::Reposition() {
  int x, y, width, parent_height;
  gdk_window_get_position(GTK_WIDGET(parent_)->window, &x, &y);
  gtk_window_get_size(parent_, &width, &parent_height);
  GtkRequisition requisition;
  gtk_widget_size_request(window_, &requisition);
  // TODO(port): RTL positioning.
  gtk_window_move(GTK_WINDOW(window_), x,
                  y + parent_height - requisition.height);
}
