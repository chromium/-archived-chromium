// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tab_contents/native_tab_contents_container_win.h"

#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tab_contents/tab_contents_container.h"
#include "views/focus/focus_manager.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerWin, public:

NativeTabContentsContainerWin::NativeTabContentsContainerWin(
    TabContentsContainer* container)
    : container_(container) {
}

NativeTabContentsContainerWin::~NativeTabContentsContainerWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerWin, NativeTabContentsContainer overrides:

void NativeTabContentsContainerWin::AttachContents(TabContents* contents) {
  // We need to register the tab contents window with the BrowserContainer so
  // that the BrowserContainer is the focused view when the focus is on the
  // TabContents window (for the TabContents case).
  set_focus_view(this);

  Attach(contents->GetNativeView());
  HWND contents_hwnd = contents->GetContentNativeView();
  if (contents_hwnd)
    views::FocusManager::InstallFocusSubclass(contents_hwnd, this);
}

void NativeTabContentsContainerWin::DetachContents(TabContents* contents) {
  // TODO(brettw) should this move to NativeViewHost::Detach which is called below?
  // It needs cleanup regardless.
  HWND container_hwnd = contents->GetNativeView();
  if (container_hwnd) {
    // Hide the contents before adjusting its parent to avoid a full desktop
    // flicker.
    ShowWindow(container_hwnd, SW_HIDE);

    // Reset the parent to NULL to ensure hidden tabs don't receive messages.
    ::SetParent(container_hwnd, NULL);

    // Unregister the tab contents window from the FocusManager.
    views::FocusManager::UninstallFocusSubclass(container_hwnd);
  }

  HWND hwnd = contents->GetContentNativeView();
  if (hwnd) {
    // We may not have an HWND anymore, if the renderer crashed and we are
    // displaying the sad tab for example.
    views::FocusManager::UninstallFocusSubclass(hwnd);
  }

  // Now detach the TabContents.
  Detach();
}

void NativeTabContentsContainerWin::SetFastResize(bool fast_resize) {
  set_fast_resize(fast_resize);
}

void NativeTabContentsContainerWin::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {
  if (old_host && old_host->view()) {
    views::FocusManager::UninstallFocusSubclass(
        old_host->view()->GetPluginNativeView());
  }

  if (new_host && new_host->view()) {
    views::FocusManager::InstallFocusSubclass(
        new_host->view()->GetPluginNativeView(), this);
  }

  // If we are focused, we need to pass the focus to the new RenderViewHost.
  views::FocusManager* focus_manager = views::FocusManager::GetFocusManager(
      GetRootView()->GetWidget()->GetNativeView());
  if (focus_manager->GetFocusedView() == this)
    Focus();
}

views::View* NativeTabContentsContainerWin::GetView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerWin, views::View overrides:

bool NativeTabContentsContainerWin::SkipDefaultKeyEventProcessing(
    const views::KeyEvent& e) {
  // Don't look-up accelerators or tab-traverse if we are showing a non-crashed
  // TabContents.
  // We'll first give the page a chance to process the key events.  If it does
  // not process them, they'll be returned to us and we'll treat them as
  // accelerators then.
  return container_->tab_contents() &&
         !container_->tab_contents()->is_crashed();
}

views::FocusTraversable* NativeTabContentsContainerWin::GetFocusTraversable() {
  return NULL;
}

bool NativeTabContentsContainerWin::IsFocusable() const {
  // We need to be focusable when our contents is not a view hierarchy, as
  // clicking on the contents needs to focus us.
  return container_->tab_contents() != NULL;
}

void NativeTabContentsContainerWin::Focus() {
  if (container_->tab_contents()) {
    // Set the native focus on the actual content of the tab, that is the
    // interstitial if one is showing.
    if (container_->tab_contents()->interstitial_page()) {
      container_->tab_contents()->interstitial_page()->Focus();
      return;
    }
    SetFocus(container_->tab_contents()->GetContentNativeView());
  }
}

void NativeTabContentsContainerWin::RequestFocus() {
  // This is a hack to circumvent the fact that a view does not explicitly get
  // a call to set the focus if it already has the focus. This causes a problem
  // with tabs such as the TabContents that instruct the RenderView that it got
  // focus when they actually get the focus. When switching from one TabContents
  // tab that has focus to another TabContents tab that had focus, since the
  // TabContentsContainerView already has focus, Focus() would not be called and
  // the RenderView would not get notified it got focused.
  // By clearing the focused view before-hand, we ensure Focus() will be called.
  GetRootView()->FocusView(NULL);
  View::RequestFocus();
}

void NativeTabContentsContainerWin::AboutToRequestFocusFromTabTraversal(
    bool reverse) {
  if (!container_->tab_contents())
    return;
  // Give an opportunity to the tab to reset its focus.
  if (container_->tab_contents()->interstitial_page()) {
    container_->tab_contents()->interstitial_page()->SetInitialFocus(reverse);
    return;
  }
  container_->tab_contents()->SetInitialFocus(reverse);
}

bool NativeTabContentsContainerWin::GetAccessibleRole(
    AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_GROUPING;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainer, public:

// static
NativeTabContentsContainer* NativeTabContentsContainer::CreateNativeContainer(
    TabContentsContainer* container) {
  return new NativeTabContentsContainerWin(container);
}
