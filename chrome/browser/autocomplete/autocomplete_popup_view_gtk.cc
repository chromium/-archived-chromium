// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_view_gtk.h"

#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/notification_service.h"

namespace {

const GdkColor kPopupBorderColor = GDK_COLOR_RGB(0xc7, 0xca, 0xce);
const GdkColor kPopupBackground = GDK_COLOR_RGB(0xff, 0xff, 0xff);
const GdkColor kHighlightColor = GDK_COLOR_RGB(0xc1, 0xc8, 0xd9);

}  // namespace

AutocompletePopupViewGtk::AutocompletePopupViewGtk(
    AutocompleteEditViewGtk* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      window_(gtk_window_new(GTK_WINDOW_POPUP)),
      vbox_(NULL),
      opened_(false) {
  GTK_WIDGET_UNSET_FLAGS(window_, GTK_CAN_FOCUS);
  // Don't allow the window to be resized.  This also forces the window to
  // shrink down to the size of its child contents.
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);
  // Set up a 1 pixel border around the popup.
  gtk_container_set_border_width(GTK_CONTAINER(window_), 1);
  gtk_widget_modify_bg(window_, GTK_STATE_NORMAL, &kPopupBorderColor);
  gtk_widget_modify_base(window_, GTK_STATE_NORMAL, &kPopupBorderColor);
}

AutocompletePopupViewGtk::~AutocompletePopupViewGtk() {
  // Explicitly destroy our model here, before we destroy our GTK widgets.
  // This is because the model destructor can call back into us, and we need
  // to make sure everything is still valid when it does.
  model_.reset();
  if (vbox_)
    gtk_widget_destroy(vbox_);
  gtk_widget_destroy(window_);
}

void AutocompletePopupViewGtk::InvalidateLine(size_t line) {
  UpdatePopupAppearance();
}

void AutocompletePopupViewGtk::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  if (result.empty()) {
    Hide();
    return;
  }

  // TODO(deanm): This could be done better, but the code is temporary
  // and will need to be replaced with custom drawing and not widgets.
  if (vbox_)
    gtk_widget_destroy(vbox_);

  vbox_ = gtk_vbox_new(FALSE, 0);

  for (size_t i = 0; i < result.size(); ++i) {
    std::string utf8;
    utf8.append(WideToUTF8(result.match_at(i).contents));
    utf8.append(" - ");
    utf8.append(WideToUTF8(result.match_at(i).description));
    GtkWidget* label = gtk_label_new(utf8.c_str());
    // We need to put the labels in an event box for background painting.
    GtkWidget* ebox = gtk_event_box_new();
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_container_add(GTK_CONTAINER(ebox), label);
    gtk_box_pack_start(GTK_BOX(vbox_), ebox, FALSE, FALSE, 0);
    gtk_widget_show(label);
    gtk_widget_show(ebox);
    if (i == model_->selected_line()) {
      gtk_widget_modify_bg(ebox, GTK_STATE_NORMAL, &kHighlightColor);
    } else {
      gtk_widget_modify_bg(ebox, GTK_STATE_NORMAL, &kPopupBackground);
    }
  }

  gtk_widget_show(vbox_);
  gtk_container_add(GTK_CONTAINER(window_), vbox_);
  Show();
}

void AutocompletePopupViewGtk::OnHoverEnabledOrDisabled(bool disabled) {
  NOTIMPLEMENTED();
}

void AutocompletePopupViewGtk::PaintUpdatesNow() {
  UpdatePopupAppearance();
}

void AutocompletePopupViewGtk::Show() {
  gint x, y, width;
  edit_view_->BottomLeftPosWidth(&x, &y, &width);
  gtk_window_move(GTK_WINDOW(window_), x, y);
  gtk_widget_set_size_request(window_, width, -1);
  gtk_widget_show(window_);
  opened_ = true;
}

void AutocompletePopupViewGtk::Hide() {
  gtk_widget_hide(window_);
  opened_ = false;
}
