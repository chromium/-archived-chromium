// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include "base/path_service.h"
#include "chrome/app/chrome_dll_resource.h" 
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/automation/url_request_failed_dns_job.h"
#include "chrome/browser/automation/url_request_mock_http_job.h"
#include "chrome/browser/automation/url_request_slow_download_job.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/external_tab_container.h"
#include "chrome/browser/find_notification_details.h"
#include "chrome/browser/login_prompt.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/ssl_manager.h"
#include "chrome/browser/ssl_blocking_page.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/web_contents_view.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/views/app_modal_dialog_delegate.h"
#include "chrome/views/window.h"
#include "net/base/cookie_monster.h"
#include "net/url_request/url_request_filter.h"

using base::Time;

class InitialLoadObserver : public NotificationObserver {
 public:
  InitialLoadObserver(size_t tab_count, AutomationProvider* automation)
      : outstanding_tab_count_(tab_count),
        automation_(automation) {
    if (outstanding_tab_count_ > 0) {
      NotificationService* service = NotificationService::current();
      registrar_.Add(this, NOTIFY_LOAD_START,
                     NotificationService::AllSources());
      registrar_.Add(this, NOTIFY_LOAD_STOP,
                     NotificationService::AllSources());
    }
  }

  ~InitialLoadObserver() {
  }

  void ConditionMet() {
    registrar_.RemoveAll();
    automation_->Send(new AutomationMsg_InitialLoadsComplete(0));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NOTIFY_LOAD_START) {
      if (outstanding_tab_count_ > loading_tabs_.size())
        loading_tabs_.insert(source.map_key());
    } else if (type == NOTIFY_LOAD_STOP) {
      if (outstanding_tab_count_ > finished_tabs_.size()) {
        if (loading_tabs_.find(source.map_key()) != loading_tabs_.end())
          finished_tabs_.insert(source.map_key());
        if (outstanding_tab_count_ == finished_tabs_.size())
          ConditionMet();
      }
    } else {
      NOTREACHED();
    }
  }

 private:
  typedef std::set<uintptr_t> TabSet;

  NotificationRegistrar registrar_;

  AutomationProvider* automation_;
  size_t outstanding_tab_count_;
  TabSet loading_tabs_;
  TabSet finished_tabs_;
};

// Watches for NewTabUI page loads for performance timing purposes.
class NewTabUILoadObserver : public NotificationObserver {
 public:
  explicit NewTabUILoadObserver(AutomationProvider* automation)
      : automation_(automation) {
    NotificationService::current()->
        AddObserver(this, NOTIFY_INITIAL_NEW_TAB_UI_LOAD,
                    NotificationService::AllSources());
  }

  ~NewTabUILoadObserver() {
    Unregister();
  }

  void Unregister() {
    NotificationService::current()->
        RemoveObserver(this, NOTIFY_INITIAL_NEW_TAB_UI_LOAD,
                       NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NOTIFY_INITIAL_NEW_TAB_UI_LOAD) {
      Details<int> load_time(details);
      automation_->Send(
          new AutomationMsg_InitialNewTabUILoadComplete(0, *load_time.ptr()));
    } else {
      NOTREACHED();
    }
  }

 private:
  AutomationProvider* automation_;
};

class NavigationControllerRestoredObserver : public NotificationObserver {
 public:
  NavigationControllerRestoredObserver(AutomationProvider* automation,
                                       NavigationController* controller,
                                       int32 routing_id)
      : automation_(automation),
        controller_(controller),
        routing_id_(routing_id) {
    if (FinishedRestoring()) {
      registered_ = false;
      SendDone();
    } else {
      registered_ = true;
      NotificationService* service = NotificationService::current();
      service->AddObserver(this, NOTIFY_LOAD_STOP,
                           NotificationService::AllSources());
    }
  }

  ~NavigationControllerRestoredObserver() {
    if (registered_)
      Unregister();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (FinishedRestoring()) {
      SendDone();
      Unregister();
    }
  }

 private:
  void Unregister() {
    NotificationService* service = NotificationService::current();
    service->RemoveObserver(this, NOTIFY_LOAD_STOP,
                            NotificationService::AllSources());
    registered_ = false;
  }

  bool FinishedRestoring() {
    return (!controller_->needs_reload() && !controller_->GetPendingEntry() &&
            !controller_->active_contents()->is_loading());
  }

  void SendDone() {
    automation_->Send(new AutomationMsg_TabFinishedRestoring(routing_id_));
  }

  bool registered_;
  AutomationProvider* automation_;
  NavigationController* controller_;
  const int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerRestoredObserver);
};

class NavigationNotificationObserver : public NotificationObserver {
 public:
  NavigationNotificationObserver(NavigationController* controller,
                                 AutomationProvider* automation,
                                 IPC::Message* completed_response,
                                 IPC::Message* auth_needed_response)
    : automation_(automation),
      completed_response_(completed_response),
      auth_needed_response_(auth_needed_response),
      controller_(controller),
      navigation_started_(false) {
    NotificationService* service = NotificationService::current();
    service->AddObserver(this, NOTIFY_NAV_ENTRY_COMMITTED,
                         Source<NavigationController>(controller_));
    service->AddObserver(this, NOTIFY_LOAD_START,
                         Source<NavigationController>(controller_));
    service->AddObserver(this, NOTIFY_LOAD_STOP,
                         Source<NavigationController>(controller_));
    service->AddObserver(this, NOTIFY_AUTH_NEEDED,
                         Source<NavigationController>(controller_));
    service->AddObserver(this, NOTIFY_AUTH_SUPPLIED,
                         Source<NavigationController>(controller_));
  }

  ~NavigationNotificationObserver() {
    if (completed_response_) delete completed_response_;
    if (auth_needed_response_) delete auth_needed_response_;
    Unregister();
  }

  void ConditionMet(IPC::Message** response) {
    if (*response) {
      automation_->Send(*response);
      *response = NULL;  // *response is deleted by Send.
    }
    automation_->RemoveNavigationStatusListener(this);
    delete this;
  }

  void Unregister() {
    NotificationService* service = NotificationService::current();
    service->RemoveObserver(this, NOTIFY_NAV_ENTRY_COMMITTED,
                            Source<NavigationController>(controller_));
    service->RemoveObserver(this, NOTIFY_LOAD_START,
                            Source<NavigationController>(controller_));
    service->RemoveObserver(this, NOTIFY_LOAD_STOP,
                            Source<NavigationController>(controller_));
    service->RemoveObserver(this, NOTIFY_AUTH_NEEDED,
                            Source<NavigationController>(controller_));
    service->RemoveObserver(this, NOTIFY_AUTH_SUPPLIED,
                            Source<NavigationController>(controller_));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    // We listen for 2 events to determine when the navigation started because:
    // - when this is used by the WaitForNavigation method, we might be invoked
    // afer the load has started (but not after the entry was committed, as
    // WaitForNavigation compares times of the last navigation).
    // - when this is used with a page requiring authentication, we will not get
    // a NOTIFY_NAV_ENTRY_COMMITTED until after we authenticate, so we need the
    // NOTIFY_LOAD_START.
    if (type == NOTIFY_NAV_ENTRY_COMMITTED || type == NOTIFY_LOAD_START) {
      navigation_started_ = true;
    } else if (type == NOTIFY_LOAD_STOP) {
      if (navigation_started_) {
        navigation_started_ = false;
        ConditionMet(&completed_response_);
      }
    } else if (type == NOTIFY_AUTH_SUPPLIED) {
      // The LoginHandler for this tab is no longer valid.
      automation_->RemoveLoginHandler(controller_);

      // Treat this as if navigation started again, since load start/stop don't
      // occur while authentication is ongoing.
      navigation_started_ = true;
    } else if (type == NOTIFY_AUTH_NEEDED) {
      if (navigation_started_) {
        // Remember the login handler that wants authentication.
        LoginHandler* handler =
            Details<LoginNotificationDetails>(details)->handler();
        automation_->AddLoginHandler(controller_, handler);

        // Respond that authentication is needed.
        navigation_started_ = false;
        ConditionMet(&auth_needed_response_);
      } else {
        NOTREACHED();
      }
    } else {
      NOTREACHED();
    }
  }

 private:
  AutomationProvider* automation_;
  IPC::Message* completed_response_;
  IPC::Message* auth_needed_response_;
  NavigationController* controller_;
  bool navigation_started_;
};

class TabStripNotificationObserver : public NotificationObserver {
 public:
  TabStripNotificationObserver(Browser* parent, NotificationType notification,
    AutomationProvider* automation, int32 routing_id)
    : automation_(automation),
      notification_(notification),
      parent_(parent),
      routing_id_(routing_id) {
    NotificationService::current()->
        AddObserver(this, notification_, NotificationService::AllSources());
  }

  virtual ~TabStripNotificationObserver() {
    Unregister();
  }

  void Unregister() {
    NotificationService::current()->
        RemoveObserver(this, notification_, NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == notification_) {
      ObserveTab(Source<NavigationController>(source).ptr());

      // If verified, no need to observe anymore
      automation_->RemoveTabStripObserver(this);
      delete this;
    } else {
      NOTREACHED();
    }
  }

  virtual void ObserveTab(NavigationController* controller) = 0;

 protected:
  AutomationProvider* automation_;
  Browser* parent_;
  NotificationType notification_;
  int32 routing_id_;
};

class TabAppendedNotificationObserver : public TabStripNotificationObserver {
 public:
  TabAppendedNotificationObserver(Browser* parent,
      AutomationProvider* automation, int32 routing_id)
      : TabStripNotificationObserver(parent, NOTIFY_TAB_PARENTED, automation,
                                     routing_id) {
  }

  virtual void ObserveTab(NavigationController* controller) {
    int tab_index =
        automation_->GetIndexForNavigationController(controller, parent_);
    if (tab_index == TabStripModel::kNoTab) {
      // This tab notification doesn't belong to the parent_
      return;
    }

    // Give the same response even if auth is needed, since it doesn't matter.
    automation_->AddNavigationStatusListener(controller,
        new AutomationMsg_AppendTabResponse(routing_id_, tab_index),
        new AutomationMsg_AppendTabResponse(routing_id_, tab_index));
  }
};

class TabClosedNotificationObserver : public TabStripNotificationObserver {
 public:
  TabClosedNotificationObserver(Browser* parent,
                                AutomationProvider* automation,
                                int32 routing_id,
                                bool wait_until_closed)
      : TabStripNotificationObserver(parent,
                                     wait_until_closed ? NOTIFY_TAB_CLOSED :
                                                         NOTIFY_TAB_CLOSING,
                                     automation,
                                     routing_id) {
  }

  virtual void ObserveTab(NavigationController* controller) {
    automation_->Send(new AutomationMsg_CloseTabResponse(routing_id_, true));
  }
};

