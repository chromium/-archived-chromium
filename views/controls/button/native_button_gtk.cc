// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include <gtk/gtk.h>

#include "views/controls/button/native_button_gtk.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/widget/widget.h"

namespace views {

NativeButtonGtk::NativeButtonGtk(NativeButton* native_button)
    : NativeControlGtk(),
      native_button_(native_button) {
  // Associates the actual GtkWidget with the native_button so the native_button
  // is the one considered as having the focus (not the wrapper) when the
  // GtkWidget is focused directly (with a click for example).
  SetAssociatedFocusView(native_button);
}

NativeButtonGtk::~NativeButtonGtk() {
}

void NativeButtonGtk::UpdateLabel() {
  if (!native_view())
    return;

  gtk_button_set_label(GTK_BUTTON(native_view()),
                       WideToUTF8(native_button_->label()).c_str());
}

void NativeButtonGtk::UpdateFont() {
  if (!native_view())
    return;

  NOTIMPLEMENTED();
  // SendMessage(GetHWND(), WM_SETFONT,
  // reinterpret_cast<WPARAM>(native_button_->font().hfont()),
  // FALSE);
}

void NativeButtonGtk::UpdateEnabled() {
  if (!native_view())
    return;
  SetEnabled(native_button_->IsEnabled());
}

void NativeButtonGtk::UpdateDefault() {
  if (!native_view())
    return;
  if (!IsCheckbox())
    NOTIMPLEMENTED();
}

View* NativeButtonGtk::GetView() {
  return this;
}

void NativeButtonGtk::SetFocus() {
  // Focus the associated widget.
  Focus();
}

gfx::Size NativeButtonGtk::GetPreferredSize() {
  GtkRequisition size_request = { 0, 0 };
  gtk_widget_size_request(native_view(), &size_request);
  return gfx::Size(size_request.width, size_request.height);
}

void NativeButtonGtk::CreateNativeControl() {
  GtkWidget* widget = gtk_button_new();
  NativeControlCreated(widget);
}

void NativeButtonGtk::NativeControlCreated(GtkWidget* widget) {
  NativeControlGtk::NativeControlCreated(widget);

  UpdateFont();
  UpdateLabel();
  UpdateDefault();
}

// static
NativeButtonWrapper* NativeButtonWrapper::CreateNativeButtonWrapper(
    NativeButton* native_button) {
  return new NativeButtonGtk(native_button);
}

}  // namespace views
