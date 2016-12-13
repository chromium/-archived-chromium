// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_tab_container.h"

#include <string>

#include "app/l10n_util.h"
#include "app/win_util.h"
#include "base/logging.h"
#include "base/win_util.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/load_notification_details.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/provisional_load_details.h"
#include "chrome/browser/views/tab_contents/render_view_context_menu_external_win.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tab_contents/tab_contents_container.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/automation/automation_messages.h"

#include "grit/generated_resources.h"

static const wchar_t kWindowObjectKey[] = L"ChromeWindowObject";

// TODO(sanjeevr): The external_accel_table_ and external_accel_entry_count_
// member variables are now obsolete and we don't use them.
// We need to remove them.
ExternalTabContainer::ExternalTabContainer(
    AutomationProvider* automation, AutomationResourceMessageFilter* filter)
    : automation_(automation),
      tab_contents_(NULL),
      external_accel_table_(NULL),
      external_accel_entry_count_(0),
      tab_contents_container_(NULL),
      tab_handle_(0),
      ignore_next_load_notification_(false),
      automation_resource_message_filter_(filter),
      load_requests_via_automation_(false) {
}

ExternalTabContainer::~ExternalTabContainer() {
  Uninitialize(GetNativeView());
}

bool ExternalTabContainer::Init(Profile* profile,
                                HWND parent,
                                const gfx::Rect& bounds,
                                DWORD style) {
  if (IsWindow()) {
    NOTREACHED();
    return false;
  }

  set_window_style(WS_POPUP);
  views::WidgetWin::Init(NULL, bounds);
  if (!IsWindow()) {
    NOTREACHED();
    return false;
  }
  // TODO(jcampan): limit focus traversal to contents.

  // We don't ever remove the prop because the lifetime of this object
  // is the same as the lifetime of the window
  SetProp(GetNativeView(), kWindowObjectKey, this);

  tab_contents_ = new TabContents(profile, NULL, MSG_ROUTING_NONE, NULL);
  tab_contents_->set_delegate(this);
  tab_contents_->render_view_host()->AllowBindings(
      BindingsPolicy::EXTERNAL_HOST);

  // Create a TabContentsContainer to handle focus cycling using Tab and
  // Shift-Tab.
  tab_contents_container_ = new TabContentsContainer;
  SetContentsView(tab_contents_container_);

  // Note that SetTabContents must be called after AddChildView is called
  tab_contents_container_->ChangeTabContents(tab_contents_);

  NavigationController* controller = &tab_contents_->controller();
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(controller));
  registrar_.Add(this, NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR,
                 Source<NavigationController>(controller));
  registrar_.Add(this, NotificationType::LOAD_STOP,
                 Source<NavigationController>(controller));
  registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB,
                 Source<TabContents>(tab_contents_));
  registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_DELETED,
                 Source<TabContents>(tab_contents_));

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
  SetWindowLong(GWL_STYLE, (GetWindowLong(GWL_STYLE) & ~WS_POPUP) | style);

  // Now apply the parenting and style
  if (parent)
    SetParent(GetNativeView(), parent);

  ::ShowWindow(tab_contents_->GetNativeView(), SW_SHOWNA);

  disabled_context_menu_ids_.push_back(
      IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
  return true;
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

void ExternalTabContainer::FocusThroughTabTraversal(bool reverse) {
  DCHECK(tab_contents_);
  if (tab_contents_) {
    static_cast<TabContents*>(tab_contents_)->Focus();
    static_cast<TabContents*>(tab_contents_)->FocusThroughTabTraversal(reverse);
  }
}

// static
bool ExternalTabContainer::IsExternalTabContainer(HWND window) {
  if (GetProp(window, kWindowObjectKey) != NULL)
    return true;

  return false;
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

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, TabContentsDelegate implementation:

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
        automation_->Send(new AutomationMsg_OpenURL(0, tab_handle_,
                                                    url, disposition));
      }
      break;
    default:
      break;
  }
}

void ExternalTabContainer::NavigationStateChanged(const TabContents* source,
                                                  unsigned changed_flags) {
  if (automation_) {
    automation_->Send(new AutomationMsg_NavigationStateChanged(0, tab_handle_,
                                                               changed_flags));
  }
}

void ExternalTabContainer::AddNewContents(TabContents* source,
                            TabContents* new_contents,
                            WindowOpenDisposition disposition,
                            const gfx::Rect& initial_pos,
                            bool user_gesture) {
  if (disposition == NEW_POPUP || disposition == NEW_WINDOW) {
    Browser::BuildPopupWindowHelper(source, new_contents, initial_pos,
                                    Browser::TYPE_POPUP,
                                    tab_contents_->profile(), true);
  } else {
    NOTREACHED();
  }
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
        new AutomationMsg_UpdateTargetUrl(0, tab_handle_, url_string));
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
  if (automation_) {
    automation_->Send(
        new AutomationMsg_ForwardMessageToExternalHost(0, tab_handle_,
            message, origin, target));
  }
}

bool ExternalTabContainer::TakeFocus(bool reverse) {
  if (automation_) {
    automation_->Send(new AutomationMsg_TabbedOut(0, tab_handle_,
        win_util::IsShiftPressed()));
  }

  return true;
}