class BrowserClosedNotificationObserver : public NotificationObserver {
 public:
  BrowserClosedNotificationObserver(Browser* browser,
                                    AutomationProvider* automation,
                                    int32 routing_id)
      : automation_(automation),
        routing_id_(routing_id) {
    NotificationService::current()->
        AddObserver(this, NOTIFY_BROWSER_CLOSED, Source<Browser>(browser));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NOTIFY_BROWSER_CLOSED);
    Details<bool> close_app(details);
    automation_->Send(
        new AutomationMsg_CloseBrowserResponse(routing_id_,
                                               true,
                                               *(close_app.ptr())));
    delete this;
  }

 private:
  AutomationProvider* automation_;
  int32 routing_id_;
};

class FindInPageNotificationObserver : public NotificationObserver {
 public:
  FindInPageNotificationObserver(AutomationProvider* automation,
                                 TabContents* parent_tab,
                                 int32 routing_id)
      : automation_(automation),
        parent_tab_(parent_tab),
        routing_id_(routing_id),
        active_match_ordinal_(-1) {
    NotificationService::current()->
        AddObserver(this, NOTIFY_FIND_RESULT_AVAILABLE,
                    Source<TabContents>(parent_tab_));
  }

  ~FindInPageNotificationObserver() {
    Unregister();
  }

  void Unregister() {
    NotificationService::current()->
        RemoveObserver(this, NOTIFY_FIND_RESULT_AVAILABLE,
                       Source<TabContents>(parent_tab_));
  }

  virtual void Observe(NotificationType type, const NotificationSource& source,
      const NotificationDetails& details) {
    if (type == NOTIFY_FIND_RESULT_AVAILABLE) {
      Details<FindNotificationDetails> find_details(details);
      if (find_details->request_id() == kFindInPageRequestId) {
        // We get multiple responses and one of those will contain the ordinal.
        // This message comes to us before the final update is sent.
        if (find_details->active_match_ordinal() > -1)
          active_match_ordinal_ = find_details->active_match_ordinal();
        if (find_details->final_update()) {
          automation_->Send(new AutomationMsg_FindInPageResponse2(routing_id_,
              active_match_ordinal_,
              find_details->number_of_matches()));
        } else {
          DLOG(INFO) << "Ignoring, since we only care about the final message";
        }
      }
    } else {
      NOTREACHED();
    }
  }

  // The Find mechanism is over asynchronous IPC, so a search is kicked off and
  // we wait for notification to find out what the results are. As the user is
  // typing, new search requests can be issued and the Request ID helps us make
  // sense of whether this is the current request or an old one. The unit tests,
  // however, which uses this constant issues only one search at a time, so we
  // don't need a rolling id to identify each search. But, we still need to
  // specify one, so we just use a fixed one - its value does not matter.
  static const int kFindInPageRequestId;
 private:
  AutomationProvider* automation_;
  TabContents* parent_tab_;
  int32 routing_id_;
  // We will at some point (before final update) be notified of the ordinal and
  // we need to preserve it so we can send it later.
  int active_match_ordinal_;
};

const int FindInPageNotificationObserver::kFindInPageRequestId = -1;

class DomOperationNotificationObserver : public NotificationObserver {
 public:
  explicit DomOperationNotificationObserver(AutomationProvider* automation)
      : automation_(automation) {
    NotificationService::current()->
        AddObserver(this, NOTIFY_DOM_OPERATION_RESPONSE,
                    NotificationService::AllSources());
  }

  ~DomOperationNotificationObserver() {
    NotificationService::current()->
        RemoveObserver(this, NOTIFY_DOM_OPERATION_RESPONSE,
                       NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type, const NotificationSource& source,
      const NotificationDetails& details) {
    if (NOTIFY_DOM_OPERATION_RESPONSE == type) {
      Details<DomOperationNotificationDetails> dom_op_details(details);
      automation_->Send(new AutomationMsg_DomOperationResponse(
          dom_op_details->automation_id(),
          dom_op_details->json()));
    }
  }
 private:
  AutomationProvider* automation_;
};

class DomInspectorNotificationObserver : public NotificationObserver {
 public:
  explicit DomInspectorNotificationObserver(AutomationProvider* automation)
      : automation_(automation) {
    NotificationService::current()->
        AddObserver(this, NOTIFY_DOM_INSPECT_ELEMENT_RESPONSE,
                    NotificationService::AllSources());
  }

  ~DomInspectorNotificationObserver() {
    NotificationService::current()->
        RemoveObserver(this, NOTIFY_DOM_INSPECT_ELEMENT_RESPONSE,
                       NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type, const NotificationSource& source,
      const NotificationDetails& details) {
    if (NOTIFY_DOM_INSPECT_ELEMENT_RESPONSE == type) {
      Details<int> dom_inspect_details(details);
      automation_->ReceivedInspectElementResponse(*(dom_inspect_details.ptr()));
    }
  }

 private:
  AutomationProvider* automation_;
};

class DocumentPrintedNotificationObserver : public NotificationObserver {
 public:
  DocumentPrintedNotificationObserver(AutomationProvider* automation,
                                      int32 routing_id)
      : automation_(automation),
        routing_id_(routing_id),
        success_(false) {
    NotificationService::current()->
        AddObserver(this, NOTIFY_PRINT_JOB_EVENT,
                    NotificationService::AllSources());
  }

  ~DocumentPrintedNotificationObserver() {
    automation_->Send(
        new AutomationMsg_PrintNowResponse(routing_id_, success_));
    automation_->RemoveNavigationStatusListener(this);
    NotificationService::current()->
        RemoveObserver(this, NOTIFY_PRINT_JOB_EVENT,
                       NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details) {
    using namespace printing;
    DCHECK(type == NOTIFY_PRINT_JOB_EVENT);
    switch (Details<JobEventDetails>(details)->type()) {
      case JobEventDetails::JOB_DONE: {
        // Succeeded.
        success_ = true;
        delete this;
        break;
      }
      case JobEventDetails::USER_INIT_CANCELED:
      case JobEventDetails::FAILED: {
        // Failed.
        delete this;
        break;
      }
      case JobEventDetails::NEW_DOC:
      case JobEventDetails::USER_INIT_DONE:
      case JobEventDetails::DEFAULT_INIT_DONE:
      case JobEventDetails::NEW_PAGE:
      case JobEventDetails::PAGE_DONE:
      case JobEventDetails::DOC_DONE:
      case JobEventDetails::ALL_PAGES_REQUESTED: {
        // Don't care.
        break;
      }
      default: {
        NOTREACHED();
        break;
      }
    }
  }

 private:
  scoped_refptr<AutomationProvider> automation_;
  int32 routing_id_;
  bool success_;
};

class AutomationInterstitialPage : public InterstitialPage {
 public:
  AutomationInterstitialPage(WebContents* tab,
                             const GURL& url,
                             const std::string& contents)
      : InterstitialPage(tab, true, url),
        contents_(contents) {
  }

  virtual std::string GetHTMLContents() { return contents_; }

 private:
  std::string contents_;
  
