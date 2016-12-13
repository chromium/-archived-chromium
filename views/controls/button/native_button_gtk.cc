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
#include "views/controls/native/native_view_host_gtk.h"
#include "views/widget/widget.h"

namespace views {

NativeButtonGtk::NativeButtonGtk(NativeButton* native_button)
    : NativeControlGtk(),
      native_button_(native_button) {
  // Associates the actual GtkWidget with the native_button so the native_button
  // is the one considered as having the focus (not the wrapper) when the
  // GtkWidget is focused directly (with a click for example).
  set_focus_view(native_button);
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

gfx::NativeView NativeButtonGtk::GetTestingHandle() const {
  return native_view();
}

gfx::Size NativeButtonGtk::GetPreferredSize() {
  if (!native_view())
    return gfx::Size();
  GtkRequisition size_request = { 0, 0 };
  gtk_widget_size_request(native_view(), &size_request);
  return gfx::Size(size_request.width, size_request.height);
}

void NativeButtonGtk::CreateNativeControl() {
  GtkWidget* widget = gtk_button_new();
  g_signal_connect(G_OBJECT(widget), "clicked",
                   G_CALLBACK(CallClicked), this);
  NativeControlCreated(widget);
}

void NativeButtonGtk::NativeControlCreated(GtkWidget* widget) {
  NativeControlGtk::NativeControlCreated(widget);

  UpdateFont();
  UpdateLabel();
  UpdateDefault();
}

// static
void NativeButtonGtk::CallClicked(GtkButton* widget, NativeButtonGtk* button) {
  button->OnClicked();
}

void NativeButtonGtk::OnClicked() {
  native_button_->ButtonPressed();
}

NativeCheckboxGtk::NativeCheckboxGtk(Checkbox* checkbox)
    : NativeButtonGtk(checkbox) {
}

void NativeCheckboxGtk::CreateNativeControl() {
  GtkWidget* widget = gtk_check_button_new();
  NativeControlCreated(widget);
}

// static
int NativeButtonWrapper::GetFixedWidth() {
  // TODO(brettw) implement this properly.
  return 10;
}

// static
NativeButtonWrapper* NativeButtonWrapper::CreateNativeButtonWrapper(
    NativeButton* native_button) {
  return new NativeButtonGtk(native_button);
}

// static
NativeButtonWrapper* NativeButtonWrapper::CreateCheckboxWrapper(
    Checkbox* checkbox) {
  return new NativeCheckboxGtk(checkbox);
}

}  // namespace views
