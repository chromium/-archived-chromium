// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_tab_container.h"

#include "base/logging.h"
#include "base/win_util.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/provisional_load_details.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tab_contents_container_view.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/win_util.h"
// Included for SetRootViewForHWND.
#include "chrome/views/widget/widget_win.h"
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
      tab_contents_container_(NULL),
      ignore_next_load_notification_(false) {
}

ExternalTabContainer::~ExternalTabContainer() {
  Uninitialize(m_hWnd);
}

bool ExternalTabContainer::Init(Profile* profile, HWND parent,
                                const gfx::Rect& dimensions,
                                unsigned int style) {
  if (IsWindow()) {
    NOTREACHED();
    return false;
  }

  // First create the container window
  if (!Create(NULL, dimensions.ToRECT())) {
    NOTREACHED();
    return false;
  }

  // We don't ever remove the prop because the lifetime of this object
  // is the same as the lifetime of the window
  SetProp(*this, kWindowObjectKey, this);

  views::SetRootViewForHWND(m_hWnd, &root_view_);
  // CreateFocusManager will subclass this window and delete the FocusManager
  // instance when this window goes away.
  views::FocusManager* focus_manager =
      views::FocusManager::CreateFocusManager(m_hWnd, GetRootView());

  DCHECK(focus_manager);
  focus_manager->AddKeystrokeListener(this);
  tab_contents_ = TabContents::CreateWithType(TAB_CONTENTS_WEB, profile, NULL);
  if (!tab_contents_) {
    NOTREACHED();
    DestroyWindow();
    return false;
  }

  tab_contents_->SetupController(profile);
  tab_contents_->set_delegate(this);

  WebContents* web_conents = tab_contents_->AsWebContents();
  if (web_conents)
    web_conents->render_view_host()->AllowExternalHostBindings();

  // Create a TabContentsContainerView to handle focus cycling using Tab and
  // Shift-Tab.
  tab_contents_container_ = new TabContentsContainerView();
  root_view_.AddChildView(tab_contents_container_);
  // Note that SetTabContents must be called after AddChildView is called
  tab_contents_container_->SetTabContents(tab_contents_);
  // Add a dummy view to catch when the user tabs out of the tab
  // Create a dummy FocusTraversable object to represent the frame of the
  // external host. This will allow Tab and Shift-Tab to cycle into the
  // external frame.  When the tab_contents_container_ loses focus,
  // the focus will be moved to this class (See OnSetFocus in this file).
  // An alternative to using views::View and catching when the focus manager
  // shifts the focus to the dummy view could be to implement our own view
  // and handle AboutToRequestFocusFromTabTraversal.
  views::View* dummy = new views::View();
  dummy->SetFocusable(true);
  DCHECK(dummy->IsFocusable());
  root_view_.AddChildView(dummy);

  NavigationController* controller = tab_contents_->controller();
  DCHECK(controller);
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(controller));
  registrar_.Add(this, NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR,
                 Source<NavigationController>(controller));
  NotificationService::current()->Notify(
      NotificationType::EXTERNAL_TAB_CREATED,
      Source<NavigationController>(controller),
      NotificationService::NoDetails());

  // We need WS_POPUP to be on the window during initialization, but
  // once initialized we apply the requested style which may or may not
  // include the popup bit.
  // Note that it's important to do this before we call SetParent since
  // during the SetParent call we will otherwise get a WA_ACTIVATE call
  // that causes us to steal the current focus.
  ModifyStyle(WS_POPUP, style, 0);

  // Now apply the parenting and style
  if (parent)
    SetParent(parent);

  ::ShowWindow(tab_contents_->GetNativeView(), SW_SHOWNA);
  return true;
}

bool ExternalTabContainer::Uninitialize(HWND window) {
  if (::IsWindow(window)) {
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManager(window);
    if (focus_manager) {
      focus_manager->RemoveKeystrokeListener(this);
    }
  }

  root_view_.RemoveAllChildViews(true);
  if (tab_contents_) {
    NavigationController* controller = tab_contents_->controller();
    DCHECK(controller);

    NotificationService::current()->Notify(
        NotificationType::EXTERNAL_TAB_CLOSED,
        Source<NavigationController>(controller),
        Details<ExternalTabContainer>(this));

    tab_contents_->set_delegate(NULL);
    tab_contents_->CloseContents();
    // WARNING: tab_contents_ has likely been deleted.
    tab_contents_ = NULL;
  }

  return true;
}

void ExternalTabContainer::OnFinalMessage(HWND window) {
  Uninitialize(window);
  delete this;
}