  DISALLOW_COPY_AND_ASSIGN(AutomationInterstitialPage);
};

AutomationProvider::AutomationProvider(Profile* profile)
    : redirect_query_(0),
      profile_(profile) {
  browser_tracker_.reset(new AutomationBrowserTracker(this));
  window_tracker_.reset(new AutomationWindowTracker(this));
  tab_tracker_.reset(new AutomationTabTracker(this));
  autocomplete_edit_tracker_.reset(
      new AutomationAutocompleteEditTracker(this));
  cwindow_tracker_.reset(new AutomationConstrainedWindowTracker(this));
  new_tab_ui_load_observer_.reset(new NewTabUILoadObserver(this));
  dom_operation_observer_.reset(new DomOperationNotificationObserver(this));
  dom_inspector_observer_.reset(new DomInspectorNotificationObserver(this));
}

AutomationProvider::~AutomationProvider() {
  // Make sure that any outstanding NotificationObservers also get destroyed.
  ObserverList<NotificationObserver>::Iterator it(notification_observer_list_);
  NotificationObserver* observer;
  while ((observer = it.GetNext()) != NULL)
    delete observer;
}

void AutomationProvider::ConnectToChannel(const std::wstring& channel_id) {
  channel_.reset(
    new IPC::ChannelProxy(channel_id, IPC::Channel::MODE_CLIENT, this, NULL,
                          g_browser_process->io_thread()->message_loop()));
  channel_->Send(new AutomationMsg_Hello(0));
}

void AutomationProvider::SetExpectedTabCount(size_t expected_tabs) {
  if (expected_tabs == 0) {
    Send(new AutomationMsg_InitialLoadsComplete(0));
  } else {
    initial_load_observer_.reset(new InitialLoadObserver(expected_tabs, this));
  }
}

NotificationObserver* AutomationProvider::AddNavigationStatusListener(
    NavigationController* tab, IPC::Message* completed_response,
    IPC::Message* auth_needed_response) {
  NotificationObserver* observer =
    new NavigationNotificationObserver(tab, this, completed_response,
                                       auth_needed_response);
  notification_observer_list_.AddObserver(observer);

  return observer;
}

void AutomationProvider::RemoveNavigationStatusListener(
    NotificationObserver* obs) {
  notification_observer_list_.RemoveObserver(obs);
}

NotificationObserver* AutomationProvider::AddTabStripObserver(
    Browser* parent, int32 routing_id) {
  NotificationObserver* observer = new
    TabAppendedNotificationObserver(parent, this, routing_id);
  notification_observer_list_.AddObserver(observer);

  return observer;
}

void AutomationProvider::RemoveTabStripObserver(NotificationObserver* obs) {
  notification_observer_list_.RemoveObserver(obs);
}

void AutomationProvider::AddLoginHandler(NavigationController* tab,
                                         LoginHandler* handler) {
  login_handler_map_[tab] = handler;
}

void AutomationProvider::RemoveLoginHandler(NavigationController* tab) {
  DCHECK(login_handler_map_[tab]);
  login_handler_map_.erase(tab);
}

int AutomationProvider::GetIndexForNavigationController(
    const NavigationController* controller, const Browser* parent) const {
  DCHECK(parent);
  return parent->GetIndexOfController(controller);
}

void AutomationProvider::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(AutomationProvider, message)
    IPC_MESSAGE_HANDLER(AutomationMsg_CloseBrowserRequest, CloseBrowser)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActivateTabRequest, ActivateTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActiveTabIndexRequest, GetActiveTabIndex)
    IPC_MESSAGE_HANDLER(AutomationMsg_AppendTabRequest, AppendTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_CloseTabRequest, CloseTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetCookiesRequest, GetCookies)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetCookieRequest, SetCookie)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigateToURLRequest, NavigateToURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationAsyncRequest, NavigationAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_GoBackRequest, GoBack)
    IPC_MESSAGE_HANDLER(AutomationMsg_GoForwardRequest, GoForward)
    IPC_MESSAGE_HANDLER(AutomationMsg_ReloadRequest, Reload)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetAuthRequest, SetAuth)
    IPC_MESSAGE_HANDLER(AutomationMsg_CancelAuthRequest, CancelAuth)
    IPC_MESSAGE_HANDLER(AutomationMsg_NeedsAuthRequest, NeedsAuth)
    IPC_MESSAGE_HANDLER(AutomationMsg_RedirectsFromRequest, GetRedirectsFrom)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserWindowCountRequest,
                        GetBrowserWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserWindowRequest, GetBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_LastActiveBrowserWindowRequest,
                        GetLastActiveBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActiveWindowRequest, GetActiveWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsWindowActiveRequest, IsWindowActive)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActivateWindow, ActivateWindow);
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowHWNDRequest, GetWindowHWND)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowExecuteCommandRequest,
                        ExecuteBrowserCommand)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowViewBoundsRequest,
                        WindowGetViewBounds)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetWindowVisibleRequest, SetWindowVisible)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowClickRequest, WindowSimulateClick)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowKeyPressRequest,
                        WindowSimulateKeyPress)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowDragRequest, WindowSimulateDrag)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabCountRequest, GetTabCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabRequest, GetTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabHWNDRequest, GetTabHWND)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabProcessIDRequest, GetTabProcessID)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabTitleRequest, GetTabTitle)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabURLRequest, GetTabURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShelfVisibilityRequest,
                        GetShelfVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_HandleUnused, HandleUnused)
    IPC_MESSAGE_HANDLER(AutomationMsg_ApplyAcceleratorRequest, ApplyAccelerator)
    IPC_MESSAGE_HANDLER(AutomationMsg_DomOperationRequest, ExecuteJavascript)
    IPC_MESSAGE_HANDLER(AutomationMsg_ConstrainedWindowCountRequest,
                        GetConstrainedWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_ConstrainedWindowRequest,
                        GetConstrainedWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_ConstrainedTitleRequest,
                        GetConstrainedTitle)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindInPageRequest,
                        HandleFindInPageRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetFocusedViewIDRequest, GetFocusedViewID)
    IPC_MESSAGE_HANDLER(AutomationMsg_InspectElementRequest,
                        HandleInspectElementRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetFilteredInet,
                        SetFilteredInet);
    IPC_MESSAGE_HANDLER(AutomationMsg_DownloadDirectoryRequest,
                        GetDownloadDirectory);
    IPC_MESSAGE_HANDLER(AutomationMsg_OpenNewBrowserWindow,
                        OpenNewBrowserWindow);
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowForBrowserRequest,
                        GetWindowForBrowser);
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditForBrowserRequest,
                        GetAutocompleteEditForBrowser);
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserForWindowRequest,
                        GetBrowserForWindow);
    IPC_MESSAGE_HANDLER(AutomationMsg_CreateExternalTab, CreateExternalTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigateInExternalTabRequest,
                        NavigateInExternalTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShowInterstitialPageRequest,
                        ShowInterstitialPage);
    IPC_MESSAGE_HANDLER(AutomationMsg_HideInterstitialPageRequest,
                        HideInterstitialPage);
    IPC_MESSAGE_HANDLER(AutomationMsg_SetAcceleratorsForTab,
                        SetAcceleratorsForTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_ProcessUnhandledAccelerator,
                        ProcessUnhandledAccelerator)
    IPC_MESSAGE_HANDLER(AutomationMsg_WaitForTabToBeRestored,
                        WaitForTabToBeRestored)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetSecurityState,
                        GetSecurityState)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetPageType,
                        GetPageType)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActionOnSSLBlockingPage,
                        ActionOnSSLBlockingPage)
    IPC_MESSAGE_HANDLER(AutomationMsg_BringBrowserToFront, BringBrowserToFront)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsPageMenuCommandEnabled,
                        IsPageMenuCommandEnabled)
    IPC_MESSAGE_HANDLER(AutomationMsg_PrintNowRequest, PrintNow)
    IPC_MESSAGE_HANDLER(AutomationMsg_SavePageRequest, SavePage)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditGetTextRequest,
                        GetAutocompleteEditText)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditSetTextRequest,
                        SetAutocompleteEditText)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditIsQueryInProgressRequest,
                        AutocompleteEditIsQueryInProgress)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditGetMatchesRequest,
                        AutocompleteEditGetMatches)
    IPC_MESSAGE_HANDLER(AutomationMsg_ConstrainedWindowBoundsRequest,
                        GetConstrainedWindowBounds)
    IPC_MESSAGE_HANDLER(AutomationMsg_OpenFindInPageRequest,
                        HandleOpenFindInPageRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_HandleMessageFromExternalHost,
                        OnMessageFromExternalHost)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindRequest,
                        HandleFindRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindWindowVisibilityRequest,
                        GetFindWindowVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindWindowLocationRequest,
                        HandleFindWindowLocationRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_BookmarkBarVisibilityRequest,
                        GetBookmarkBarVisitility)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetSSLInfoBarCountRequest,
                        GetSSLInfoBarCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_ClickSSLInfoBarLinkRequest,
                        ClickSSLInfoBarLink)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetLastNavigationTimeRequest,
                        GetLastNavigationTime)
    IPC_MESSAGE_HANDLER(AutomationMsg_WaitForNavigationRequest,
                        WaitForNavigation)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetIntPreferenceRequest,
                        SetIntPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShowingAppModalDialogRequest,
                        GetShowingAppModalDialog)
    IPC_MESSAGE_HANDLER(AutomationMsg_ClickAppModalDialogButtonRequest,
                        ClickAppModalDialogButton)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetStringPreferenceRequest,
                        SetStringPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetBooleanPreferenceRequest,
                        GetBooleanPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetBooleanPreferenceRequest,
                        SetBooleanPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetPageCurrentEncodingRequest,
                        GetPageCurrentEncoding)
    IPC_MESSAGE_HANDLER(AutomationMsg_OverrideEncodingRequest,
                        OverrideEncoding)
  IPC_END_MESSAGE_MAP()
}

void AutomationProvider::ActivateTab(const IPC::Message& message,
                                     int handle, int at_index) {
  int status = -1;
  if (browser_tracker_->ContainsHandle(handle) && at_index > -1) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (at_index >= 0 && at_index < browser->tab_count()) {
      browser->SelectTabContentsAt(at_index, true);
      status = 0;
    }
  }
  Send(new AutomationMsg_ActivateTabResponse(message.routing_id(), status));
}

void AutomationProvider::AppendTab(const IPC::Message& message,
                                   int handle, const GURL& url) {
  int append_tab_response = -1;  // -1 is the error code
  NotificationObserver* observer = NULL;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    observer = AddTabStripObserver(browser, message.routing_id());
    TabContents* tab_contents =
        browser->AddTabWithURL(url, GURL(), PageTransition::TYPED, true, NULL);
    if (tab_contents) {
      append_tab_response =
          GetIndexForNavigationController(tab_contents->controller(), browser);
    }
  }

  if (append_tab_response < 0) {
    // The append tab failed. Remove the TabStripObserver
    if (observer) {
      RemoveTabStripObserver(observer);
      delete observer;
    }

    // This will be reached only if the tab could not be appended. In case of a
    // successful tab append, a successful navigation notification triggers the
    // send.
    Send(new AutomationMsg_AppendTabResponse(message.routing_id(),
                                             append_tab_response));
  }
}

void AutomationProvider::NavigateToURL(const IPC::Message& message,
                                       int handle, const GURL& url) {
  int status = AUTOMATION_MSG_NAVIGATION_ERROR;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);

    // Simulate what a user would do. Activate the tab and then navigate.
    // We could allow navigating in a background tab in future.
    Browser* browser = FindAndActivateTab(tab);

    if (browser) {
      AddNavigationStatusListener(tab,
        new AutomationMsg_NavigateToURLResponse(
            message.routing_id(), AUTOMATION_MSG_NAVIGATION_SUCCESS),
        new AutomationMsg_NavigateToURLResponse(
            message.routing_id(), AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED));
      // TODO(darin): avoid conversion to GURL
      browser->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);
      return;
    }
  }
  Send(new AutomationMsg_NavigateToURLResponse(
           message.routing_id(), AUTOMATION_MSG_NAVIGATION_ERROR));
}

void AutomationProvider::NavigationAsync(const IPC::Message& message,
                                         int handle, const GURL& url) {
  bool status = false;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);

    // Simulate what a user would do. Activate the tab and then navigate.
    // We could allow navigating in a background tab in future.
    Browser* browser = FindAndActivateTab(tab);

    if (browser) {
      // Don't add any listener unless a callback mechanism is desired.
      // TODO(vibhor): Do this if such a requirement arises in future.
      browser->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);
      status = true;
    }
  }

  Send(new AutomationMsg_NavigationAsyncResponse(message.routing_id(), status));
}

void AutomationProvider::GoBack(const IPC::Message& message, int handle) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->IsCommandEnabled(IDC_BACK)) {
      AddNavigationStatusListener(tab,
          new AutomationMsg_GoBackResponse(
              message.routing_id(), AUTOMATION_MSG_NAVIGATION_SUCCESS),
          new AutomationMsg_GoBackResponse(
              message.routing_id(), AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED));
      browser->GoBack();
      return;
    }
  }
  Send(new AutomationMsg_GoBackResponse(message.routing_id(),
                                        AUTOMATION_MSG_NAVIGATION_ERROR));
}

void AutomationProvider::GoForward(const IPC::Message& message, int handle) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->IsCommandEnabled(IDC_FORWARD)) {
      AddNavigationStatusListener(tab,
          new AutomationMsg_GoForwardResponse(
              message.routing_id(), AUTOMATION_MSG_NAVIGATION_SUCCESS),
          new AutomationMsg_GoForwardResponse(
              message.routing_id(), AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED));
      browser->GoForward();
      return;
    }
  }
  Send(new AutomationMsg_GoForwardResponse(message.routing_id(),
                                           AUTOMATION_MSG_NAVIGATION_ERROR));
}

void AutomationProvider::Reload(const IPC::Message& message, int handle) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->IsCommandEnabled(IDC_RELOAD)) {
      AddNavigationStatusListener(tab,
          new AutomationMsg_ReloadResponse(
              message.routing_id(), AUTOMATION_MSG_NAVIGATION_SUCCESS),
          new AutomationMsg_ReloadResponse(
              message.routing_id(), AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED));
      browser->Reload();
      return;
    }
  }
  Send(new AutomationMsg_ReloadResponse(message.routing_id(),
                                        AUTOMATION_MSG_NAVIGATION_ERROR));
}

