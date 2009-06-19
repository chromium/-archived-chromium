// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/constrained_window_gtk.h"

#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"
#include "chrome/common/gtk_util.h"

// The minimal border around the edge of the notification.
const int kSmallPadding = 2;

ConstrainedWindowGtk::ConstrainedWindowGtk(
    TabContents* owner, ConstrainedWindowGtkDelegate* delegate)
    : owner_(owner),
      delegate_(delegate) {
  DCHECK(owner);
  DCHECK(delegate);
  GtkWidget* dialog = delegate->GetWidgetRoot();

  // Unlike other users of CreateBorderBin, we need a dedicated frame around
  // our "window".
  GtkWidget* ebox = gtk_event_box_new();
  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
      kSmallPadding, kSmallPadding, kSmallPadding, kSmallPadding);
  gtk_container_add(GTK_CONTAINER(alignment), dialog);
  gtk_container_add(GTK_CONTAINER(frame), alignment);
  gtk_container_add(GTK_CONTAINER(ebox), frame);
  border_.Own(ebox);

  gtk_widget_show_all(border_.get());

  // We collaborate with TabContentsViewGtk and stick ourselves in the
  // TabContentsViewGtk's floating container.
  ContainingView()->AttachConstrainedWindow(this);
}

ConstrainedWindowGtk::~ConstrainedWindowGtk() {
  border_.Destroy();
}

void ConstrainedWindowGtk::CloseConstrainedWindow() {
  ContainingView()->RemoveConstrainedWindow(this);
  delegate_->DeleteDelegate();
  owner_->WillClose(this);

  delete this;
}

TabContentsViewGtk* ConstrainedWindowGtk::ContainingView() {
  return static_cast<TabContentsViewGtk*>(owner_->view());
}

// static
ConstrainedWindow* ConstrainedWindow::CreateConstrainedDialog(
    TabContents* parent,
    ConstrainedWindowGtkDelegate* delegate) {
  return new ConstrainedWindowGtk(parent, delegate);
}