bool ExternalTabContainer::HandleContextMenu(const ContextMenuParams& params) {
  if (!automation_) {
    NOTREACHED();
    return false;
  }

  external_context_menu_.reset(
      new RenderViewContextMenuExternalWin(tab_contents(),
                                           params,
                                           disabled_context_menu_ids_));
  external_context_menu_->Init();

  POINT screen_pt = { params.x, params.y };
  MapWindowPoints(GetNativeView(), HWND_DESKTOP, &screen_pt, 1);

  bool rtl = l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT;
  automation_->Send(
      new AutomationMsg_ForwardContextMenuToExternalHost(0, tab_handle_,
          external_context_menu_->GetMenuHandle(), screen_pt.x, screen_pt.y,
          rtl ? TPM_RIGHTALIGN : TPM_LEFTALIGN));

  return true;
}

bool ExternalTabContainer::ExecuteContextMenuCommand(int command) {
  if (!external_context_menu_.get()) {
    NOTREACHED();
    return false;
  }

  external_context_menu_->ExecuteCommand(command);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, NotificationObserver implementation:

void ExternalTabContainer::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  if (!automation_)
    return;

  static const int kHttpClientErrorStart = 400;
  static const int kHttpServerErrorEnd = 510;

  switch (type.value) {
    case NotificationType::LOAD_STOP: {
        const LoadNotificationDetails* load =
            Details<LoadNotificationDetails>(details).ptr();
        if (load != NULL && PageTransition::IsMainFrame(load->origin())) {
          automation_->Send(new AutomationMsg_TabLoaded(0, tab_handle_,
                                                        load->url()));
        }
        break;
      }
    case NotificationType::NAV_ENTRY_COMMITTED: {
        if (ignore_next_load_notification_) {
          ignore_next_load_notification_ = false;
          return;
        }

        const NavigationController::LoadCommittedDetails* commit =
            Details<NavigationController::LoadCommittedDetails>(details).ptr();

        if (commit->http_status_code >= kHttpClientErrorStart &&
            commit->http_status_code <= kHttpServerErrorEnd) {
          automation_->Send(new AutomationMsg_NavigationFailed(
              0, tab_handle_, commit->http_status_code, commit->entry->url()));

          ignore_next_load_notification_ = true;
        } else {
          // When the previous entry index is invalid, it will be -1, which
          // will still make the computation come out right (navigating to the
          // 0th entry will be +1).
          automation_->Send(new AutomationMsg_DidNavigate(
              0, tab_handle_, commit->type,
              commit->previous_entry_index -
                  tab_contents_->controller().last_committed_entry_index(),
              commit->entry->url()));
        }
        break;
      }
    case NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR: {
      const ProvisionalLoadDetails* load_details =
          Details<ProvisionalLoadDetails>(details).ptr();
      automation_->Send(new AutomationMsg_NavigationFailed(
          0, tab_handle_, load_details->error_code(), load_details->url()));

      ignore_next_load_notification_ = true;
      break;
    }
    case NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      if (load_requests_via_automation_) {
        RenderViewHost* rvh = Details<RenderViewHost>(details).ptr();
        if (rvh) {
          AutomationResourceMessageFilter::RegisterRenderView(
              rvh->process()->pid(), rvh->routing_id(), tab_handle_,
              automation_resource_message_filter_);
        }
      }
      break;
    }
    case NotificationType::RENDER_VIEW_HOST_DELETED: {
      if (load_requests_via_automation_) {
        RenderViewHost* rvh = Details<RenderViewHost>(details).ptr();
        if (rvh) {
          AutomationResourceMessageFilter::UnRegisterRenderView(
              rvh->process()->pid(), rvh->routing_id());
        }
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, views::WidgetWin overrides:

void ExternalTabContainer::OnDestroy() {
  Uninitialize(GetNativeView());
  WidgetWin::OnDestroy();
}

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, views::KeystrokeListener implementation:

bool ExternalTabContainer::ProcessKeyStroke(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam) {
  if (!automation_) {
    return false;
  }
  if ((wparam == VK_TAB) && !win_util::IsCtrlPressed()) {
    // Tabs are handled separately (except if this is Ctrl-Tab or
    // Ctrl-Shift-Tab)
    return false;
  }

  unsigned int flags = HIWORD(lparam);
  bool alt = (flags & KF_ALTDOWN) != 0;
  if (!alt && (message == WM_SYSKEYUP || message == WM_KEYUP)) {
    // In case the Alt key is being released.
    alt = (wparam == VK_MENU);
  }

  if ((flags & KF_EXTENDED) || alt || (wparam >= VK_F1 && wparam <= VK_F24) ||
      wparam == VK_ESCAPE || wparam == VK_RETURN ||
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
    automation_->Send(new AutomationMsg_HandleAccelerator(0, tab_handle_, msg));
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, private:

void ExternalTabContainer::Uninitialize(HWND window) {
  registrar_.RemoveAll();
  if (tab_contents_) {
    NotificationService::current()->Notify(
        NotificationType::EXTERNAL_TAB_CLOSED,
        Source<NavigationController>(&tab_contents_->controller()),
        Details<ExternalTabContainer>(this));

    delete tab_contents_;
    tab_contents_ = NULL;
  }
}
