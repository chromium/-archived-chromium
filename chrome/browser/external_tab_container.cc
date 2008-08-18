// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/external_tab_container.h"

#include "base/logging.h"
#include "base/win_util.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_contents_container_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/win_util.h"
#include "chrome/views/hwnd_view_container.h"
#include "chrome/test/automation/automation_messages.h"

static const wchar_t kWindowObjectKey[] = L"ChromeWindowObject";

// TODO(sanjeevr): The external_accel_table_ and external_accel_entry_count_
// member variables are now obsolete and we don't use them.
// We need to remove them.
ExternalTabContainer::ExternalTabContainer(
    AutomationProvider* automation)
    : automation_(automation),
      root_view_(this),
      tab_contents_(NULL),
      external_accel_table_(NULL),
      external_accel_entry_count_(0),
      tab_contents_container_(NULL) {
}

ExternalTabContainer::~ExternalTabContainer() {
}

bool ExternalTabContainer::Init(Profile* profile) {
  if (IsWindow()) {
    NOTREACHED();
    return false;
  }
  // First create the container window
  if (!Create(NULL)) {
    NOTREACHED();
    return false;
  }

  // We don't ever remove the prop because the lifetime of this object
  // is the same as the lifetime of the window
  SetProp(*this, kWindowObjectKey, this);

  ChromeViews::SetRootViewForHWND(m_hWnd, &root_view_);
  // CreateFocusManager will subclass this window and delete the FocusManager
  // instance when this window goes away.
  ChromeViews::FocusManager* focus_manager =
      ChromeViews::FocusManager::CreateFocusManager(m_hWnd, GetRootView());
  DCHECK(focus_manager);
  focus_manager->AddKeystrokeListener(this);
  tab_contents_ = TabContents::CreateWithType(TAB_CONTENTS_WEB,
                                              m_hWnd, profile, NULL);
  if (!tab_contents_) {
    NOTREACHED();
    DestroyWindow();
    return false;
  }
  tab_contents_->SetupController(profile);
  tab_contents_->set_delegate(this);
  // Create a TabContentsContainerView to handle focus cycling using Tab and
  // Shift-Tab.
  // TODO(sanjeevr): We need to create a dummy FocusTraversable object to
  // represent the frame of the external host. This will allow Tab and
  // Shift-Tab to cycle into the external frame.
  tab_contents_container_ = new TabContentsContainerView();
  root_view_.AddChildView(tab_contents_container_);
  tab_contents_container_->SetTabContents(tab_contents_);

  NavigationController* controller = tab_contents_->controller();
  DCHECK(controller);
  NotificationService::current()->
      Notify(NOTIFY_EXTERNAL_TAB_CREATED,
              Source<NavigationController>(controller),
              NotificationService::NoDetails());
  ::ShowWindow(tab_contents_->GetContainerHWND(), SW_SHOW);
  return true;
}

void ExternalTabContainer::OnDestroy() {
  ChromeViews::FocusManager * focus_manager =
      ChromeViews::FocusManager::GetFocusManager(GetHWND());
  if (focus_manager) {
    focus_manager->RemoveKeystrokeListener(this);
  }
  root_view_.RemoveAllChildViews(true);
  if (tab_contents_) {
    NavigationController* controller = tab_contents_->controller();
    DCHECK(controller);

    NotificationService::current()->
        Notify(NOTIFY_EXTERNAL_TAB_CLOSED,
                Source<NavigationController>(controller),
                Details<ExternalTabContainer>(this));
    tab_contents_->set_delegate(NULL);
    tab_contents_->CloseContents();
    // WARNING: tab_contents_ has likely been deleted.
    tab_contents_ = NULL;
  }
}

void ExternalTabContainer::OnFinalMessage(HWND window) {
  delete this;
}

LRESULT ExternalTabContainer::OnSize(UINT, WPARAM, LPARAM, BOOL& handled) {
  if (tab_contents_) {
    RECT client_rect = {0};
    GetClientRect(&client_rect);
    ::SetWindowPos(tab_contents_->GetContainerHWND(), NULL, client_rect.left,
                   client_rect.top, client_rect.right - client_rect.left,
                   client_rect.bottom - client_rect.top, SWP_NOZORDER);
  }
  return 0;
}

// TODO(sanjeevr): The implementation of the TabContentsDelegate interface
// needs to be fully fleshed out based on the requirements of the
// "Chrome tab in external browser" feature.

void ExternalTabContainer::OpenURLFromTab(
    TabContents* source,
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition,
    const std::wstring& override_encoding) {
  switch (disposition) {
    case CURRENT_TAB:
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB:
    case NEW_WINDOW:
      if (automation_) {
        automation_->Send(new AutomationMsg_OpenURL(0, url, disposition));
      }
      break;
    default:
      break;
   }
}

void ExternalTabContainer::NavigationStateChanged(const TabContents* source,
                                    unsigned changed_flags) {
  if (automation_) {
    automation_->Send(
        new AutomationMsg_NavigationStateChanged(0,changed_flags));
  }
}

void ExternalTabContainer::ReplaceContents(TabContents* source, TabContents* new_contents) {
}