LRESULT ExternalTabContainer::OnSize(UINT, WPARAM, LPARAM, BOOL& handled) {
  if (tab_contents_) {
    RECT client_rect = {0};
    GetClientRect(&client_rect);
    ::SetWindowPos(tab_contents_->GetNativeView(), NULL, client_rect.left,
                   client_rect.top, client_rect.right - client_rect.left,
                   client_rect.bottom - client_rect.top, SWP_NOZORDER);
  }
  return 0;
}

LRESULT ExternalTabContainer::OnSetFocus(UINT msg, WPARAM wp, LPARAM lp,
                                         BOOL& handled) {
  if (automation_) {
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManager(GetNativeView());
    DCHECK(focus_manager);
    if (focus_manager) {
      focus_manager->ClearFocus();
      automation_->Send(new AutomationMsg_TabbedOut(0,
          win_util::IsShiftPressed()));
    }
  }

  return 0;
}

void ExternalTabContainer::OpenURLFromTab(TabContents* source,
                           const GURL& url,
                           const GURL& referrer,
                           WindowOpenDisposition disposition,
                           PageTransition::Type transition) {
  switch (disposition) {
    case CURRENT_TAB:
    case SINGLETON_TAB:
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
        new AutomationMsg_NavigationStateChanged(0, changed_flags));
  }
}

void ExternalTabContainer::ReplaceContents(TabContents* source,
                                           TabContents* new_contents) {
}

void ExternalTabContainer::AddNewContents(TabContents* source,
                            TabContents* new_contents,
                            WindowOpenDisposition disposition,
                            const gfx::Rect& initial_pos,
                            bool user_gesture) {
}

void ExternalTabContainer::ActivateContents(TabContents* contents) {
}

void ExternalTabContainer::LoadingStateChanged(TabContents* source) {
}

void ExternalTabContainer::CloseContents(TabContents* source) {
}

void ExternalTabContainer::MoveContents(TabContents* source,
                                        const gfx::Rect& pos) {
}

bool ExternalTabContainer::IsPopup(TabContents* source) {
  return false;
}

void ExternalTabContainer::URLStarredChanged(TabContents* source,
                                             bool starred) {
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

void ExternalTabContainer::ForwardMessageToExternalHost(
    const std::string& message, const std::string& origin,
    const std::string& target) {
  if(automation_) {
    automation_->Send(
        new AutomationMsg_ForwardMessageToExternalHost(0, message, origin,
                                                       target));
  }
}

void ExternalTabContainer::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  static const int kHttpClientErrorStart = 400;
  static const int kHttpServerErrorEnd = 510;

  switch (type.value) {
    case NotificationType::NAV_ENTRY_COMMITTED:
      if (ignore_next_load_notification_) {
        ignore_next_load_notification_ = false;
        return;
      }

      if (automation_) {
        const NavigationController::LoadCommittedDetails* commit =
            Details<NavigationController::LoadCommittedDetails>(details).ptr();

        if (commit->http_status_code >= kHttpClientErrorStart &&
            commit->http_status_code <= kHttpServerErrorEnd) {
          automation_->Send(new AutomationMsg_NavigationFailed(
              0, commit->http_status_code, commit->entry->url()));

          ignore_next_load_notification_ = true;
        } else {
          // When the previous entry index is invalid, it will be -1, which
          // will still make the computation come out right (navigating to the
          // 0th entry will be +1).
          automation_->Send(new AutomationMsg_DidNavigate(
              0, commit->type,
              commit->previous_entry_index -
                  tab_contents_->controller()->GetLastCommittedEntryIndex(),
              commit->entry->url()));
        }
      }
      break;
    case NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR: {
      if (automation_) {
        const ProvisionalLoadDetails* load_details =
            Details<ProvisionalLoadDetails>(details).ptr();
        automation_->Send(new AutomationMsg_NavigationFailed(
            0, load_details->error_code(), load_details->url()));

        ignore_next_load_notification_ = true;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void ExternalTabContainer::GetBounds(gfx::Rect* out,
                                     bool including_frame) const {
  CRect crect;
  GetWindowRect(&crect);
  *out = gfx::Rect(crect);
}

void ExternalTabContainer::MoveToFront(bool should_activate) {
}

gfx::NativeView ExternalTabContainer::GetNativeView() const {
  return m_hWnd;
}

void ExternalTabContainer::PaintNow(const gfx::Rect& update_rect) {
  RECT native_update_rect = update_rect.ToRECT();
  RedrawWindow(&native_update_rect,
               NULL,
               RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
}

views::RootView* ExternalTabContainer::GetRootView() {
  return const_cast<views::RootView*>(&root_view_);
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
      (wparam >= VK_F1 && wparam <= VK_F24) ||
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

void ExternalTabContainer::SetInitialFocus(bool reverse) {
  DCHECK(tab_contents_);
  if (tab_contents_) {
    tab_contents_->SetInitialFocus(reverse);
  }
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