void AutomationProvider::SetAuth(const IPC::Message& message, int tab_handle,
                                 const std::wstring& username,
                                 const std::wstring& password) {
  int status = -1;

  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // If auth is needed again after this, assume login has failed.  This is
      // not strictly correct, because a navigation can require both proxy and
      // server auth, but it should be OK for now.
      LoginHandler* handler = iter->second;
      AddNavigationStatusListener(tab,
        new AutomationMsg_SetAuthResponse(message.routing_id(), 0),
        new AutomationMsg_SetAuthResponse(message.routing_id(), -1));
      handler->SetAuth(username, password);
      status = 0;
    }
  }
  if (status < 0) {
    Send(new AutomationMsg_SetAuthResponse(message.routing_id(), status));
  }
}

void AutomationProvider::CancelAuth(const IPC::Message& message,
                                    int tab_handle) {
  int status = -1;

  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // If auth is needed again after this, something is screwy.
      LoginHandler* handler = iter->second;
      AddNavigationStatusListener(tab,
        new AutomationMsg_CancelAuthResponse(message.routing_id(), 0),
        new AutomationMsg_CancelAuthResponse(message.routing_id(), -1));
      handler->CancelAuth();
      status = 0;
    }
  }
  if (status < 0) {
    Send(new AutomationMsg_CancelAuthResponse(message.routing_id(), status));
  }
}

void AutomationProvider::NeedsAuth(const IPC::Message& message,
                                   int tab_handle) {
  bool needs_auth = false;

  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // The LoginHandler will be in our map IFF the tab needs auth.
      needs_auth = true;
    }
  }

  Send(new AutomationMsg_NeedsAuthResponse(message.routing_id(), needs_auth));
}

void AutomationProvider::GetRedirectsFrom(const IPC::Message& message,
                                          int tab_handle,
                                          const GURL& source_url) {
  DCHECK(!redirect_query_) << "Can only handle one redirect query at once.";
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    HistoryService* history_service =
        tab->profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);

    DCHECK(history_service) << "Tab " << tab_handle << "'s profile " <<
                               "has no history service";
    if (history_service) {
      // Schedule a history query for redirects. The response will be sent
      // asynchronously from the callback the history system uses to notify us
      // that it's done: OnRedirectQueryComplete.
      redirect_query_routing_id_ = message.routing_id();
      redirect_query_ = history_service->QueryRedirectsFrom(
          source_url, &consumer_,
          NewCallback(this, &AutomationProvider::OnRedirectQueryComplete));
      return;  // Response will be sent when query completes.
    }
  }

  // Send failure response.
  IPC::Message* msg = new IPC::Message(
    message.routing_id(), AutomationMsg_RedirectsFromResponse::ID,
    IPC::Message::PRIORITY_NORMAL);
  msg->WriteInt(-1);   // Negative string count indicates an error.
  Send(msg);
}

void AutomationProvider::GetActiveTabIndex(const IPC::Message& message,
                                           int handle) {
  int active_tab_index = -1;  // -1 is the error code
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    active_tab_index = browser->selected_index();
  }
  Send(new AutomationMsg_ActiveTabIndexResponse(message.routing_id(),
                                                active_tab_index));
}

void AutomationProvider::GetBrowserWindowCount(const IPC::Message& message) {
  Send(new AutomationMsg_BrowserWindowCountResponse(
      message.routing_id(), static_cast<int>(BrowserList::size())));
}

void AutomationProvider::GetShowingAppModalDialog(const IPC::Message& message) {
  views::AppModalDialogDelegate* dialog_delegate =
      BrowserList::GetShowingAppModalDialog();
  Send(new AutomationMsg_ShowingAppModalDialogResponse(
           message.routing_id(), dialog_delegate != NULL,
           dialog_delegate ? dialog_delegate->GetDialogButtons() :
                             views::DialogDelegate::DIALOGBUTTON_NONE));
}

void AutomationProvider::ClickAppModalDialogButton(const IPC::Message& message,
                                                   int button) {
  bool success = false;

  views::AppModalDialogDelegate* dialog_delegate =
      BrowserList::GetShowingAppModalDialog();
  if (dialog_delegate &&
      (dialog_delegate->GetDialogButtons() & button) == button) {
    views::DialogClientView* client_view =
        dialog_delegate->window()->client_view()->AsDialogClientView();
    if ((button & views::DialogDelegate::DIALOGBUTTON_OK) ==
        views::DialogDelegate::DIALOGBUTTON_OK) {
      client_view->AcceptWindow();
      success =  true;
    }
    if ((button & views::DialogDelegate::DIALOGBUTTON_CANCEL) ==
        views::DialogDelegate::DIALOGBUTTON_CANCEL) {
      DCHECK(!success) << "invalid param, OK and CANCEL specified";
      client_view->CancelWindow();
      success =  true;
    }
  }
  Send(new AutomationMsg_ClickAppModalDialogButtonResponse(
      message.routing_id(), success));
}

void AutomationProvider::GetBrowserWindow(const IPC::Message& message,
                                          int index) {
  int handle = 0;
  if (index >= 0) {
    BrowserList::const_iterator iter = BrowserList::begin();

  for (; (iter != BrowserList::end()) && (index > 0); ++iter, --index);
    if (iter != BrowserList::end()) {
      handle = browser_tracker_->Add(*iter);
    }
  }

  Send(new AutomationMsg_BrowserWindowResponse(message.routing_id(), handle));
}

void AutomationProvider::GetLastActiveBrowserWindow(
    const IPC::Message& message) {
  int handle = 0;
  Browser* browser = BrowserList::GetLastActive();
  if (browser)
    handle = browser_tracker_->Add(browser);
  Send(new AutomationMsg_LastActiveBrowserWindowResponse(message.routing_id(),
                                                         handle));
}

BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM l_param) {
  if (hwnd == reinterpret_cast<HWND>(l_param)) {
    return FALSE;
  }
  return TRUE;
}

void AutomationProvider::GetActiveWindow(const IPC::Message& message) {
  HWND window = GetForegroundWindow();

  // Let's make sure this window belongs to our process.
  if (EnumThreadWindows(::GetCurrentThreadId(),
                        EnumThreadWndProc,
                        reinterpret_cast<LPARAM>(window))) {
    // We enumerated all the windows and did not find the foreground window,
    // it is not our window, ignore it.
    Send(new AutomationMsg_ActiveWindowResponse(message.routing_id(), 0));
    return;
  }

  int handle = window_tracker_->Add(window);
  Send(new AutomationMsg_ActiveWindowResponse(message.routing_id(), handle));
}

void AutomationProvider::GetWindowHWND(const IPC::Message& message,
                                       int handle) {
  HWND win32_handle = window_tracker_->GetResource(handle);
  Send(new AutomationMsg_WindowHWNDResponse(message.routing_id(),
                                            win32_handle));
}

void AutomationProvider::ExecuteBrowserCommand(const IPC::Message& message,
                                               int handle,
                                               int command) {
  bool success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser->SupportsCommand(command) &&
        browser->IsCommandEnabled(command)) {
      browser->ExecuteCommand(command);
      success = true;
    }
  }
  Send(new AutomationMsg_WindowExecuteCommandResponse(message.routing_id(),
                                                      success));
}

void AutomationProvider::WindowGetViewBounds(const IPC::Message& message,
                                             int handle,
                                             int view_id,
                                             bool screen_coordinates) {
  bool succeeded = false;
  gfx::Rect bounds;

  void* iter = NULL;
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    views::RootView* root_view = views::WidgetWin::FindRootView(hwnd);
    if (root_view) {
      views::View* view = root_view->GetViewByID(view_id);
      if (view) {
        succeeded = true;
        gfx::Point point;
        if (screen_coordinates)
          views::View::ConvertPointToScreen(view, &point);
        else
          views::View::ConvertPointToView(view, root_view, &point);
        bounds = view->GetLocalBounds(false);
        bounds.set_origin(point);
      }
    }
  }

  Send(new AutomationMsg_WindowViewBoundsResponse(message.routing_id(),
                                                  succeeded, bounds));
}

// This task enqueues a mouse event on the event loop, so that the view
// that it's being sent to can do the requisite post-processing.
class MouseEventTask : public Task {
 public:
  MouseEventTask(views::View* view,
                 views::Event::EventType type,
                 POINT point,
                 int flags)
      : view_(view), type_(type), point_(point), flags_(flags) {}
  virtual ~MouseEventTask() {}

  virtual void Run() {
    views::MouseEvent event(type_, point_.x, point_.y, flags_);
    // We need to set the cursor position before we process the event because
    // some code (tab dragging, for instance) queries the actual cursor location
    // rather than the location of the mouse event. Note that the reason why
    // the drag code moved away from using mouse event locations was because
    // our conversion to screen location doesn't work well with multiple
    // monitors, so this only works reliably in a single monitor setup.
    gfx::Point screen_location(point_.x, point_.y);
    view_->ConvertPointToScreen(view_, &screen_location);
    ::SetCursorPos(screen_location.x(), screen_location.y());
    switch (type_) {
      case views::Event::ET_MOUSE_PRESSED:
        view_->OnMousePressed(event);
        break;

      case views::Event::ET_MOUSE_DRAGGED:
        view_->OnMouseDragged(event);
        break;

      case views::Event::ET_MOUSE_RELEASED:
        view_->OnMouseReleased(event, false);
        break;

      default:
        NOTREACHED();
    }
  }

 private:
  views::View* view_;
  views::Event::EventType type_;
  POINT point_;
  int flags_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventTask);
};

void AutomationProvider::ScheduleMouseEvent(views::View* view,
                                            views::Event::EventType type,
                                            POINT point,
                                            int flags) {
  MessageLoop::current()->PostTask(FROM_HERE,
      new MouseEventTask(view, type, point, flags));
}

// This task just adds another task to the event queue.  This is useful if
// you want to ensure that any tasks added to the event queue after this one
// have already been processed by the time |task| is run.
class InvokeTaskLaterTask : public Task {
 public:
  explicit InvokeTaskLaterTask(Task* task) : task_(task) {}
  virtual ~InvokeTaskLaterTask() {}

  virtual void Run() {
    MessageLoop::current()->PostTask(FROM_HERE, task_);
  }

 private:
  Task* task_;

  DISALLOW_COPY_AND_ASSIGN(InvokeTaskLaterTask);
};

// This task sends a WindowDragResponse message with the appropriate
// routing ID to the automation proxy.  This is implemented as a task so that
// we know that the mouse events (and any tasks that they spawn on the message
// loop) have been processed by the time this is sent.
class WindowDragResponseTask : public Task {
 public:
  WindowDragResponseTask(AutomationProvider* provider, int routing_id)
      : provider_(provider), routing_id_(routing_id) {}
  virtual ~WindowDragResponseTask() {}

