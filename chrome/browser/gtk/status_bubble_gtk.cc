// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/status_bubble_gtk.h"

#include <gtk/gtk.h>

#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/slide_animator_gtk.h"
#include "chrome/common/gtk_util.h"
#include "googleurl/src/gurl.h"

namespace {

const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xe6, 0xed, 0xf4);
const GdkColor kFrameBorderColor = GDK_COLOR_RGB(0xbe, 0xc8, 0xd4);

// Inner padding between the border and the text label.
const int kInternalTopBottomPadding = 1;
const int kInternalLeftRightPadding = 2;

// Border of color kFrameBorderColor around the status bubble.
const int kBorderPadding = 1;

}  // namespace

StatusBubbleGtk::StatusBubbleGtk()
    : parent_(NULL) {
  InitWidgets();
}

StatusBubbleGtk::~StatusBubbleGtk() {
  container_.Destroy();
}

void StatusBubbleGtk::SetStatus(const std::string& status) {
  if (status.empty()) {
    Hide();
    return;
  }

  gtk_label_set_text(GTK_LABEL(label_), status.c_str());

  Show();
}

void StatusBubbleGtk::SetStatus(const std::wstring& status) {
  SetStatus(WideToUTF8(status));
}

void StatusBubbleGtk::SetParentAllocation(
    GtkWidget* parent, GtkAllocation* allocation) {
  parent_ = parent;
  parent_allocation_ = *allocation;
  SetStatusBubbleSize();
}

void StatusBubbleGtk::SetURL(const GURL& url, const std::wstring& languages) {
  SetStatus(url.possibly_invalid_spec());
}

void StatusBubbleGtk::Show() {
  SetStatusBubbleSize();
  gtk_widget_show_all(container_.get());

  if (container_.get()->window)
    gdk_window_raise(container_.get()->window);
}

void StatusBubbleGtk::Hide() {
  gtk_widget_hide_all(container_.get());
}

void StatusBubbleGtk::SetStatusBubbleSize() {
  if (parent_) {
    GtkRequisition requisition;
    gtk_widget_size_request(container_.get(), &requisition);

    // TODO(erg): Previously, I put a call to gtk_fixed_put() here. It appears
    // that doing this sets off a size-allocate storm, since gtk_fixed_put()
    // calls gtk_widget_queue_resize on the GtkFixed that caused this message.
    // The real solution may be creating a subclass of GtkVBox that has extra
    // code to deal with floating widgets, but this hack is good enough for
    // Friday. evanm says that there's a a GtkFixed subclass in test_shell that
    // we'll be stealing for plugin support anyway that should also do the same
    // task.

    GtkAllocation widget_allocation;
    int child_y = std::max(
        parent_allocation_.y + parent_allocation_.height - requisition.height,
        0);
    widget_allocation.x = 0;
    widget_allocation.y = child_y;
    widget_allocation.width = std::min(requisition.width,
                                       parent_allocation_.width);
    widget_allocation.height = requisition.height;
    gtk_widget_size_allocate(container_.get(), &widget_allocation);
  }
}

void StatusBubbleGtk::MouseMoved() {
  // We can't do that fancy sliding behaviour where the status bubble slides
  // out of the window because the window manager gets in the way. So totally
  // ignore this message for now.
  //
  // TODO(erg): At least get some sliding behaviour so that it slides out of
  // the way to hide the status bubble on mouseover.
}

void StatusBubbleGtk::InitWidgets() {
  label_ = gtk_label_new(NULL);

  GtkWidget* padding = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(padding),
      kInternalTopBottomPadding, kInternalTopBottomPadding,
      kInternalLeftRightPadding, kInternalLeftRightPadding);
  gtk_container_add(GTK_CONTAINER(padding), label_);

  GtkWidget* bg_box = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(bg_box), padding);
  gtk_widget_modify_bg(bg_box, GTK_STATE_NORMAL, &kBackgroundColor);

  container_.Own(gtk_util::CreateGtkBorderBin(bg_box, &kFrameBorderColor,
          kBorderPadding, kBorderPadding, kBorderPadding, kBorderPadding));
  gtk_widget_set_name(container_.get(), "status-bubble");
  gtk_widget_set_app_paintable(container_.get(), TRUE);
}
