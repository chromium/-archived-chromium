// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/window_gtk.h"

#include "app/l10n_util.h"
#include "base/gfx/rect.h"
#include "views/window/custom_frame_view.h"
#include "views/window/non_client_view.h"
#include "views/window/window_delegate.h"

namespace views {

WindowGtk::~WindowGtk() {
}

// static
Window* Window::CreateChromeWindow(gfx::NativeWindow parent,
                                   const gfx::Rect& bounds,
                                   WindowDelegate* window_delegate) {
  WindowGtk* window = new WindowGtk(window_delegate);
  window->GetNonClientView()->SetFrameView(window->CreateFrameViewForWindow());
  window->Init(bounds);
  return window;
}

gfx::Rect WindowGtk::GetBounds() const {
  gfx::Rect bounds;
  WidgetGtk::GetBounds(&bounds, true);
  return bounds;
}

gfx::Rect WindowGtk::GetNormalBounds() const {
  NOTIMPLEMENTED();
  return GetBounds();
}

void WindowGtk::SetBounds(const gfx::Rect& bounds) {
  // TODO: this may need to set an initial size if not showing.
  // TODO: need to constrain based on screen size.
  gtk_window_resize(GTK_WINDOW(GetNativeView()), bounds.width(),
                    bounds.height());

  gtk_window_move(GTK_WINDOW(GetNativeView()), bounds.x(), bounds.y());
}

void WindowGtk::SetBounds(const gfx::Rect& bounds,
                          gfx::NativeWindow other_window) {
  // TODO: need to deal with other_window.
  SetBounds(bounds);
}

void WindowGtk::Show() {
  gtk_widget_show_all(GetNativeView());
}

void WindowGtk::HideWindow() {
  NOTIMPLEMENTED();
}

void WindowGtk::PushForceHidden() {
  NOTIMPLEMENTED();
}

void WindowGtk::PopForceHidden() {
  NOTIMPLEMENTED();
}

void WindowGtk::Activate() {
  NOTIMPLEMENTED();
}

void WindowGtk::Close() {
  NOTIMPLEMENTED();
}

void WindowGtk::Maximize() {
  gtk_window_maximize(GetNativeWindow());
}

void WindowGtk::Minimize() {
  gtk_window_iconify(GetNativeWindow());
}

void WindowGtk::Restore() {
  NOTIMPLEMENTED();
}

bool WindowGtk::IsActive() const {
  return gtk_window_is_active(GetNativeWindow());
}

bool WindowGtk::IsVisible() const {
  return GTK_WIDGET_VISIBLE(GetNativeView());
}

bool WindowGtk::IsMaximized() const {
  NOTIMPLEMENTED();
  return false;
}

bool WindowGtk::IsMinimized() const {
  NOTIMPLEMENTED();
  return false;
}

void WindowGtk::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool WindowGtk::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

void WindowGtk::EnableClose(bool enable) {
  gtk_window_set_deletable(GetNativeWindow(), enable);
}

void WindowGtk::DisableInactiveRendering() {
  NOTIMPLEMENTED();
}

void WindowGtk::UpdateWindowTitle() {
  // If the non-client view is rendering its own title, it'll need to relayout
  // now.
  non_client_view_->Layout();

  // Update the native frame's text. We do this regardless of whether or not
  // the native frame is being used, since this also updates the taskbar, etc.
  std::wstring window_title = window_delegate_->GetWindowTitle();
  std::wstring localized_text;
  if (l10n_util::AdjustStringForLocaleDirection(window_title, &localized_text))
    window_title.assign(localized_text);

  gtk_window_set_title(GetNativeWindow(), WideToUTF8(window_title).c_str());
}

void WindowGtk::UpdateWindowIcon() {
  NOTIMPLEMENTED();
}

void WindowGtk::SetIsAlwaysOnTop(bool always_on_top) {
  NOTIMPLEMENTED();
}

NonClientFrameView* WindowGtk::CreateFrameViewForWindow() {
  // TODO(erg): Always use a custom frame view? Are there cases where we let
  // the window manager deal with the X11 equivalent of the "non-client" area?
  return new CustomFrameView(this);
}

void WindowGtk::UpdateFrameAfterFrameChange() {
  NOTIMPLEMENTED();
}

WindowDelegate* WindowGtk::GetDelegate() const {
  return window_delegate_;
}

NonClientView* WindowGtk::GetNonClientView() const {
  return non_client_view_;
}

ClientView* WindowGtk::GetClientView() const {
  return non_client_view_->client_view();
}

gfx::NativeWindow WindowGtk::GetNativeWindow() const {
  return GTK_WINDOW(GetNativeView());
}

WindowGtk::WindowGtk(WindowDelegate* window_delegate)
    : WidgetGtk(TYPE_WINDOW),
      is_modal_(false),
      window_delegate_(window_delegate),
      non_client_view_(new NonClientView(this)) {
  is_window_ = true;
  window_delegate_->window_.reset(this);
}

void WindowGtk::Init(const gfx::Rect& bounds) {
  // We call this after initializing our members since our implementations of
  // assorted WidgetWin functions may be called during initialization.
  is_modal_ = window_delegate_->IsModal();
  if (is_modal_) {
    // TODO(erg): Fix once modality works.
    // BecomeModal();
  }

  WidgetGtk::Init(bounds, true);

  // Create the ClientView, add it to the NonClientView and add the
  // NonClientView to the RootView. This will cause everything to be parented.
  non_client_view_->set_client_view(window_delegate_->CreateClientView(this));
  WidgetGtk::SetContentsView(non_client_view_);

  UpdateWindowTitle();

  //  SetInitialBounds(bounds);

  // if (!IsAppWindow()) {
  //   notification_registrar_.Add(
  //       this,
  //       NotificationType::ALL_APPWINDOWS_CLOSED,
  //       NotificationService::AllSources());
  // }

  // ResetWindowRegion(false);
}

}  // namespace views