  virtual void Run() {
    provider_->Send(new AutomationMsg_WindowDragResponse(routing_id_, true));
  }

 private:
  AutomationProvider* provider_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(WindowDragResponseTask);
};

void AutomationProvider::WindowSimulateClick(const IPC::Message& message,
                                             int handle,
                                             POINT click,
                                             int flags) {
  HWND hwnd = 0;

  if (window_tracker_->ContainsHandle(handle)) {
    hwnd = window_tracker_->GetResource(handle);

    ui_controls::SendMouseMove(click.x, click.y);

    ui_controls::MouseButton button = ui_controls::LEFT;
    if ((flags & views::Event::EF_LEFT_BUTTON_DOWN) ==
        views::Event::EF_LEFT_BUTTON_DOWN) {
      button = ui_controls::LEFT;
    } else if ((flags & views::Event::EF_RIGHT_BUTTON_DOWN) ==
        views::Event::EF_RIGHT_BUTTON_DOWN) {
      button = ui_controls::RIGHT;
    } else if ((flags & views::Event::EF_MIDDLE_BUTTON_DOWN) ==
        views::Event::EF_MIDDLE_BUTTON_DOWN) {
      button = ui_controls::MIDDLE;
    } else {
      NOTREACHED();
    }
    ui_controls::SendMouseClick(button);
  }
}

void AutomationProvider::WindowSimulateDrag(const IPC::Message& message,
                                            int handle,
                                            std::vector<POINT> drag_path,
                                            int flags,
                                            bool press_escape_en_route) {
  bool succeeded = false;
  if (browser_tracker_->ContainsHandle(handle) && (drag_path.size() > 1)) {
    succeeded = true;

    UINT down_message = 0;
    UINT up_message = 0;
    WPARAM wparam_flags = 0;
    if (flags & views::Event::EF_SHIFT_DOWN)
      wparam_flags |= MK_SHIFT;
    if (flags & views::Event::EF_CONTROL_DOWN)
      wparam_flags |= MK_CONTROL;
    if (flags & views::Event::EF_LEFT_BUTTON_DOWN) {
      wparam_flags |= MK_LBUTTON;
      down_message = WM_LBUTTONDOWN;
      up_message = WM_LBUTTONUP;
    }
    if (flags & views::Event::EF_MIDDLE_BUTTON_DOWN) {
      wparam_flags |= MK_MBUTTON;
      down_message = WM_MBUTTONDOWN;
      up_message = WM_MBUTTONUP;
    }
    if (flags & views::Event::EF_RIGHT_BUTTON_DOWN) {
      wparam_flags |= MK_RBUTTON;
      down_message = WM_LBUTTONDOWN;
      up_message = WM_LBUTTONUP;
    }

    Browser* browser = browser_tracker_->GetResource(handle);
    DCHECK(browser);
    HWND top_level_hwnd =
        reinterpret_cast<HWND>(browser->window()->GetNativeHandle());
    POINT temp = drag_path[0];
    MapWindowPoints(top_level_hwnd, HWND_DESKTOP, &temp, 1);
    SetCursorPos(temp.x, temp.y);
    SendMessage(top_level_hwnd, down_message, wparam_flags,
                MAKELPARAM(drag_path[0].x, drag_path[0].y));
    for (int i = 1; i < static_cast<int>(drag_path.size()); ++i) {
      temp = drag_path[i];
      MapWindowPoints(top_level_hwnd, HWND_DESKTOP, &temp, 1);
      SetCursorPos(temp.x, temp.y);
      SendMessage(top_level_hwnd, WM_MOUSEMOVE, wparam_flags,
                  MAKELPARAM(drag_path[i].x, drag_path[i].y));
    }
    POINT end = drag_path[drag_path.size() - 1];
    MapWindowPoints(top_level_hwnd, HWND_DESKTOP, &end, 1);
    SetCursorPos(end.x, end.y);

    if (press_escape_en_route) {
      // Press Escape.
      ui_controls::SendKeyPress(VK_ESCAPE,
                               ((flags & views::Event::EF_CONTROL_DOWN)
                                == views::Event::EF_CONTROL_DOWN),
                               ((flags & views::Event::EF_SHIFT_DOWN) ==
                                views::Event::EF_SHIFT_DOWN),
                               ((flags & views::Event::EF_ALT_DOWN) ==
                                views::Event::EF_ALT_DOWN));
    }
    SendMessage(top_level_hwnd, up_message, wparam_flags,
                MAKELPARAM(end.x, end.y));

    MessageLoop::current()->PostTask(FROM_HERE,
        new InvokeTaskLaterTask(
            new WindowDragResponseTask(this, message.routing_id())));
  } else {
    Send(new AutomationMsg_WindowDragResponse(message.routing_id(), true));
  }
}

void AutomationProvider::WindowSimulateKeyPress(const IPC::Message& message,
                                                int handle,
                                                wchar_t key,
                                                int flags) {
  if (!window_tracker_->ContainsHandle(handle))
    return;

  // The key event is sent to whatever window is active.
  ui_controls::SendKeyPress(key,
                           ((flags & views::Event::EF_CONTROL_DOWN) ==
                              views::Event::EF_CONTROL_DOWN),
                            ((flags & views::Event::EF_SHIFT_DOWN) ==
                              views::Event::EF_SHIFT_DOWN),
                            ((flags & views::Event::EF_ALT_DOWN) ==
                              views::Event::EF_ALT_DOWN));
}

void AutomationProvider::GetFocusedViewID(const IPC::Message& message,
                                          int handle) {
  int view_id = -1;
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManager(hwnd);
    DCHECK(focus_manager);
    views::View* focused_view = focus_manager->GetFocusedView();
    if (focused_view)
      view_id = focused_view->GetID();
  }
  Send(new AutomationMsg_GetFocusedViewIDResponse(message.routing_id(),
                                                  view_id));
}

void AutomationProvider::SetWindowVisible(const IPC::Message& message,
                                          int handle, bool visible) {
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    ::ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
    Send(new AutomationMsg_SetWindowVisibleResponse(message.routing_id(),
                                                    true));
  } else {
    Send(new AutomationMsg_SetWindowVisibleResponse(message.routing_id(),
                                                    false));
  }
}

void AutomationProvider::IsWindowActive(const IPC::Message& message,
                                        int handle) {
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    bool is_active = ::GetForegroundWindow() == hwnd;
    Send(new AutomationMsg_IsWindowActiveResponse(
        message.routing_id(), true, is_active));
  } else {
    Send(new AutomationMsg_IsWindowActiveResponse(message.routing_id(),
                                                  false, false));
  }
}

void AutomationProvider::ActivateWindow(const IPC::Message& message,
                                        int handle) {
  if (window_tracker_->ContainsHandle(handle)) {
    ::SetActiveWindow(window_tracker_->GetResource(handle));
  }
}

void AutomationProvider::GetTabCount(const IPC::Message& message, int handle) {
  int tab_count = -1;  // -1 is the error code

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    tab_count = browser->tab_count();
  }

  Send(new AutomationMsg_TabCountResponse(message.routing_id(), tab_count));
}

void AutomationProvider::GetTab(const IPC::Message& message,
                                int win_handle, int tab_index) {
  void* iter = NULL;
  int tab_handle = 0;
  if (browser_tracker_->ContainsHandle(win_handle) && (tab_index >= 0)) {
    Browser* browser = browser_tracker_->GetResource(win_handle);
    if (tab_index < browser->tab_count()) {
      TabContents* tab_contents =
          browser->GetTabContentsAt(tab_index);
      tab_handle = tab_tracker_->Add(tab_contents->controller());
    }
  }

  Send(new AutomationMsg_TabResponse(message.routing_id(), tab_handle));
}

void AutomationProvider::GetTabTitle(const IPC::Message& message, int handle) {
  int title_string_size = -1;  // -1 is the error code
  std::wstring title;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    title = tab->GetActiveEntry()->title();
    title_string_size = static_cast<int>(title.size());
  }

  Send(new AutomationMsg_TabTitleResponse(message.routing_id(),
                                          title_string_size, title));
}

void AutomationProvider::HandleUnused(const IPC::Message& message, int handle) {
  if (window_tracker_->ContainsHandle(handle)) {
    window_tracker_->Remove(window_tracker_->GetResource(handle));
  }
}

void AutomationProvider::OnChannelError() {
  LOG(ERROR) << "AutomationProxy went away, shutting down app.";
  AutomationProviderList::GetInstance()->RemoveProvider(this);
}

// TODO(brettw) change this to accept GURLs when history supports it
void AutomationProvider::OnRedirectQueryComplete(
    HistoryService::Handle request_handle,
    GURL from_url,
    bool success,
    HistoryService::RedirectList* redirects) {
  DCHECK(request_handle == redirect_query_);

  // Respond to the pending request for the redirect list.
  IPC::Message* msg = new IPC::Message(redirect_query_routing_id_,
                                       AutomationMsg_RedirectsFromResponse::ID,
                                       IPC::Message::PRIORITY_NORMAL);
  if (success) {
    msg->WriteInt(static_cast<int>(redirects->size()));
    for (size_t i = 0; i < redirects->size(); i++)
      IPC::ParamTraits<GURL>::Write(msg, redirects->at(i));
  } else {
    msg->WriteInt(-1);  // Negative count indicates failure.
  }

  Send(msg);
  redirect_query_ = NULL;
}

bool AutomationProvider::Send(IPC::Message* msg) {
  DCHECK(channel_.get());
  return channel_->Send(msg);
}

Browser* AutomationProvider::FindAndActivateTab(
    NavigationController* controller) {
  int tab_index;
  Browser* browser = Browser::GetBrowserForController(controller, &tab_index);
  if (browser)
    browser->SelectTabContentsAt(tab_index, true);

  return browser;
}

void AutomationProvider::GetCookies(const IPC::Message& message,
                                    const GURL& url, int handle) {
  std::string value;

  if (url.is_valid() && tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    value =
        tab->profile()->GetRequestContext()->cookie_store()->GetCookies(url);
  }

  Send(new AutomationMsg_GetCookiesResponse(message.routing_id(),
    static_cast<int>(value.size()), value));
}

void AutomationProvider::SetCookie(const IPC::Message& message,
                                   const GURL& url,
                                   const std::string value,
                                   int handle) {
  int response_value = -1;

  if (url.is_valid() && tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    URLRequestContext* context = tab->profile()->GetRequestContext();
    if (context->cookie_store()->SetCookie(url, value))
      response_value = 1;
  }

  Send(new AutomationMsg_SetCookieResponse(message.routing_id(),
    response_value));
}

void AutomationProvider::GetTabURL(const IPC::Message& message, int handle) {
  bool success = false;
  GURL url;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    // Return what the user would see in the location bar.
    url = tab->GetActiveEntry()->display_url();
    success = true;
  }

  Send(new AutomationMsg_TabURLResponse(message.routing_id(), success, url));
}

