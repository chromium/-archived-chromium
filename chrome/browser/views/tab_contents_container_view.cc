// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/views/tab_contents_container_view.h"

#include "base/logging.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/render_view_host_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/root_view.h"
#include "chrome/views/widget.h"

using views::FocusTraversable;
using views::FocusManager;
using views::View;

TabContentsContainerView::TabContentsContainerView() : tab_contents_(NULL) {
  SetID(VIEW_ID_TAB_CONTAINER);
}

TabContentsContainerView::~TabContentsContainerView() {
  if (tab_contents_)
    RemoveObservers();
}

void TabContentsContainerView::SetTabContents(TabContents* tab_contents) {
  if (tab_contents_) {
    // TODO(brettw) should this move to HWNDView::Detach which is called below?
    // It needs cleanup regardless.
    HWND container_hwnd = tab_contents_->GetNativeView();

    // Hide the contents before adjusting its parent to avoid a full desktop
    // flicker.
    ::ShowWindow(container_hwnd, SW_HIDE);

    // Reset the parent to NULL to ensure hidden tabs don't receive messages.
    ::SetParent(container_hwnd, NULL);

    tab_contents_->WasHidden();

    // Unregister the tab contents window from the FocusManager.
    views::FocusManager::UninstallFocusSubclass(container_hwnd);
    HWND hwnd = tab_contents_->GetContentNativeView();
    if (hwnd) {
      // We may not have an HWND anymore, if the renderer crashed and we are
      // displaying the sad tab for example.
      FocusManager::UninstallFocusSubclass(hwnd);
    }

    views::RootView* root_view = tab_contents_->GetContentsRootView();
    if (root_view) {
      // Unlink the RootViews as a clean-up.
      root_view->SetFocusTraversableParent(NULL);
      root_view->SetFocusTraversableParentView(NULL);
    }

    // Now detach the TabContents.
    Detach();

    RemoveObservers();
  }

  tab_contents_ = tab_contents;

  if (!tab_contents_) {
    // When detaching the last tab of the browser SetTabContents is invoked
    // with NULL. Don't attempt to do anything in that case.
    return;
  }

  // We need to register the tab contents window with the BrowserContainer so
  // that the BrowserContainer is the focused view when the focus is on the
  // TabContents window (for the WebContents case).
  SetAssociatedFocusView(this);

  Attach(tab_contents->GetNativeView());
  HWND contents_hwnd = tab_contents_->GetContentNativeView();
  if (contents_hwnd)
    FocusManager::InstallFocusSubclass(contents_hwnd, this);

  AddObservers();

  views::RootView* root_view = tab_contents_->GetContentsRootView();
  if (root_view) {
    // Link the RootViews for proper focus traversal (note that we skip the
    // TabContentsContainerView as it acts as a FocusTraversable proxy).
    root_view->SetFocusTraversableParent(GetRootView());
    root_view->SetFocusTraversableParentView(this);
  }
}

views::FocusTraversable* TabContentsContainerView::GetFocusTraversable() {
  if (tab_contents_ && tab_contents_->GetContentsRootView())
    return tab_contents_->GetContentsRootView();
  return NULL;
}

bool TabContentsContainerView::IsFocusable() const {
  // We need to be focusable when our contents is not a view hierarchy, as
  // clicking on the contents needs to focus us.
  if (tab_contents_ && !tab_contents_->GetContentsRootView())
    return true;

  // If we do contain views, then we should just act as a regular container by
  // not being focusable.
  return false;
}

void TabContentsContainerView::AboutToRequestFocusFromTabTraversal(
    bool reverse) {
  if (!tab_contents_)
    return;
  // Give an opportunity to the tab to reset its focus.
  tab_contents_->SetInitialFocus(reverse);
}

bool TabContentsContainerView::CanProcessTabKeyEvents() {
  // TabContents with no RootView are supposed to deal with the focus traversal
  // explicitly.  For that reason, they receive tab key events as is.
  return tab_contents_ && !tab_contents_->GetContentsRootView();
}

views::FocusTraversable*
    TabContentsContainerView::GetFocusTraversableParent() {
  if (tab_contents_ && tab_contents_->GetContentsRootView()) {
    // Since we link the RootView of the TabContents to the RootView that
    // contains us, this should not be invoked.
    NOTREACHED();
    return NULL;
  }
  return GetRootView();
}