void ExternalTabContainer::AddNewContents(TabContents* source,
                            TabContents* new_contents,
                            WindowOpenDisposition disposition,
                            const gfx::Rect& initial_pos,
                            bool user_gesture) {
}

void ExternalTabContainer::StartDraggingDetachedContents(
    TabContents* source,
    TabContents* new_contents,
    const gfx::Rect& contents_bounds,
    const gfx::Point& mouse_pt) {
}

void ExternalTabContainer::ActivateContents(TabContents* contents) {
}

void ExternalTabContainer::LoadingStateChanged(TabContents* source) {
}

void ExternalTabContainer::CloseContents(TabContents* source) {
}

void ExternalTabContainer::MoveContents(TabContents* source, const gfx::Rect& pos) {
}

bool ExternalTabContainer::IsPopup(TabContents* source) {
  return false;
}

void ExternalTabContainer::URLStarredChanged(TabContents* source, bool starred) {
}

void ExternalTabContainer::UpdateTargetURL(TabContents* source,
                                           const GURL& url) {
  if (automation_) {
    std::wstring url_string = CA2W(url.spec().c_str());
    automation_->Send(
        new AutomationMsg_UpdateTargetUrl(0, url_string));
  }
}

void ExternalTabContainer::ContentsZoomChange(bool zoom_in) {
}

void ExternalTabContainer::ToolbarSizeChanged(TabContents* source,
                                            bool finished) {
}

void ExternalTabContainer::DidNavigate(NavigationType nav_type,
                                       int relative_navigation_offet) {
  if (automation_) {
    automation_->Send(
        new AutomationMsg_DidNavigate(0, nav_type,
                                      relative_navigation_offet));
  }
}

void ExternalTabContainer::SendExternalHostMessage(const std::string& receiver,
                                                   const std::string& message) {
  if(automation_) {
    automation_->Send(
        new AutomationMsg_SendExternalHostMessage(0, receiver, message));
  }
}

void ExternalTabContainer::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
}

void ExternalTabContainer::GetBounds(CRect *out, bool including_frame) const {
  GetWindowRect(out);
}

void ExternalTabContainer::MoveToFront(bool should_activate) {
}

HWND ExternalTabContainer::GetHWND() const {
  return m_hWnd;
}

void ExternalTabContainer::PaintNow(const CRect& update_rect) {
  RedrawWindow(update_rect,
               NULL,
               RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
}

ChromeViews::RootView* ExternalTabContainer::GetRootView() {
  return const_cast<ChromeViews::RootView*>(&root_view_);
}

bool ExternalTabContainer::IsVisible() {
  return !!::IsWindowVisible(*this);
}

bool ExternalTabContainer::IsActive() {
  return win_util::IsWindowActive(*this);
}

bool ExternalTabContainer::ProcessKeyDown(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam) {
  if (!automation_) {
    return false;
  }
  if ((wparam == VK_TAB) && !win_util::IsCtrlPressed()) {
    // Tabs are handled separately (except if this is Ctrl-Tab or
    // Ctrl-Shift-Tab)
    return false;
  }
  int flags = HIWORD(lparam);
  if ((flags & KF_EXTENDED) || (flags & KF_ALTDOWN) ||
      win_util::IsShiftPressed() || win_util::IsCtrlPressed()) {
    // If this is an extended key or if one or more of Alt, Shift and Control
    // are pressed, this might be an accelerator that the external host wants
    // to handle. If the host does not handle this accelerator, it will reflect
    // the accelerator back to us via the ProcessUnhandledAccelerator method.
    MSG msg = {0};
    msg.hwnd = window;
    msg.message = message;
    msg.wParam = wparam;
    msg.lParam = lparam;
    automation_->Send(new AutomationMsg_HandleAccelerator(0, msg));
    return true;
  }
  return false;
}

void ExternalTabContainer::SetAccelerators(HACCEL accel_table,
                                           int accel_table_entry_count) {
  external_accel_table_ = accel_table;
  external_accel_entry_count_ = accel_table_entry_count;
}

void ExternalTabContainer::ProcessUnhandledAccelerator(const MSG& msg) {
  // We just received an accelerator key that we had sent to external host
  // back. Since the external host was not interested in handling this, we
  // need to dispatch this message as if we had just peeked this out. (we
  // also need to call TranslateMessage to generate a WM_CHAR if needed).
  TranslateMessage(&msg);
  DispatchMessage(&msg);
}

// static
bool ExternalTabContainer::IsExternalTabContainer(HWND window) {
  std::wstring class_name = win_util::GetClassName(window);
  return _wcsicmp(class_name.c_str(), chrome::kExternalTabWindowClass) == 0;
}

// static
ExternalTabContainer* ExternalTabContainer::GetContainerForTab(
    HWND tab_window) {
  HWND parent_window = ::GetParent(tab_window);
  if (!::IsWindow(parent_window)) {
    return NULL;
  }
  if (!IsExternalTabContainer(parent_window)) {
    return NULL;
  }
  ExternalTabContainer* container = reinterpret_cast<ExternalTabContainer*>(
      GetProp(parent_window, kWindowObjectKey));
  return container;
}