void AutomationProvider::GetTabHWND(const IPC::Message& message, int handle) {
  HWND tab_hwnd = NULL;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    tab_hwnd = tab->active_contents()->GetContainerHWND();
  }

  Send(new AutomationMsg_TabHWNDResponse(message.routing_id(), tab_hwnd));
}

void AutomationProvider::GetTabProcessID(
    const IPC::Message& message, int handle) {
  int process_id = -1;

  if (tab_tracker_->ContainsHandle(handle)) {
    process_id = 0;
    NavigationController* tab = tab_tracker_->GetResource(handle);
    if (tab->active_contents()->AsWebContents()) {
      WebContents* web_contents = tab->active_contents()->AsWebContents();
      if (web_contents->process())
        process_id = web_contents->process()->process().pid();
    }
  }

  Send(new AutomationMsg_TabProcessIDResponse(message.routing_id(),
                                              process_id));
}

void AutomationProvider::ApplyAccelerator(int handle, int id) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->controller()->ExecuteCommand(id);
  }
}

void AutomationProvider::ExecuteJavascript(const IPC::Message& message,
                                           int handle,
                                           const std::wstring& frame_xpath,
                                           const std::wstring& script) {
  bool succeeded = false;
  WebContents* web_contents = GetWebContentsForHandle(handle, NULL);
  if (web_contents) {
    // Set the routing id of this message with the controller.
    // This routing id needs to be remembered for the reverse
    // communication while sending back the response of
    // this javascript execution.
    std::wstring set_automation_id;
    SStringPrintf(&set_automation_id,
      L"window.domAutomationController.setAutomationId(%d);",
      message.routing_id());

    web_contents->render_view_host()->ExecuteJavascriptInWebFrame(
        frame_xpath, set_automation_id);
    web_contents->render_view_host()->ExecuteJavascriptInWebFrame(
        frame_xpath, script);
    succeeded = true;
  }

  if (!succeeded) {
    Send(new AutomationMsg_DomOperationResponse(message.routing_id(), ""));
  }
}

void AutomationProvider::GetShelfVisibility(const IPC::Message& message,
                                            int handle) {
  bool visible = false;

  WebContents* web_contents = GetWebContentsForHandle(handle, NULL);
  if (web_contents)
    visible = web_contents->IsDownloadShelfVisible();

  Send(new AutomationMsg_ShelfVisibilityResponse(message.routing_id(),
                                                 visible));
}

void AutomationProvider::GetConstrainedWindowCount(const IPC::Message& message,
                                                   int handle) {
  int count = -1;  // -1 is the error code
  if (tab_tracker_->ContainsHandle(handle)) {
      NavigationController* nav_controller = tab_tracker_->GetResource(handle);
      TabContents* tab_contents = nav_controller->active_contents();
      if (tab_contents) {
        count = static_cast<int>(tab_contents->child_windows_.size());
      }
  }

  Send(new AutomationMsg_ConstrainedWindowCountResponse(message.routing_id(),
                                                        count));
}

void AutomationProvider::GetConstrainedWindow(const IPC::Message& message,
                                              int handle, int index) {
  int cwindow_handle = 0;
  if (tab_tracker_->ContainsHandle(handle) && index >= 0) {
    NavigationController* nav_controller =
        tab_tracker_->GetResource(handle);
    TabContents* tab = nav_controller->active_contents();
    if (tab && index < static_cast<int>(tab->child_windows_.size())) {
      ConstrainedWindow* window = tab->child_windows_[index];
      cwindow_handle = cwindow_tracker_->Add(window);
    }
  }

  Send(new AutomationMsg_ConstrainedWindowResponse(message.routing_id(),
                                                   cwindow_handle));
}

void AutomationProvider::GetConstrainedTitle(const IPC::Message& message,
                                             int handle) {
  int title_string_size = -1;  // -1 is the error code
  std::wstring title;
  if (cwindow_tracker_->ContainsHandle(handle)) {
    ConstrainedWindow* window = cwindow_tracker_->GetResource(handle);
    title = window->GetWindowTitle();
    title_string_size = static_cast<int>(title.size());
  }

  Send(new AutomationMsg_ConstrainedTitleResponse(message.routing_id(),
                                                  title_string_size, title));
}

void AutomationProvider::GetConstrainedWindowBounds(const IPC::Message& message,
                                                    int handle) {
  bool exists = false;
  gfx::Rect rect(0, 0, 0, 0);
  if (cwindow_tracker_->ContainsHandle(handle)) {
    ConstrainedWindow* window = cwindow_tracker_->GetResource(handle);
    if (window) {
      exists = true;
      rect = window->GetCurrentBounds();
    }
  }

  Send(new AutomationMsg_ConstrainedWindowBoundsResponse(message.routing_id(),
                                                         exists, rect));
}

void AutomationProvider::HandleFindInPageRequest(
    const IPC::Message& message, int handle, const std::wstring& find_request,
    int forward, int match_case) {
  NOTREACHED() << "This function has been deprecated."
    << "Please use HandleFindRequest instead.";
  Send(new AutomationMsg_FindInPageResponse2(message.routing_id(), -1, -1));
  return;
}

void AutomationProvider::HandleFindRequest(const IPC::Message& message,
    int handle, const FindInPageRequest& request) {
  if (!tab_tracker_->ContainsHandle(handle)) {
    Send(new AutomationMsg_FindInPageResponse2(message.routing_id(), -1, -1));
    return;
  }

  NavigationController* nav = tab_tracker_->GetResource(handle);
  TabContents* tab_contents = nav->active_contents();

  find_in_page_observer_.reset(new
      FindInPageNotificationObserver(this, tab_contents, message.routing_id()));

  // The find in page dialog must be up for us to get the notification that the
  // find was complete.
  WebContents* web_contents = tab_contents->AsWebContents();
  if (web_contents) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = Browser::GetBrowserForController(tab, NULL);
    web_contents->view()->FindInPage(*browser, true, request.forward);

    web_contents->render_view_host()->StartFinding(
        FindInPageNotificationObserver::kFindInPageRequestId,
        request.search_string, request.forward, request.match_case,
        request.find_next);
  }
}

void AutomationProvider::HandleOpenFindInPageRequest(
    const IPC::Message& message, int handle) {
  NavigationController* tab = NULL;
  WebContents* web_contents = GetWebContentsForHandle(handle, &tab);
  if (web_contents) {
    Browser* browser = Browser::GetBrowserForController(tab, NULL);
    web_contents->view()->FindInPage(*browser, false, false);
  }
}

void AutomationProvider::GetFindWindowVisibility(const IPC::Message& message,
                                                 int handle) {
  gfx::Point position;
  bool visible = false;
  WebContents* web_contents = GetWebContentsForHandle(handle, NULL);
  if (web_contents)
    web_contents->view()->GetFindBarWindowInfo(&position, &visible);

  Send(new AutomationMsg_FindWindowVisibilityResponse(message.routing_id(),
                                                      visible));
}

void AutomationProvider::HandleFindWindowLocationRequest(
    const IPC::Message& message, int handle) {
  gfx::Point position(0, 0);
  bool visible = false;
  WebContents* web_contents = GetWebContentsForHandle(handle, NULL);
  if (web_contents)
    web_contents->view()->GetFindBarWindowInfo(&position, &visible);

  Send(new AutomationMsg_FindWindowLocationResponse(message.routing_id(),
                                                    position.x(),
                                                    position.y()));
}

void AutomationProvider::GetBookmarkBarVisitility(const IPC::Message& message,
                                                  int handle) {
  bool visible = false;
  bool animating = false;

  void* iter = NULL;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkBarView* bookmark_bar = browser->window()->GetBookmarkBarView();
      if (bookmark_bar) {
        animating = bookmark_bar->IsAnimating();
        visible = browser->window()->IsBookmarkBarVisible();
      }
    }
  }

  Send(new AutomationMsg_BookmarkBarVisibilityResponse(message.routing_id(),
                                                       visible, animating));
}

void AutomationProvider::HandleInspectElementRequest(
    const IPC::Message& message, int handle, int x, int y) {
  WebContents* web_contents = GetWebContentsForHandle(handle, NULL);
  if (web_contents) {
    web_contents->render_view_host()->InspectElementAt(x, y);
    inspect_element_routing_id_ = message.routing_id();
  } else {
    Send(new AutomationMsg_InspectElementResponse(message.routing_id(), -1));
  }
}

void AutomationProvider::ReceivedInspectElementResponse(int num_resources) {
  Send(new AutomationMsg_InspectElementResponse(inspect_element_routing_id_,
                                                num_resources));
}

// Helper class for making changes to the URLRequest ProtocolFactory on the
// IO thread.
class SetFilteredInetTask : public Task {
 public:
  explicit SetFilteredInetTask(bool enabled) : enabled_(enabled) { }
  virtual void Run() {
    if (enabled_) {
      URLRequestFilter::GetInstance()->ClearHandlers();

      URLRequestFailedDnsJob::AddUITestUrls();
      URLRequestSlowDownloadJob::AddUITestUrls();

      std::wstring root_http;
      PathService::Get(chrome::DIR_TEST_DATA, &root_http);
      URLRequestMockHTTPJob::AddUITestUrls(root_http);
    } else {
      // Revert to the default handlers.
      URLRequestFilter::GetInstance()->ClearHandlers();
    }
  }
 private:
  bool enabled_;
};

void AutomationProvider::SetFilteredInet(const IPC::Message& message,
                                         bool enabled) {
  // Since this involves changing the URLRequest ProtocolFactory, we want to
  // run on the main thread.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      new SetFilteredInetTask(enabled));
}

void AutomationProvider::GetDownloadDirectory(const IPC::Message& message,
                                              int handle) {
  DLOG(INFO) << "Handling download directory request";
  std::wstring download_directory;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    DownloadManager* dlm = tab->profile()->GetDownloadManager();
    DCHECK(dlm);
    download_directory = dlm->download_path();
  }

  Send(new AutomationMsg_DownloadDirectoryResponse(message.routing_id(),
                                                   download_directory));
}

void AutomationProvider::OpenNewBrowserWindow(int show_command) {
  // We may have no current browser windows open so don't rely on
  // asking an existing browser to execute the IDC_NEWWINDOW command
  Browser* browser = Browser::Create(profile_);
  browser->AddBlankTab(true);
  if (show_command != SW_HIDE)
    browser->window()->Show();
}

void AutomationProvider::GetWindowForBrowser(const IPC::Message& message,
                                             int browser_handle) {
  bool success = false;
  int window_handle = 0;

  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    HWND hwnd = reinterpret_cast<HWND>(browser->window()->GetNativeHandle());
    // Add() returns the existing handle for the resource if any.
    window_handle = window_tracker_->Add(hwnd);
    success = true;
  }
  Send(new AutomationMsg_WindowForBrowserResponse(message.routing_id(),
                                                  success, window_handle));
}

