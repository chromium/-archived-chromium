// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tab_contents/native_tab_contents_container_win.h"

#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/views/tab_contents/tab_contents_view_win.h"

#include "views/focus/focus_manager.h"

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
}

void NativeTabContentsContainerWin::DetachContents(TabContents* contents) {
  // TODO(brettw) should this move to NativeViewHost::Detach which is called
  // below?
  // It needs cleanup regardless.
  HWND container_hwnd = contents->GetNativeView();
  if (container_hwnd) {
    // Hide the contents before adjusting its parent to avoid a full desktop
    // flicker.
    ShowWindow(container_hwnd, SW_HIDE);

    // Reset the parent to NULL to ensure hidden tabs don't receive messages.
    static_cast<TabContentsViewWin*>(contents->view())->Unparent();
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
  // If we are focused, we need to pass the focus to the new RenderViewHost.
  if (GetFocusManager()->GetFocusedView() == this)
    Focus();
}

views::View* NativeTabContentsContainerWin::GetView() {
  return this;
}

void NativeTabContentsContainerWin::TabContentsFocused(
    TabContents* tab_contents) {
  views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    NOTREACHED();
    return;
  }
  focus_manager->SetFocusedView(this);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerWin, views::View overrides:

bool NativeTabContentsContainerWin::SkipDefaultKeyEventProcessing(
    const views::KeyEvent& e) {
  // Don't look-up accelerators or tab-traversal if we are showing a non-crashed
  // TabContents.
  // We'll first give the page a chance to process the key events.  If it does
  // not process them, they'll be returned to us and we'll treat them as
  // accelerators then.
  return container_->tab_contents() &&
         !container_->tab_contents()->is_crashed();
}

bool NativeTabContentsContainerWin::IsFocusable() const {
  // We need to be focusable when our contents is not a view hierarchy, as
  // clicking on the contents needs to focus us.
  return container_->tab_contents() != NULL;
}

void NativeTabContentsContainerWin::Focus() {
  if (container_->tab_contents())
    container_->tab_contents()->Focus();
}

void NativeTabContentsContainerWin::RequestFocus() {
  // This is a hack to circumvent the fact that a the Focus() method is not
  // invoked when RequestFocus() is called on an already focused view.
  // The TabContentsContainer is the view focused when the TabContents has
  // focus.  When switching between from one tab that has focus to another tab
  // that should also have focus, RequestFocus() is invoked one the
  // TabContentsContainer.  In order to make sure Focus() is invoked we need to
  // clear the focus before hands.
  GetFocusManager()->ClearFocus();
  View::RequestFocus();
}

void NativeTabContentsContainerWin::AboutToRequestFocusFromTabTraversal(
    bool reverse) {
  container_->tab_contents()->FocusThroughTabTraversal(reverse);
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