views::View* TabContentsContainerView::GetFocusTraversableParentView() {
  if (tab_contents_ && tab_contents_->GetContentsRootView()) {
    // Since we link the RootView of the TabContents to the RootView that
    // contains us, this should not be invoked.
    NOTREACHED();
    return NULL;
  }
  return this;
}

void TabContentsContainerView::Focus() {
  if (tab_contents_ && !tab_contents_->GetContentsRootView()) {
    // Set the native focus on the actual content of the tab.
    ::SetFocus(tab_contents_->GetContentNativeView());
  }
}

void TabContentsContainerView::RequestFocus() {
  // This is a hack to circumvent the fact that a view does not explicitly get
  // a call to set the focus if it already has the focus. This causes a problem
  // with tabs such as the WebContents that instruct the RenderView that it got
  // focus when they actually get the focus. When switching from one WebContents
  // tab that has focus to another WebContents tab that had focus, since the
  // TabContentsContainerView already has focus, Focus() would not be called and
  // the RenderView would not get notified it got focused.
  // By clearing the focused view before-hand, we ensure Focus() will be called.
  GetRootView()->FocusView(NULL);
  View::RequestFocus();
}

bool TabContentsContainerView::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_GROUPING;
  return true;
}

bool TabContentsContainerView::ShouldLookupAccelerators(
    const views::KeyEvent& e) {
  if (tab_contents_ && !tab_contents_->is_crashed() &&
      tab_contents_->AsWebContents())
    return false;
  return true;
}

void TabContentsContainerView::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  if (type == NotificationType::RENDER_VIEW_HOST_CHANGED) {
    RenderViewHostSwitchedDetails* switched_details =
        Details<RenderViewHostSwitchedDetails>(details).ptr();
    RenderViewHostChanged(switched_details->old_host,
                          switched_details->new_host);
  } else if (type == NotificationType::TAB_CONTENTS_DESTROYED) {
    TabContentsDestroyed(Source<TabContents>(source).ptr());
  } else {
    NOTREACHED();
  }
}

void TabContentsContainerView::AddObservers() {
  DCHECK(tab_contents_);
  if (tab_contents_->AsWebContents()) {
    // WebContents can change their RenderViewHost and hence the HWND that is
    // shown and getting focused.  We need to keep track of that so we install
    // the focus subclass on the shown HWND so we intercept focus change events.
    NotificationService::current()->AddObserver(
        this, NotificationType::RENDER_VIEW_HOST_CHANGED,
        Source<NavigationController>(tab_contents_->controller()));
  }
  NotificationService::current()->AddObserver(
      this,
      NotificationType::TAB_CONTENTS_DESTROYED,
      Source<TabContents>(tab_contents_));
}

void TabContentsContainerView::RemoveObservers() {
  DCHECK(tab_contents_);
  if (tab_contents_->AsWebContents()) {
    NotificationService::current()->RemoveObserver(
        this,
        NotificationType::RENDER_VIEW_HOST_CHANGED,
        Source<NavigationController>(tab_contents_->controller()));
  }
  NotificationService::current()->RemoveObserver(
      this,
      NotificationType::TAB_CONTENTS_DESTROYED,
      Source<TabContents>(tab_contents_));
}

void TabContentsContainerView::RenderViewHostChanged(RenderViewHost* old_host,
                                                     RenderViewHost* new_host) {
  if (old_host && old_host->view()) {
    FocusManager::UninstallFocusSubclass(
        old_host->view()->GetPluginNativeView());
  }

  if (new_host && new_host->view()) {
    FocusManager::InstallFocusSubclass(
        new_host->view()->GetPluginNativeView(), this);
  }

  // If we are focused, we need to pass the focus to the new RenderViewHost.
  FocusManager* focus_manager =
      FocusManager::GetFocusManager(GetRootView()->GetWidget()->GetHWND());
  if (focus_manager->GetFocusedView() == this)
    Focus();
}

void TabContentsContainerView::TabContentsDestroyed(TabContents* contents) {
  // Sometimes, a TabContents is destroyed before we know about it. This allows
  // us to clean up our state in case this happens.
  DCHECK(contents == tab_contents_);
  SetTabContents(NULL);
}