void AutomationProvider::GetAutocompleteEditForBrowser(
    const IPC::Message& message,
    int browser_handle) {
  bool success = false;
  int autocomplete_edit_handle = 0;

  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    LocationBarView* loc_bar_view = browser->GetLocationBarView();
    AutocompleteEditView* edit_view = loc_bar_view->location_entry();
    // Add() returns the existing handle for the resource if any.
    autocomplete_edit_handle = autocomplete_edit_tracker_->Add(edit_view);
    success = true;
  }
  Send(new AutomationMsg_AutocompleteEditForBrowserResponse(
    message.routing_id(), success, autocomplete_edit_handle));
}

void AutomationProvider::GetBrowserForWindow(const IPC::Message& message,
                                             int window_handle) {
  bool success = false;
  int browser_handle = 0;

  if (window_tracker_->ContainsHandle(window_handle)) {
    HWND window = window_tracker_->GetResource(window_handle);
    BrowserList::const_iterator iter = BrowserList::begin();
    Browser* browser = NULL;
    for (;iter != BrowserList::end(); ++iter) {
      HWND hwnd = reinterpret_cast<HWND>((*iter)->window()->GetNativeHandle());
      if (window == hwnd) {
        browser = *iter;
        break;
      }
    }
    if (browser) {
      // Add() returns the existing handle for the resource if any.
      browser_handle = browser_tracker_->Add(browser);
      success = true;
    }
  }
  Send(new AutomationMsg_BrowserForWindowResponse(message.routing_id(),
                                                  success, browser_handle));
}

void AutomationProvider::ShowInterstitialPage(const IPC::Message& message,
                                              int tab_handle,
                                              const std::string& html_text) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* controller = tab_tracker_->GetResource(tab_handle);
    TabContents* tab_contents = controller->active_contents();
    if (tab_contents->type() == TAB_CONTENTS_WEB) {
      AddNavigationStatusListener(controller,
          new AutomationMsg_ShowInterstitialPageResponse(message.routing_id(),
                                                         true),
          NULL);
      WebContents* web_contents = tab_contents->AsWebContents();
      AutomationInterstitialPage* interstitial =
          new AutomationInterstitialPage(web_contents,
                                         GURL("about:interstitial"),
                                         html_text);
      interstitial->Show();
      return;
    }
  }
  Send(new AutomationMsg_ShowInterstitialPageResponse(message.routing_id(),
                                                      false));
}

void AutomationProvider::HideInterstitialPage(const IPC::Message& message,
                                              int tab_handle) {
  WebContents* web_contents = GetWebContentsForHandle(tab_handle, NULL);
  if (web_contents && web_contents->interstitial_page()) {
    web_contents->interstitial_page()->DontProceed();
    Send(new AutomationMsg_HideInterstitialPageResponse(message.routing_id(),
                                                        true));
    return;
  }
  Send(new AutomationMsg_HideInterstitialPageResponse(message.routing_id(),
                                                      false));
}

void AutomationProvider::CloseTab(const IPC::Message& message,
                                  int tab_handle,
                                  bool wait_until_closed) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* controller = tab_tracker_->GetResource(tab_handle);
    int index;
    Browser* browser = Browser::GetBrowserForController(controller, &index);
    DCHECK(browser);
    TabClosedNotificationObserver* observer =
        new TabClosedNotificationObserver(browser, this, message.routing_id(),
                                          wait_until_closed);
    browser->CloseContents(controller->active_contents());
  } else {
    Send(new AutomationMsg_CloseTabResponse(message.routing_id(), false));
  }
}

void AutomationProvider::CloseBrowser(const IPC::Message& message,
                                      int browser_handle) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    new BrowserClosedNotificationObserver(browser, this, message.routing_id());
    browser->window()->Close();
  } else {
    NOTREACHED();
  }
}

void AutomationProvider::CreateExternalTab(const IPC::Message& message) {
  int tab_handle = 0;
  HWND tab_container_window = NULL;
  ExternalTabContainer *external_tab_container =
      new ExternalTabContainer(this);
  external_tab_container->Init(profile_);
  TabContents* tab_contents = external_tab_container->tab_contents();
  if (tab_contents) {
    tab_handle = tab_tracker_->Add(tab_contents->controller());
    tab_container_window = *external_tab_container;
  }
  Send(new AutomationMsg_CreateExternalTabResponse(message.routing_id(),
                                                   tab_container_window,
                                                   tab_handle));
}

void AutomationProvider::NavigateInExternalTab(const IPC::Message& message,
                                               int handle, const GURL& url) {
  bool status = false;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    tab->LoadURL(url, GURL(), PageTransition::TYPED);
    status = true;
  }

  Send(new AutomationMsg_NavigateInExternalTabResponse(message.routing_id(),
                                                       status));
}

void AutomationProvider::SetAcceleratorsForTab(const IPC::Message& message,
                                               int handle,
                                               HACCEL accel_table,
                                               int accel_entry_count) {
  bool status = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    TabContents* tab_contents = tab->GetTabContents(TAB_CONTENTS_WEB);
    ExternalTabContainer* external_tab_container =
        ExternalTabContainer::GetContainerForTab(
            tab_contents->GetContainerHWND());
    // This call is only valid on an externally hosted tab
    if (external_tab_container) {
      external_tab_container->SetAccelerators(accel_table,
                                              accel_entry_count);
      status = true;
    }
  }
  Send(new AutomationMsg_SetAcceleratorsForTabResponse(message.routing_id(),
                                                       status));
}

void AutomationProvider::ProcessUnhandledAccelerator(
    const IPC::Message& message, int handle, const MSG& msg) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    TabContents* tab_contents = tab->GetTabContents(TAB_CONTENTS_WEB);
    ExternalTabContainer* external_tab_container =
        ExternalTabContainer::GetContainerForTab(
            tab_contents->GetContainerHWND());
    // This call is only valid on an externally hosted tab
    if (external_tab_container) {
      external_tab_container->ProcessUnhandledAccelerator(msg);
    }
  }
  // This message expects no response.
}

void AutomationProvider::WaitForTabToBeRestored(
    const IPC::Message& message,
    int tab_handle) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    restore_tracker_.reset(
        new NavigationControllerRestoredObserver(this, tab,
                                                 message.routing_id()));
  }
}

void AutomationProvider::GetSecurityState(const IPC::Message& message,
                                          int handle) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    Send(new AutomationMsg_GetSecurityStateResponse(message.routing_id(), true,
        entry->ssl().security_style(), entry->ssl().cert_status(),
        entry->ssl().content_status()));
  } else {
    Send(new AutomationMsg_GetSecurityStateResponse(message.routing_id(), false,
                                                    SECURITY_STYLE_UNKNOWN,
                                                    0, 0));
  }
}

void AutomationProvider::GetPageType(const IPC::Message& message, int handle) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    NavigationEntry::PageType page_type = entry->page_type();
    // In order to return the proper result when an interstitial is shown and
    // no navigation entry were created for it we need to ask the WebContents.
    if (page_type == NavigationEntry::NORMAL_PAGE &&
        tab->active_contents()->AsWebContents() &&
        tab->active_contents()->AsWebContents()->showing_interstitial_page())
      page_type = NavigationEntry::INTERSTITIAL_PAGE;

    Send(new AutomationMsg_GetPageTypeResponse(message.routing_id(), true,
                                               page_type));
  } else {
    Send(new AutomationMsg_GetPageTypeResponse(message.routing_id(), false,
                                               NavigationEntry::NORMAL_PAGE));
  }
}

void AutomationProvider::ActionOnSSLBlockingPage(const IPC::Message& message,
                                                 int handle, bool proceed) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    if (entry->page_type() == NavigationEntry::INTERSTITIAL_PAGE) {
      TabContents* tab_contents = tab->GetTabContents(TAB_CONTENTS_WEB);
      InterstitialPage* ssl_blocking_page =
          InterstitialPage::GetInterstitialPage(tab_contents->AsWebContents());
      if (ssl_blocking_page) {
        if (proceed) {
          AddNavigationStatusListener(tab,
              new AutomationMsg_ActionOnSSLBlockingPageResponse(
                  message.routing_id(), true),
              new AutomationMsg_ActionOnSSLBlockingPageResponse(
                  message.routing_id(), true));
            ssl_blocking_page->Proceed();
          return;
        }
        ssl_blocking_page->DontProceed();
        Send(new AutomationMsg_ActionOnSSLBlockingPageResponse(
             message.routing_id(), true));
        return;
      }
    }
  }
  // We failed.
  Send(new AutomationMsg_ActionOnSSLBlockingPageResponse(message.routing_id(),
                                                         false));
}

void AutomationProvider::BringBrowserToFront(const IPC::Message& message,
                                             int browser_handle) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    browser->window()->Activate();
    Send(new AutomationMsg_BringBrowserToFrontResponse(message.routing_id(),
                                                       true));
  } else {
    Send(new AutomationMsg_BringBrowserToFrontResponse(message.routing_id(),
                                                       false));
  }
}

void AutomationProvider::IsPageMenuCommandEnabled(const IPC::Message& message,
                                                  int browser_handle,
                                                  int message_num) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    bool menu_item_enabled =
        browser->controller()->IsCommandEnabled(message_num);
    Send(new AutomationMsg_IsPageMenuCommandEnabledResponse(
             message.routing_id(), menu_item_enabled));
  } else {
    Send(new AutomationMsg_IsPageMenuCommandEnabledResponse(
             message.routing_id(), false));
  }
}

void AutomationProvider::PrintNow(const IPC::Message& message, int tab_handle) {
  NavigationController* tab = NULL;
  WebContents* web_contents = GetWebContentsForHandle(tab_handle, &tab);
  if (web_contents) {
    FindAndActivateTab(tab);
    notification_observer_list_.AddObserver(
        new DocumentPrintedNotificationObserver(this, message.routing_id()));
    if (web_contents->PrintNow())
      return;
  }
  Send(new AutomationMsg_PrintNowResponse(message.routing_id(), false));
}

void AutomationProvider::SavePage(const IPC::Message& message,
                                  int tab_handle,
                                  const std::wstring& file_name,
                                  const std::wstring& dir_path,
                                  int type) {
  if (!tab_tracker_->ContainsHandle(tab_handle)) {
    Send(new AutomationMsg_SavePageResponse(message.routing_id(), false));
    return;
  }

  NavigationController* nav = tab_tracker_->GetResource(tab_handle);
  Browser* browser = FindAndActivateTab(nav);
  DCHECK(browser);
  if (!browser->IsCommandEnabled(IDC_SAVE_PAGE)) {
    Send(new AutomationMsg_SavePageResponse(message.routing_id(), false));
    return;
  }

  TabContents* tab_contents = nav->active_contents();
  if (tab_contents->type() != TAB_CONTENTS_WEB) {
    Send(new AutomationMsg_SavePageResponse(message.routing_id(), false));
    return;
  }

  SavePackage::SavePackageType save_type =
      static_cast<SavePackage::SavePackageType>(type);
  DCHECK(save_type >= SavePackage::SAVE_AS_ONLY_HTML &&
         save_type <= SavePackage::SAVE_AS_COMPLETE_HTML);
  tab_contents->AsWebContents()->SavePage(file_name, dir_path, save_type);

  Send(new AutomationMsg_SavePageResponse(
       message.routing_id(), true));
}

void AutomationProvider::GetAutocompleteEditText(const IPC::Message& message,
                                                int autocomplete_edit_handle) {
  bool success = false;
  std::wstring text;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    text = autocomplete_edit_tracker_->GetResource(autocomplete_edit_handle)->
        GetText();
    success = true;
  }
  Send(new AutomationMsg_AutocompleteEditGetTextResponse(message.routing_id(),
      success, text));
}

void AutomationProvider::SetAutocompleteEditText(const IPC::Message& message,
                                                 int autocomplete_edit_handle,
                                                 const std::wstring& text) {
  bool success = false;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    autocomplete_edit_tracker_->GetResource(autocomplete_edit_handle)->
        SetUserText(text);
    success = true;
  }
  Send(new AutomationMsg_AutocompleteEditSetTextResponse(
      message.routing_id(), success));
}

void AutomationProvider::AutocompleteEditGetMatches(
    const IPC::Message& message,
    int autocomplete_edit_handle) {
  bool success = false;
  std::vector<AutocompleteMatchData> matches;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    const AutocompleteResult& result = autocomplete_edit_tracker_->
        GetResource(autocomplete_edit_handle)->model()->result();
    for (AutocompleteResult::const_iterator i = result.begin();
        i != result.end(); ++i)
      matches.push_back(AutocompleteMatchData(*i));
    success = true;
  }
  Send(new AutomationMsg_AutocompleteEditGetMatchesResponse(
      message.routing_id(), success, matches));
}

void AutomationProvider::AutocompleteEditIsQueryInProgress(
    const IPC::Message& message,
    int autocomplete_edit_handle) {
  bool success = false;
  bool query_in_progress = false;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    query_in_progress = autocomplete_edit_tracker_->
        GetResource(autocomplete_edit_handle)->model()->query_in_progress();
    success = true;
  }
  Send(new AutomationMsg_AutocompleteEditIsQueryInProgressResponse(
      message.routing_id(), success, query_in_progress));
}

void AutomationProvider::OnMessageFromExternalHost(
    int handle, const std::string& target, const std::string& message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    if (!tab) {
      NOTREACHED();
      return;
    }
    TabContents* tab_contents = tab->GetTabContents(TAB_CONTENTS_WEB);
    if (!tab_contents) {
      NOTREACHED();
      return;
    }

    WebContents* web_contents = tab_contents->AsWebContents();
    if (!web_contents) {
      NOTREACHED();
      return;
    }

    RenderViewHost* view_host = web_contents->render_view_host();
    if (!view_host) {
      return;
    }

    view_host->ForwardMessageFromExternalHost(target, message);
  }
}

WebContents* AutomationProvider::GetWebContentsForHandle(
    int handle, NavigationController** tab) {
  WebContents* web_contents = NULL;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    TabContents* tab_contents = nav_controller->active_contents();
    if (tab_contents && tab_contents->type() == TAB_CONTENTS_WEB) {
      web_contents = tab_contents->AsWebContents();
      if (tab)
        *tab = nav_controller;
    }
  }
  return web_contents;
}

TestingAutomationProvider::TestingAutomationProvider(Profile* profile)
    : AutomationProvider(profile) {
  BrowserList::AddObserver(this);
  NotificationService::current()->AddObserver(this, NOTIFY_SESSION_END,
      NotificationService::AllSources());
}

TestingAutomationProvider::~TestingAutomationProvider() {
  NotificationService::current()->RemoveObserver(this, NOTIFY_SESSION_END,
      NotificationService::AllSources());
  BrowserList::RemoveObserver(this);
}

void TestingAutomationProvider::OnChannelError() {
  BrowserList::CloseAllBrowsers(true);
  AutomationProvider::OnChannelError();
}

void TestingAutomationProvider::OnBrowserRemoving(const Browser* browser) {
  // For backwards compatibility with the testing automation interface, we
  // want the automation provider (and hence the process) to go away when the
  // last browser goes away.
  if (BrowserList::size() == 1) {
    // If you change this, update Observer for NOTIFY_SESSION_END below.
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &TestingAutomationProvider::OnRemoveProvider));
  }
}

void TestingAutomationProvider::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(type == NOTIFY_SESSION_END);
  // OnBrowserRemoving does a ReleaseLater. When session end is received we exit
  // before the task runs resulting in this object not being deleted. This
  // Release balance out the Release scheduled by OnBrowserRemoving.
  Release();
}

void TestingAutomationProvider::OnRemoveProvider() {
  AutomationProviderList::GetInstance()->RemoveProvider(this);
}

void AutomationProvider::GetSSLInfoBarCount(const IPC::Message& message,
                                            int handle) {
  int count = -1;  // -1 means error.
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (nav_controller)
      count = nav_controller->active_contents()->infobar_delegate_count();
  }
  Send(new AutomationMsg_GetSSLInfoBarCountResponse(message.routing_id(),
                                                    count));
}

void AutomationProvider::ClickSSLInfoBarLink(const IPC::Message& message,
                                             int handle,
                                             int info_bar_index,
                                             bool wait_for_navigation) {
  bool success = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (nav_controller) {
      int count = nav_controller->active_contents()->infobar_delegate_count();
      if (info_bar_index >= 0 && info_bar_index < count) {
        if (wait_for_navigation) {
          AddNavigationStatusListener(nav_controller,
              new AutomationMsg_ClickSSLInfoBarLinkResponse(
                  message.routing_id(), true),
              new AutomationMsg_ClickSSLInfoBarLinkResponse(
                  message.routing_id(), true));
        }
        InfoBarDelegate* delegate =
            nav_controller->active_contents()->GetInfoBarDelegateAt(
                info_bar_index);
        if (delegate->AsConfirmInfoBarDelegate())
          delegate->AsConfirmInfoBarDelegate()->Accept();
        success = true;
      }
    }
   }
  if (!wait_for_navigation || !success)
    Send(new AutomationMsg_ClickSSLInfoBarLinkResponse(message.routing_id(),
                                                       success));
}

void AutomationProvider::GetLastNavigationTime(const IPC::Message& message,
                                               int handle) {
  Time time = tab_tracker_->GetLastNavigationTime(handle);
  Send(new AutomationMsg_GetLastNavigationTimeResponse(message.routing_id(),
                                                       time.ToInternalValue()));
}

void AutomationProvider::WaitForNavigation(const IPC::Message& message,
                                           int handle,
                                           int64 last_navigation_time) {
  NavigationController* controller = NULL;
  if (tab_tracker_->ContainsHandle(handle))
    controller = tab_tracker_->GetResource(handle);

  Time time = tab_tracker_->GetLastNavigationTime(handle);
  if (time.ToInternalValue() > last_navigation_time || !controller) {
    Send(new AutomationMsg_WaitForNavigationResponse(message.routing_id(),
                                                     controller != NULL));
    return;                                                       
  }

  AddNavigationStatusListener(controller,
      new AutomationMsg_WaitForNavigationResponse(message.routing_id(),
                                                  true),
      new AutomationMsg_WaitForNavigationResponse(message.routing_id(),
                                                  true));
}

void AutomationProvider::SetIntPreference(const IPC::Message& message,
                                          int handle,
                                          const std::wstring& name,
                                          int value) {
  bool success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetInteger(name.c_str(), value);
    success = true;
  }
  Send(new AutomationMsg_SetIntPreferenceResponse(message.routing_id(),
                                                  success));
}

void AutomationProvider::SetStringPreference(const IPC::Message& message,
                                             int handle,
                                             const std::wstring& name,
                                             const std::wstring& value) {
  bool success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetString(name.c_str(), value);
    success = true;
  }
  Send(new AutomationMsg_SetStringPreferenceResponse(message.routing_id(),
                                                     success));
}

void AutomationProvider::GetBooleanPreference(const IPC::Message& message,
                                              int handle,
                                              const std::wstring& name) {
  bool success = false;
  bool value = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    value = browser->profile()->GetPrefs()->GetBoolean(name.c_str());
    success = true;
  }
  Send(new AutomationMsg_GetBooleanPreferenceResponse(message.routing_id(),
                                                      success, value));
}

void AutomationProvider::SetBooleanPreference(const IPC::Message& message,
                                              int handle,
                                              const std::wstring& name,
                                              bool value) {
  bool success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetBoolean(name.c_str(), value);
    success = true;
  }
  Send(new AutomationMsg_SetBooleanPreferenceResponse(message.routing_id(),
                                                      success));
}

// Gets the current used encoding name of the page in the specified tab.
void AutomationProvider::GetPageCurrentEncoding(const IPC::Message& message,
                                                int tab_handle) {
  std::wstring current_encoding;
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* nav = tab_tracker_->GetResource(tab_handle);
    Browser* browser = FindAndActivateTab(nav);
    DCHECK(browser);

    if (browser->IsCommandEnabled(IDC_ENCODING_MENU)) {
      TabContents* tab_contents = nav->active_contents();
      DCHECK(tab_contents->type() == TAB_CONTENTS_WEB);
      current_encoding = tab_contents->AsWebContents()->encoding();
    }
  }
  Send(new AutomationMsg_GetPageCurrentEncodingResponse(message.routing_id(),
                                                        current_encoding));
}

// Gets the current used encoding name of the page in the specified tab.
void AutomationProvider::OverrideEncoding(const IPC::Message& message,
                                          int tab_handle,
                                          const std::wstring& encoding_name) {
  bool succeed = false;
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* nav = tab_tracker_->GetResource(tab_handle);
    Browser* browser = FindAndActivateTab(nav);
    DCHECK(browser);

    if (browser->IsCommandEnabled(IDC_ENCODING_MENU)) {
      TabContents* tab_contents = nav->active_contents();
      DCHECK(tab_contents->type() == TAB_CONTENTS_WEB);
      int selected_encoding_id =
          CharacterEncoding::GetCommandIdByCanonicalEncodingName(encoding_name);
      if (selected_encoding_id) {
        browser->OverrideEncoding(selected_encoding_id);
        succeed = true;
      }
    }
  }
  Send(new AutomationMsg_OverrideEncodingResponse(message.routing_id(),
                                                  succeed));
}
