// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/file_version_info.h"
#include "base/json_reader.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/values.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/automation/automation_extension_function.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/browser/automation/extension_port_container.h"
#include "chrome/browser/automation/url_request_failed_dns_job.h"
#include "chrome/browser/automation/url_request_mock_http_job.h"
#include "chrome/browser/automation/url_request_slow_download_job.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/find_bar.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/find_notification_details.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/ssl/ssl_manager.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/platform_util.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/automation/automation_messages.h"
#include "net/base/cookie_monster.h"
#include "net/proxy/proxy_service.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"

#if defined(OS_WIN)
// TODO(port): Port these headers.
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/external_tab_container.h"
#include "chrome/browser/printing/print_job.h"
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_LINUX)
// TODO(port): Port these to the mac.
#include "chrome/browser/login_prompt.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/views/bookmark_bar_view.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_win.h"
#include "views/window/window.h"
#endif

using base::Time;

class InitialLoadObserver : public NotificationObserver {
 public:
  InitialLoadObserver(size_t tab_count, AutomationProvider* automation)
      : automation_(automation),
        outstanding_tab_count_(tab_count) {
    if (outstanding_tab_count_ > 0) {
      registrar_.Add(this, NotificationType::LOAD_START,
                     NotificationService::AllSources());
      registrar_.Add(this, NotificationType::LOAD_STOP,
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
    if (type == NotificationType::LOAD_START) {
      if (outstanding_tab_count_ > loading_tabs_.size())
        loading_tabs_.insert(source.map_key());
    } else if (type == NotificationType::LOAD_STOP) {
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
    registrar_.Add(this, NotificationType::INITIAL_NEW_TAB_UI_LOAD,
                   NotificationService::AllSources());
  }

  ~NewTabUILoadObserver() {
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::INITIAL_NEW_TAB_UI_LOAD) {
      Details<int> load_time(details);
      automation_->Send(
          new AutomationMsg_InitialNewTabUILoadComplete(0, *load_time.ptr()));
    } else {
      NOTREACHED();
    }
  }

 private:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
};

class NavigationControllerRestoredObserver : public NotificationObserver {
 public:
  NavigationControllerRestoredObserver(AutomationProvider* automation,
                                       NavigationController* controller,
                                       IPC::Message* reply_message)
      : automation_(automation),
        controller_(controller),
        reply_message_(reply_message) {
    if (FinishedRestoring()) {
      SendDone();
    } else {
      registrar_.Add(this, NotificationType::LOAD_STOP,
                     NotificationService::AllSources());
    }
  }

  ~NavigationControllerRestoredObserver() {
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (FinishedRestoring()) {
      SendDone();
      registrar_.RemoveAll();
    }
  }

 private:
  bool FinishedRestoring() {
    return (!controller_->needs_reload() && !controller_->pending_entry() &&
            !controller_->tab_contents()->is_loading());
  }

  void SendDone() {
    DCHECK(reply_message_ != NULL);
    automation_->Send(reply_message_);
  }

  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  NavigationController* controller_;
  IPC::Message* reply_message_;

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerRestoredObserver);
};

template<class NavigationCodeType>
class NavigationNotificationObserver : public NotificationObserver {
 public:
  NavigationNotificationObserver(NavigationController* controller,
                                 AutomationProvider* automation,
                                 IPC::Message* reply_message,
                                 NavigationCodeType success_code,
                                 NavigationCodeType auth_needed_code,
                                 NavigationCodeType failed_code)
    : automation_(automation),
      reply_message_(reply_message),
      controller_(controller),
      navigation_started_(false),
      success_code_(success_code),
      auth_needed_code_(auth_needed_code),
      failed_code_(failed_code) {
    Source<NavigationController> source(controller_);
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED, source);
    registrar_.Add(this, NotificationType::LOAD_START, source);
    registrar_.Add(this, NotificationType::LOAD_STOP, source);
    registrar_.Add(this, NotificationType::AUTH_NEEDED, source);
    registrar_.Add(this, NotificationType::AUTH_SUPPLIED, source);
  }

  ~NavigationNotificationObserver() {
    if (reply_message_) {
      // This means we did not receive a notification for this navigation.
      // Send over a failed navigation status back to the caller to ensure that
      // the caller does not hang waiting for the response.
      IPC::ParamTraits<NavigationCodeType>::Write(reply_message_,
                                                  failed_code_);
      automation_->Send(reply_message_);
      reply_message_ = NULL;
    }

    automation_->RemoveNavigationStatusListener(this);
  }

  void ConditionMet(NavigationCodeType navigation_result) {
    DCHECK(reply_message_ != NULL);

    IPC::ParamTraits<NavigationCodeType>::Write(reply_message_,
                                                navigation_result);
    automation_->Send(reply_message_);
    reply_message_ = NULL;

    delete this;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    // We listen for 2 events to determine when the navigation started because:
    // - when this is used by the WaitForNavigation method, we might be invoked
    // afer the load has started (but not after the entry was committed, as
    // WaitForNavigation compares times of the last navigation).
    // - when this is used with a page requiring authentication, we will not get
    // a NotificationType::NAV_ENTRY_COMMITTED until after we authenticate, so
    // we need the NotificationType::LOAD_START.
    if (type == NotificationType::NAV_ENTRY_COMMITTED ||
        type == NotificationType::LOAD_START) {
      navigation_started_ = true;
    } else if (type == NotificationType::LOAD_STOP) {
      if (navigation_started_) {
        navigation_started_ = false;
        ConditionMet(success_code_);
      }
    } else if (type == NotificationType::AUTH_SUPPLIED) {
      // The LoginHandler for this tab is no longer valid.
      automation_->RemoveLoginHandler(controller_);

      // Treat this as if navigation started again, since load start/stop don't
      // occur while authentication is ongoing.
      navigation_started_ = true;
    } else if (type == NotificationType::AUTH_NEEDED) {
#if defined(OS_WIN)
      if (navigation_started_) {
        // Remember the login handler that wants authentication.
        LoginHandler* handler =
            Details<LoginNotificationDetails>(details)->handler();
        automation_->AddLoginHandler(controller_, handler);

        // Respond that authentication is needed.
        navigation_started_ = false;
        ConditionMet(auth_needed_code_);
      } else {
        NOTREACHED();
      }
#else
      // TODO(port): Enable when we have LoginNotificationDetails etc.
      NOTIMPLEMENTED();
#endif
    } else {
      NOTREACHED();
    }
  }

 private:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  IPC::Message* reply_message_;
  NavigationController* controller_;
  bool navigation_started_;
  NavigationCodeType success_code_;
  NavigationCodeType auth_needed_code_;
  NavigationCodeType failed_code_;
};

class TabStripNotificationObserver : public NotificationObserver {
 public:
  TabStripNotificationObserver(NotificationType notification,
                               AutomationProvider* automation)
      : automation_(automation),
        notification_(notification) {
    registrar_.Add(this, notification_, NotificationService::AllSources());
  }

  virtual ~TabStripNotificationObserver() {
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
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  NotificationType notification_;
};

class TabAppendedNotificationObserver : public TabStripNotificationObserver {
 public:
  TabAppendedNotificationObserver(Browser* parent,
                                  AutomationProvider* automation,
                                  IPC::Message* reply_message)
      : TabStripNotificationObserver(NotificationType::TAB_PARENTED,
                                     automation),
        parent_(parent),
        reply_message_(reply_message) {
  }

  virtual void ObserveTab(NavigationController* controller) {
    if (automation_->GetIndexForNavigationController(controller, parent_) ==
        TabStripModel::kNoTab) {
      // This tab notification doesn't belong to the parent_.
      return;
    }

    // Give the same response even if auth is needed, since it doesn't matter.
    automation_->AddNavigationStatusListener<int>(
        controller, reply_message_, AUTOMATION_MSG_NAVIGATION_SUCCESS,
        AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED, AUTOMATION_MSG_NAVIGATION_ERROR);
  }

 protected:
  Browser* parent_;
  IPC::Message* reply_message_;
};

class TabClosedNotificationObserver : public TabStripNotificationObserver {
 public:
  TabClosedNotificationObserver(AutomationProvider* automation,
                                bool wait_until_closed,
                                IPC::Message* reply_message)
      : TabStripNotificationObserver(wait_until_closed ?
            NotificationType::TAB_CLOSED : NotificationType::TAB_CLOSING,
            automation),
        reply_message_(reply_message),
        for_browser_command_(false) {
  }

  virtual void ObserveTab(NavigationController* controller) {
    if (for_browser_command_) {
      AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_,
                                                           true);
    } else {
      AutomationMsg_CloseTab::WriteReplyParams(reply_message_, true);
    }
    automation_->Send(reply_message_);
  }

  void set_for_browser_command(bool for_browser_command) {
    for_browser_command_ = for_browser_command;
  }

 protected:
  IPC::Message* reply_message_;
  bool for_browser_command_;
};

class BrowserOpenedNotificationObserver : public NotificationObserver {
 public:
  BrowserOpenedNotificationObserver(AutomationProvider* automation,
                                    IPC::Message* reply_message)
      : automation_(automation),
        reply_message_(reply_message),
        for_browser_command_(false) {
    registrar_.Add(this, NotificationType::BROWSER_OPENED,
                   NotificationService::AllSources());
  }

  ~BrowserOpenedNotificationObserver() {
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::BROWSER_OPENED) {
      if (for_browser_command_) {
        AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_,
                                                             true);
      }
      automation_->Send(reply_message_);
      delete this;
    } else {
      NOTREACHED();
    }
  }

  void set_for_browser_command(bool for_browser_command) {
    for_browser_command_ = for_browser_command;
  }

 private:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  IPC::Message* reply_message_;
  bool for_browser_command_;
};

class BrowserClosedNotificationObserver : public NotificationObserver {
 public:
  BrowserClosedNotificationObserver(Browser* browser,
                                    AutomationProvider* automation,
                                    IPC::Message* reply_message)
      : automation_(automation),
        reply_message_(reply_message),
        for_browser_command_(false) {
    registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                   Source<Browser>(browser));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::BROWSER_CLOSED);
    Details<bool> close_app(details);
    DCHECK(reply_message_ != NULL);
    if (for_browser_command_) {
      AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_,
                                                           true);
    } else {
      AutomationMsg_CloseBrowser::WriteReplyParams(reply_message_, true,
                                                   *(close_app.ptr()));
    }
    automation_->Send(reply_message_);
    reply_message_ = NULL;
    delete this;
  }

  void set_for_browser_command(bool for_browser_command) {
    for_browser_command_ = for_browser_command;
  }

 private:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  IPC::Message* reply_message_;
  bool for_browser_command_;
};

namespace {

// Define mapping from command to notification
struct CommandNotification {
  int command;
  NotificationType::Type notification_type;
};

const struct CommandNotification command_notifications[] = {
  {IDC_DUPLICATE_TAB, NotificationType::TAB_PARENTED},
  {IDC_NEW_TAB, NotificationType::TAB_PARENTED},
  // Returns as soon as the restored tab is created. To further wait until
  // the content page is loaded, use WaitForTabToBeRestored.
  {IDC_RESTORE_TAB, NotificationType::TAB_PARENTED}
};

}  // namespace

class ExecuteBrowserCommandObserver : public NotificationObserver {
 public:
  ~ExecuteBrowserCommandObserver() {
  }

  static bool CreateAndRegisterObserver(AutomationProvider* automation,
                                        Browser* browser,
                                        int command,
                                        IPC::Message* reply_message) {
    bool result = true;
    switch (command) {
      case IDC_NEW_WINDOW:
      case IDC_NEW_INCOGNITO_WINDOW: {
        BrowserOpenedNotificationObserver* observer =
            new BrowserOpenedNotificationObserver(automation, reply_message);
        observer->set_for_browser_command(true);
        break;
      }
      case IDC_CLOSE_WINDOW: {
        BrowserClosedNotificationObserver* observer =
            new BrowserClosedNotificationObserver(browser, automation,
                                                  reply_message);
        observer->set_for_browser_command(true);
        break;
      }
      case IDC_CLOSE_TAB: {
        TabClosedNotificationObserver* observer =
            new TabClosedNotificationObserver(automation, true, reply_message);
        observer->set_for_browser_command(true);
        break;
      }
      case IDC_BACK:
      case IDC_FORWARD:
      case IDC_RELOAD: {
        automation->
        AddNavigationStatusListener<AutomationMsg_NavigationResponseValues>(
            &browser->GetSelectedTabContents()->controller(),
            reply_message,
            AUTOMATION_MSG_NAVIGATION_SUCCESS,
            AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            AUTOMATION_MSG_NAVIGATION_ERROR);
        break;
      }
      default: {
        ExecuteBrowserCommandObserver* observer =
            new ExecuteBrowserCommandObserver(automation, reply_message);
        if (!observer->Register(command)) {
          delete observer;
          result = false;
        }
        break;
      }
    }
    return result;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == notification_type_) {
      AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message_,
                                                           true);
      automation_->Send(reply_message_);
      delete this;
    } else {
      NOTREACHED();
    }
  }

 private:
  ExecuteBrowserCommandObserver(AutomationProvider* automation,
                                IPC::Message* reply_message)
      : automation_(automation),
        reply_message_(reply_message) {
  }

  bool Register(int command) {
    if (!GetNotificationType(command, &notification_type_))
      return false;
    registrar_.Add(this, notification_type_, NotificationService::AllSources());
    return true;
  }

  bool GetNotificationType(int command, NotificationType::Type* type) {
    if (!type)
      return false;
    bool found = false;
    for (unsigned int i = 0; i < arraysize(command_notifications); i++) {
      if (command_notifications[i].command == command) {
        *type = command_notifications[i].notification_type;
        found = true;
        break;
      }
    }
    return found;
  }

  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  NotificationType::Type notification_type_;
  IPC::Message* reply_message_;
};

class FindInPageNotificationObserver : public NotificationObserver {
 public:
  FindInPageNotificationObserver(AutomationProvider* automation,
                                 TabContents* parent_tab,
                                 IPC::Message* reply_message)
      : automation_(automation),
        active_match_ordinal_(-1),
        reply_message_(reply_message) {
    registrar_.Add(this, NotificationType::FIND_RESULT_AVAILABLE,
                   Source<TabContents>(parent_tab));
  }

  ~FindInPageNotificationObserver() {
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::FIND_RESULT_AVAILABLE) {
      Details<FindNotificationDetails> find_details(details);
      if (find_details->request_id() == kFindInPageRequestId) {
        // We get multiple responses and one of those will contain the ordinal.
        // This message comes to us before the final update is sent.
        if (find_details->active_match_ordinal() > -1)
          active_match_ordinal_ = find_details->active_match_ordinal();
        if (find_details->final_update()) {
          DCHECK(reply_message_ != NULL);

          AutomationMsg_FindInPage::WriteReplyParams(reply_message_,
              active_match_ordinal_, find_details->number_of_matches());

          automation_->Send(reply_message_);
          reply_message_ = NULL;
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
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
  // We will at some point (before final update) be notified of the ordinal and
  // we need to preserve it so we can send it later.
  int active_match_ordinal_;
  IPC::Message* reply_message_;
};

const int FindInPageNotificationObserver::kFindInPageRequestId = -1;

class DomOperationNotificationObserver : public NotificationObserver {
 public:
  explicit DomOperationNotificationObserver(AutomationProvider* automation)
      : automation_(automation) {
    registrar_.Add(this, NotificationType::DOM_OPERATION_RESPONSE,
                   NotificationService::AllSources());
  }

  ~DomOperationNotificationObserver() {
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (NotificationType::DOM_OPERATION_RESPONSE == type) {
      Details<DomOperationNotificationDetails> dom_op_details(details);

      IPC::Message* reply_message = automation_->reply_message_release();
      DCHECK(reply_message != NULL);

      AutomationMsg_DomOperation::WriteReplyParams(reply_message,
                                                   dom_op_details->json());
      automation_->Send(reply_message);
    }
  }

 private:
  NotificationRegistrar registrar_;
  AutomationProvider* automation_;
};

#if defined(OS_WIN)
// TODO(port): Enable when printing is ported.
class DocumentPrintedNotificationObserver : public NotificationObserver {
 public:
  DocumentPrintedNotificationObserver(AutomationProvider* automation,
                                      IPC::Message* reply_message)
      : automation_(automation),
        success_(false),
        reply_message_(reply_message) {
    registrar_.Add(this, NotificationType::PRINT_JOB_EVENT,
                   NotificationService::AllSources());
  }

  ~DocumentPrintedNotificationObserver() {
    DCHECK(reply_message_ != NULL);
    AutomationMsg_PrintNow::WriteReplyParams(reply_message_, success_);
    automation_->Send(reply_message_);
    automation_->RemoveNavigationStatusListener(this);
  }

  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details) {
    using namespace printing;
    DCHECK(type == NotificationType::PRINT_JOB_EVENT);
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
  NotificationRegistrar registrar_;
  scoped_refptr<AutomationProvider> automation_;
  bool success_;
  IPC::Message* reply_message_;
};
#endif  // defined(OS_WIN)

class AutomationInterstitialPage : public InterstitialPage {
 public:
  AutomationInterstitialPage(TabContents* tab,
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
      profile_(profile),
      reply_message_(NULL) {
  browser_tracker_.reset(new AutomationBrowserTracker(this));
  tab_tracker_.reset(new AutomationTabTracker(this));
  window_tracker_.reset(new AutomationWindowTracker(this));
  autocomplete_edit_tracker_.reset(
      new AutomationAutocompleteEditTracker(this));
  new_tab_ui_load_observer_.reset(new NewTabUILoadObserver(this));
  dom_operation_observer_.reset(new DomOperationNotificationObserver(this));
}

AutomationProvider::~AutomationProvider() {
  STLDeleteContainerPairSecondPointers(port_containers_.begin(),
                                       port_containers_.end());
  port_containers_.clear();

  // Make sure that any outstanding NotificationObservers also get destroyed.
  ObserverList<NotificationObserver>::Iterator it(notification_observer_list_);
  NotificationObserver* observer;
  while ((observer = it.GetNext()) != NULL)
    delete observer;
}

void AutomationProvider::ConnectToChannel(const std::string& channel_id) {
  channel_.reset(
    new IPC::SyncChannel(channel_id, IPC::Channel::MODE_CLIENT, this, NULL,
                         g_browser_process->io_thread()->message_loop(),
                         true, g_browser_process->shutdown_event()));
  scoped_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  std::string version_string;
  if (file_version_info != NULL) {
    version_string = WideToASCII(file_version_info->file_version());
  }

  // Send a hello message with our current automation protocol version.
  channel_->Send(new AutomationMsg_Hello(0, version_string.c_str()));
}

void AutomationProvider::SetExpectedTabCount(size_t expected_tabs) {
  if (expected_tabs == 0) {
    Send(new AutomationMsg_InitialLoadsComplete(0));
  } else {
    initial_load_observer_.reset(new InitialLoadObserver(expected_tabs, this));
  }
}

template<class NavigationCodeType>
NotificationObserver* AutomationProvider::AddNavigationStatusListener(
    NavigationController* tab, IPC::Message* reply_message,
    NavigationCodeType success_code,
    NavigationCodeType auth_needed_code,
    NavigationCodeType failed_code) {
  NotificationObserver* observer =
    new NavigationNotificationObserver<NavigationCodeType>(
        tab, this, reply_message, success_code, auth_needed_code,
        failed_code);

  notification_observer_list_.AddObserver(observer);
  return observer;
}

void AutomationProvider::RemoveNavigationStatusListener(
    NotificationObserver* obs) {
  notification_observer_list_.RemoveObserver(obs);
}

NotificationObserver* AutomationProvider::AddTabStripObserver(
    Browser* parent,
    IPC::Message* reply_message) {
  NotificationObserver* observer =
      new TabAppendedNotificationObserver(parent, this, reply_message);
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

void AutomationProvider::AddPortContainer(ExtensionPortContainer* port) {
  int port_id = port->port_id();
  DCHECK_NE(-1, port_id);
  DCHECK(port_containers_.find(port_id) == port_containers_.end());

  port_containers_[port_id] = port;
}

void AutomationProvider::RemovePortContainer(ExtensionPortContainer* port) {
  int port_id = port->port_id();
  DCHECK_NE(-1, port_id);

  PortContainerMap::iterator it = port_containers_.find(port_id);
  DCHECK(it != port_containers_.end());

  if (it != port_containers_.end()) {
    delete it->second;
    port_containers_.erase(it);
  }
}

ExtensionPortContainer* AutomationProvider::GetPortContainer(
    int port_id) const {
  PortContainerMap::const_iterator it = port_containers_.find(port_id);
  if (it == port_containers_.end())
    return NULL;

  return it->second;
}

int AutomationProvider::GetIndexForNavigationController(
    const NavigationController* controller, const Browser* parent) const {
  DCHECK(parent);
  return parent->GetIndexOfController(controller);
}

void AutomationProvider::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(AutomationProvider, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CloseBrowser, CloseBrowser)
    IPC_MESSAGE_HANDLER(AutomationMsg_CloseBrowserRequestAsync,
                        CloseBrowserAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActivateTab, ActivateTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActiveTabIndex, GetActiveTabIndex)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_AppendTab, AppendTab)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CloseTab, CloseTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetCookies, GetCookies)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetCookie, SetCookie)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_NavigateToURL, NavigateToURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationAsync, NavigationAsync)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_GoBack, GoBack)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_GoForward, GoForward)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_Reload, Reload)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_SetAuth, SetAuth)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CancelAuth, CancelAuth)
    IPC_MESSAGE_HANDLER(AutomationMsg_NeedsAuth, NeedsAuth)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_RedirectsFrom,
                                    GetRedirectsFrom)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserWindowCount, GetBrowserWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_NormalBrowserWindowCount,
                        GetNormalBrowserWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserWindow, GetBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetBrowserLocale, GetBrowserLocale)
    IPC_MESSAGE_HANDLER(AutomationMsg_LastActiveBrowserWindow,
                        GetLastActiveBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActiveWindow, GetActiveWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindNormalBrowserWindow,
                        FindNormalBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsWindowActive, IsWindowActive)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActivateWindow, ActivateWindow)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowHWND, GetWindowHWND)
#endif  // defined(OS_WIN)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowExecuteCommandAsync,
                        ExecuteBrowserCommandAsync)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WindowExecuteCommand,
                        ExecuteBrowserCommand)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowViewBounds, WindowGetViewBounds)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetWindowVisible, SetWindowVisible)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowClick, WindowSimulateClick)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowKeyPress, WindowSimulateKeyPress)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WindowDrag,
                                    WindowSimulateDrag)
#endif  // defined(OS_WIN)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabCount, GetTabCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_Tab, GetTab)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabHWND, GetTabHWND)
#endif  // defined(OS_WIN)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabProcessID, GetTabProcessID)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabTitle, GetTabTitle)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabIndex, GetTabIndex)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabURL, GetTabURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShelfVisibility, GetShelfVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_HandleUnused, HandleUnused)
    IPC_MESSAGE_HANDLER(AutomationMsg_ApplyAccelerator, ApplyAccelerator)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_DomOperation,
                                    ExecuteJavascript)
    IPC_MESSAGE_HANDLER(AutomationMsg_ConstrainedWindowCount,
                        GetConstrainedWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindInPage, HandleFindInPageRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetFocusedViewID, GetFocusedViewID)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_InspectElement,
                                    HandleInspectElementRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetFilteredInet, SetFilteredInet)
    IPC_MESSAGE_HANDLER(AutomationMsg_DownloadDirectory, GetDownloadDirectory)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetProxyConfig, SetProxyConfig);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_OpenNewBrowserWindow,
                                    OpenNewBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowForBrowser, GetWindowForBrowser)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditForBrowser,
                        GetAutocompleteEditForBrowser)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserForWindow, GetBrowserForWindow)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(AutomationMsg_CreateExternalTab, CreateExternalTab)
#endif
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigateInExternalTab,
                        NavigateInExternalTab)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_ShowInterstitialPage,
                                    ShowInterstitialPage)
    IPC_MESSAGE_HANDLER(AutomationMsg_HideInterstitialPage,
                        HideInterstitialPage)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetAcceleratorsForTab,
                        SetAcceleratorsForTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_ProcessUnhandledAccelerator,
                        ProcessUnhandledAccelerator)
#endif
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForTabToBeRestored,
                                    WaitForTabToBeRestored)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetInitialFocus, SetInitialFocus)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabReposition, OnTabReposition)
    IPC_MESSAGE_HANDLER(AutomationMsg_ForwardContextMenuCommandToChrome,
                        OnForwardContextMenuCommandToChrome)
#endif
    IPC_MESSAGE_HANDLER(AutomationMsg_GetSecurityState, GetSecurityState)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetPageType, GetPageType)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_ActionOnSSLBlockingPage,
                                    ActionOnSSLBlockingPage)
    IPC_MESSAGE_HANDLER(AutomationMsg_BringBrowserToFront, BringBrowserToFront)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsPageMenuCommandEnabled,
                        IsPageMenuCommandEnabled)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_PrintNow, PrintNow)
    IPC_MESSAGE_HANDLER(AutomationMsg_SavePage, SavePage)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditGetText,
                        GetAutocompleteEditText)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditSetText,
                        SetAutocompleteEditText)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditIsQueryInProgress,
                        AutocompleteEditIsQueryInProgress)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditGetMatches,
                        AutocompleteEditGetMatches)
    IPC_MESSAGE_HANDLER(AutomationMsg_OpenFindInPage,
                        HandleOpenFindInPageRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_HandleMessageFromExternalHost,
                        OnMessageFromExternalHost)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_Find, HandleFindRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindWindowVisibility,
                        GetFindWindowVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindWindowLocation,
                        HandleFindWindowLocationRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_BookmarkBarVisibility,
                        GetBookmarkBarVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetSSLInfoBarCount, GetSSLInfoBarCount)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_ClickSSLInfoBarLink,
                                    ClickSSLInfoBarLink)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetLastNavigationTime,
                        GetLastNavigationTime)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForNavigation,
                                    WaitForNavigation)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetIntPreference, SetIntPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShowingAppModalDialog,
                        GetShowingAppModalDialog)
    IPC_MESSAGE_HANDLER(AutomationMsg_ClickAppModalDialogButton,
                        ClickAppModalDialogButton)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetStringPreference, SetStringPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetBooleanPreference,
                        GetBooleanPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetBooleanPreference,
                        SetBooleanPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetPageCurrentEncoding,
                        GetPageCurrentEncoding)
    IPC_MESSAGE_HANDLER(AutomationMsg_OverrideEncoding, OverrideEncoding)
    IPC_MESSAGE_HANDLER(AutomationMsg_SavePackageShouldPromptUser,
                        SavePackageShouldPromptUser)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowTitle, GetWindowTitle)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetEnableExtensionAutomation,
                        SetEnableExtensionAutomation)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetShelfVisibility, SetShelfVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_BlockedPopupCount, GetBlockedPopupCount)
  IPC_END_MESSAGE_MAP()
}

void AutomationProvider::ActivateTab(int handle, int at_index, int* status) {
  *status = -1;
  if (browser_tracker_->ContainsHandle(handle) && at_index > -1) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (at_index >= 0 && at_index < browser->tab_count()) {
      browser->SelectTabContentsAt(at_index, true);
      *status = 0;
    }
  }
}

void AutomationProvider::AppendTab(int handle, const GURL& url,
                                   IPC::Message* reply_message) {
  int append_tab_response = -1;  // -1 is the error code
  NotificationObserver* observer = NULL;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    observer = AddTabStripObserver(browser, reply_message);
    TabContents* tab_contents = browser->AddTabWithURL(url, GURL(),
                                                       PageTransition::TYPED,
                                                       true, -1, false, NULL);
    if (tab_contents) {
      append_tab_response =
          GetIndexForNavigationController(&tab_contents->controller(), browser);
    }
  }

  if (append_tab_response < 0) {
    // The append tab failed. Remove the TabStripObserver
    if (observer) {
      RemoveTabStripObserver(observer);
      delete observer;
    }

    AutomationMsg_AppendTab::WriteReplyParams(reply_message,
                                              append_tab_response);
    Send(reply_message);
  }
}

void AutomationProvider::NavigateToURL(int handle, const GURL& url,
                                       IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);

    // Simulate what a user would do. Activate the tab and then navigate.
    // We could allow navigating in a background tab in future.
    Browser* browser = FindAndActivateTab(tab);

    if (browser) {
      AddNavigationStatusListener<AutomationMsg_NavigationResponseValues>(
          tab, reply_message, AUTOMATION_MSG_NAVIGATION_SUCCESS,
          AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
          AUTOMATION_MSG_NAVIGATION_ERROR);

      // TODO(darin): avoid conversion to GURL
      browser->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);
      return;
    }
  }

  AutomationMsg_NavigateToURL::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void AutomationProvider::NavigationAsync(int handle, const GURL& url,
                                         bool* status) {
  *status = false;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);

    // Simulate what a user would do. Activate the tab and then navigate.
    // We could allow navigating in a background tab in future.
    Browser* browser = FindAndActivateTab(tab);

    if (browser) {
      // Don't add any listener unless a callback mechanism is desired.
      // TODO(vibhor): Do this if such a requirement arises in future.
      browser->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);
      *status = true;
    }
  }
}

void AutomationProvider::GoBack(int handle, IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_BACK)) {
      AddNavigationStatusListener<AutomationMsg_NavigationResponseValues>(
          tab, reply_message, AUTOMATION_MSG_NAVIGATION_SUCCESS,
          AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
          AUTOMATION_MSG_NAVIGATION_ERROR);
      browser->GoBack(CURRENT_TAB);
      return;
    }
  }

  AutomationMsg_GoBack::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void AutomationProvider::GoForward(int handle, IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_FORWARD)) {
      AddNavigationStatusListener<AutomationMsg_NavigationResponseValues>(
          tab, reply_message, AUTOMATION_MSG_NAVIGATION_SUCCESS,
          AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
          AUTOMATION_MSG_NAVIGATION_ERROR);
      browser->GoForward(CURRENT_TAB);
      return;
    }
  }

  AutomationMsg_GoForward::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void AutomationProvider::Reload(int handle, IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_RELOAD)) {
      AddNavigationStatusListener<AutomationMsg_NavigationResponseValues>(
          tab, reply_message, AUTOMATION_MSG_NAVIGATION_SUCCESS,
          AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
          AUTOMATION_MSG_NAVIGATION_ERROR);
      browser->Reload();
      return;
    }
  }

  AutomationMsg_Reload::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void AutomationProvider::SetAuth(int tab_handle,
                                 const std::wstring& username,
                                 const std::wstring& password,
                                 IPC::Message* reply_message) {
  int status = -1;

  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // If auth is needed again after this, assume login has failed.  This is
      // not strictly correct, because a navigation can require both proxy and
      // server auth, but it should be OK for now.
      LoginHandler* handler = iter->second;
      AddNavigationStatusListener<int>(tab, reply_message, 0, -1, -1);
      handler->SetAuth(username, password);
      status = 0;
    }
  }

  if (status < 0) {
    AutomationMsg_SetAuth::WriteReplyParams(reply_message, status);
    Send(reply_message);
  }
}

void AutomationProvider::CancelAuth(int tab_handle,
                                    IPC::Message* reply_message) {
  int status = -1;

  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // If auth is needed again after this, something is screwy.
      LoginHandler* handler = iter->second;
      AddNavigationStatusListener<int>(tab, reply_message, 0, -1, -1);
      handler->CancelAuth();
      status = 0;
    }
  }

  if (status < 0) {
    AutomationMsg_CancelAuth::WriteReplyParams(reply_message, status);
    Send(reply_message);
  }
}

void AutomationProvider::NeedsAuth(int tab_handle, bool* needs_auth) {
  *needs_auth = false;

  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // The LoginHandler will be in our map IFF the tab needs auth.
      *needs_auth = true;
    }
  }
}

void AutomationProvider::GetRedirectsFrom(int tab_handle,
                                          const GURL& source_url,
                                          IPC::Message* reply_message) {
  DCHECK(!redirect_query_) << "Can only handle one redirect query at once.";
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    HistoryService* history_service =
        tab->profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);

    DCHECK(history_service) << "Tab " << tab_handle << "'s profile " <<
                               "has no history service";
    if (history_service) {
      DCHECK(reply_message_ == NULL);
      reply_message_ = reply_message;
      // Schedule a history query for redirects. The response will be sent
      // asynchronously from the callback the history system uses to notify us
      // that it's done: OnRedirectQueryComplete.
      redirect_query_ = history_service->QueryRedirectsFrom(
          source_url, &consumer_,
          NewCallback(this, &AutomationProvider::OnRedirectQueryComplete));
      return;  // Response will be sent when query completes.
    }
  }

  // Send failure response.
  std::vector<GURL> empty;
  AutomationMsg_RedirectsFrom::WriteReplyParams(reply_message, false, empty);
  Send(reply_message);
}

void AutomationProvider::GetActiveTabIndex(int handle, int* active_tab_index) {
  *active_tab_index = -1;  // -1 is the error code
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    *active_tab_index = browser->selected_index();
  }
}

void AutomationProvider::GetBrowserLocale(string16* locale) {
  DCHECK(g_browser_process);
  *locale = ASCIIToUTF16(g_browser_process->GetApplicationLocale());
}

void AutomationProvider::GetBrowserWindowCount(int* window_count) {
  *window_count = static_cast<int>(BrowserList::size());
}

void AutomationProvider::GetNormalBrowserWindowCount(int* window_count) {
  *window_count = static_cast<int>(
      BrowserList::GetBrowserCountForType(profile_, Browser::TYPE_NORMAL));
}

void AutomationProvider::GetShowingAppModalDialog(bool* showing_dialog,
                                                  int* dialog_button) {
  AppModalDialog* dialog_delegate =
      Singleton<AppModalDialogQueue>()->active_dialog();
  *showing_dialog = (dialog_delegate != NULL);
  if (*showing_dialog)
    *dialog_button = dialog_delegate->GetDialogButtons();
  else
    *dialog_button = MessageBoxFlags::DIALOGBUTTON_NONE;
}

void AutomationProvider::ClickAppModalDialogButton(int button, bool* success) {
  *success = false;

  AppModalDialog* dialog_delegate =
      Singleton<AppModalDialogQueue>()->active_dialog();
  if (dialog_delegate &&
      (dialog_delegate->GetDialogButtons() & button) == button) {
    if ((button & MessageBoxFlags::DIALOGBUTTON_OK) ==
        MessageBoxFlags::DIALOGBUTTON_OK) {
      dialog_delegate->AcceptWindow();
      *success =  true;
    }
    if ((button & MessageBoxFlags::DIALOGBUTTON_CANCEL) ==
        MessageBoxFlags::DIALOGBUTTON_CANCEL) {
      DCHECK(!*success) << "invalid param, OK and CANCEL specified";
      dialog_delegate->CancelWindow();
      *success =  true;
    }
  }
}

void AutomationProvider::GetBrowserWindow(int index, int* handle) {
  *handle = 0;
  if (index >= 0) {
    BrowserList::const_iterator iter = BrowserList::begin();
    for (; (iter != BrowserList::end()) && (index > 0); ++iter, --index);
    if (iter != BrowserList::end()) {
      *handle = browser_tracker_->Add(*iter);
    }
  }
}

void AutomationProvider::FindNormalBrowserWindow(int* handle) {
  *handle = 0;
  Browser* browser = BrowserList::FindBrowserWithType(profile_,
                                                      Browser::TYPE_NORMAL);
  if (browser)
    *handle = browser_tracker_->Add(browser);
}

void AutomationProvider::GetLastActiveBrowserWindow(int* handle) {
  *handle = 0;
  Browser* browser = BrowserList::GetLastActive();
  if (browser)
    *handle = browser_tracker_->Add(browser);
}

#if defined(OS_WIN)
// TODO(port): Remove windowsisms.
BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM l_param) {
  if (hwnd == reinterpret_cast<HWND>(l_param)) {
    return FALSE;
  }
  return TRUE;
}

void AutomationProvider::GetActiveWindow(int* handle) {
  HWND window = GetForegroundWindow();

  // Let's make sure this window belongs to our process.
  if (EnumThreadWindows(::GetCurrentThreadId(),
                        EnumThreadWndProc,
                        reinterpret_cast<LPARAM>(window))) {
    // We enumerated all the windows and did not find the foreground window,
    // it is not our window, ignore it.
    *handle = 0;
    return;
  }

  *handle = window_tracker_->Add(window);
}

void AutomationProvider::GetWindowHWND(int handle, HWND* win32_handle) {
  *win32_handle = window_tracker_->GetResource(handle);
}
#endif  // defined(OS_WIN)

void AutomationProvider::ExecuteBrowserCommandAsync(int handle, int command,
                                                    bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser->command_updater()->SupportsCommand(command) &&
        browser->command_updater()->IsCommandEnabled(command)) {
      browser->ExecuteCommand(command);
      *success = true;
    }
  }
}

void AutomationProvider::ExecuteBrowserCommand(
    int handle, int command, IPC::Message* reply_message) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser->command_updater()->SupportsCommand(command) &&
        browser->command_updater()->IsCommandEnabled(command)) {
      if (ExecuteBrowserCommandObserver::CreateAndRegisterObserver(
          this, browser, command, reply_message)) {
        browser->ExecuteCommand(command);
        return;
      }
    }
  }
  AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void AutomationProvider::WindowGetViewBounds(int handle, int view_id,
                                             bool screen_coordinates,
                                             bool* success,
                                             gfx::Rect* bounds) {
  *success = false;

#if defined(OS_WIN)
  void* iter = NULL;
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    views::RootView* root_view = views::WidgetWin::FindRootView(hwnd);
    if (root_view) {
      views::View* view = root_view->GetViewByID(view_id);
      if (view) {
        *success = true;
        gfx::Point point;
        if (screen_coordinates)
          views::View::ConvertPointToScreen(view, &point);
        else
          views::View::ConvertPointToView(view, root_view, &point);
        *bounds = view->GetLocalBounds(false);
        bounds->set_origin(point);
      }
    }
  }
#else
  // TODO(port): Enable when window_tracker is ported.
  NOTIMPLEMENTED();
#endif
}

#if defined(OS_WIN)
// TODO(port): Use portable replacement for POINT.

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
#endif  // defined(OS_WIN)

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

#if defined(OS_WIN)
// TODO(port): Replace POINT and other windowsisms.

// This task sends a WindowDragResponse message with the appropriate
// routing ID to the automation proxy.  This is implemented as a task so that
// we know that the mouse events (and any tasks that they spawn on the message
// loop) have been processed by the time this is sent.
class WindowDragResponseTask : public Task {
 public:
  WindowDragResponseTask(AutomationProvider* provider,
                         IPC::Message* reply_message)
      : provider_(provider), reply_message_(reply_message) {}
  virtual ~WindowDragResponseTask() {}

  virtual void Run() {
    DCHECK(reply_message_ != NULL);
    AutomationMsg_WindowDrag::WriteReplyParams(reply_message_, true);
    provider_->Send(reply_message_);
  }

 private:
  AutomationProvider* provider_;
  IPC::Message* reply_message_;

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

void AutomationProvider::WindowSimulateDrag(int handle,
                                            std::vector<POINT> drag_path,
                                            int flags,
                                            bool press_escape_en_route,
                                            IPC::Message* reply_message) {
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

    MessageLoop::current()->PostTask(FROM_HERE, new InvokeTaskLaterTask(
        new WindowDragResponseTask(this, reply_message)));
  } else {
    AutomationMsg_WindowDrag::WriteReplyParams(reply_message, true);
    Send(reply_message);
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

void AutomationProvider::GetFocusedViewID(int handle, int* view_id) {
  *view_id = -1;
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManagerForNativeView(hwnd);
    DCHECK(focus_manager);
    views::View* focused_view = focus_manager->GetFocusedView();
    if (focused_view)
      *view_id = focused_view->GetID();
  }
}

void AutomationProvider::SetWindowVisible(int handle, bool visible,
                                          bool* result) {
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    ::ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
    *result = true;
  } else {
    *result = false;
  }
}
#endif  // defined(OS_WIN)

void AutomationProvider::IsWindowActive(int handle, bool* success,
                                        bool* is_active) {
  if (window_tracker_->ContainsHandle(handle)) {
    *is_active =
        platform_util::IsWindowActive(window_tracker_->GetResource(handle));
    *success = true;
  } else {
    *success = false;
    *is_active = false;
  }
}

// TODO(port): port this.
#if defined(OS_WIN)
void AutomationProvider::ActivateWindow(int handle) {
  if (window_tracker_->ContainsHandle(handle)) {
    ::SetActiveWindow(window_tracker_->GetResource(handle));
  }
}
#endif

void AutomationProvider::GetTabCount(int handle, int* tab_count) {
  *tab_count = -1;  // -1 is the error code

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    *tab_count = browser->tab_count();
  }
}

void AutomationProvider::GetTab(int win_handle, int tab_index,
                                int* tab_handle) {
  *tab_handle = 0;
  if (browser_tracker_->ContainsHandle(win_handle) && (tab_index >= 0)) {
    Browser* browser = browser_tracker_->GetResource(win_handle);
    if (tab_index < browser->tab_count()) {
      TabContents* tab_contents =
          browser->GetTabContentsAt(tab_index);
      *tab_handle = tab_tracker_->Add(&tab_contents->controller());
    }
  }
}

void AutomationProvider::GetTabTitle(int handle, int* title_string_size,
                                     std::wstring* title) {
  *title_string_size = -1;  // -1 is the error code
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    if (entry != NULL) {
      *title = UTF16ToWideHack(entry->title());
    } else {
      *title = std::wstring();
    }
    *title_string_size = static_cast<int>(title->size());
  }
}

void AutomationProvider::GetTabIndex(int handle, int* tabstrip_index) {
  *tabstrip_index = -1;  // -1 is the error code

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = Browser::GetBrowserForController(tab, NULL);
    *tabstrip_index = browser->tabstrip_model()->GetIndexOfController(tab);
  }
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
  DCHECK(reply_message_ != NULL);

  std::vector<GURL> redirects_gurl;
  if (success) {
    reply_message_->WriteBool(true);
    for (size_t i = 0; i < redirects->size(); i++)
      redirects_gurl.push_back(redirects->at(i));
  } else {
    reply_message_->WriteInt(-1);  // Negative count indicates failure.
  }

  IPC::ParamTraits<std::vector<GURL> >::Write(reply_message_, redirects_gurl);

  Send(reply_message_);
  redirect_query_ = NULL;
  reply_message_ = NULL;
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

void AutomationProvider::GetCookies(const GURL& url, int handle,
                                    int* value_size,
                                    std::string* value) {
  *value_size = -1;
  if (url.is_valid() && tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    *value =
        tab->profile()->GetRequestContext()->cookie_store()->GetCookies(url);
    *value_size = static_cast<int>(value->size());
  }
}

void AutomationProvider::SetCookie(const GURL& url,
                                   const std::string value,
                                   int handle,
                                   int* response_value) {
  *response_value = -1;

  if (url.is_valid() && tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    URLRequestContext* context = tab->profile()->GetRequestContext();
    if (context->cookie_store()->SetCookie(url, value))
      *response_value = 1;
  }
}

void AutomationProvider::GetTabURL(int handle, bool* success, GURL* url) {
  *success = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    // Return what the user would see in the location bar.
    *url = tab->GetActiveEntry()->display_url();
    *success = true;
  }
}

#if defined(OS_WIN)
void AutomationProvider::GetTabHWND(int handle, HWND* tab_hwnd) {
  *tab_hwnd = NULL;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    *tab_hwnd = tab->tab_contents()->GetNativeView();
  }
}
#endif  // defined(OS_WIN)

void AutomationProvider::GetTabProcessID(int handle, int* process_id) {
  *process_id = -1;

  if (tab_tracker_->ContainsHandle(handle)) {
    *process_id = 0;
    TabContents* tab_contents =
        tab_tracker_->GetResource(handle)->tab_contents();
    if (tab_contents->process())
      *process_id = tab_contents->process()->process().pid();
  }
}

void AutomationProvider::ApplyAccelerator(int handle, int id) {
  NOTREACHED() << "This function has been deprecated. "
               << "Please use ExecuteBrowserCommandAsync instead.";
}

void AutomationProvider::ExecuteJavascript(int handle,
                                           const std::wstring& frame_xpath,
                                           const std::wstring& script,
                                           IPC::Message* reply_message) {
  bool succeeded = false;
  TabContents* tab_contents = GetTabContentsForHandle(handle, NULL);
  if (tab_contents) {
    // Set the routing id of this message with the controller.
    // This routing id needs to be remembered for the reverse
    // communication while sending back the response of
    // this javascript execution.
    std::wstring set_automation_id;
    SStringPrintf(&set_automation_id,
      L"window.domAutomationController.setAutomationId(%d);",
      reply_message->routing_id());

    DCHECK(reply_message_ == NULL);
    reply_message_ = reply_message;

    tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(
        frame_xpath, set_automation_id);
    tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(
        frame_xpath, script);
    succeeded = true;
  }

  if (!succeeded) {
    AutomationMsg_DomOperation::WriteReplyParams(reply_message, std::string());
    Send(reply_message);
  }
}

void AutomationProvider::GetShelfVisibility(int handle, bool* visible) {
  *visible = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      *visible = browser->window()->IsDownloadShelfVisible();
    }
  }
}

void AutomationProvider::SetShelfVisibility(int handle, bool visible) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      if (visible)
        browser->window()->GetDownloadShelf()->Show();
      else
        browser->window()->GetDownloadShelf()->Close();
    }
  }
}


void AutomationProvider::GetConstrainedWindowCount(int handle, int* count) {
  *count = -1;  // -1 is the error code
  if (tab_tracker_->ContainsHandle(handle)) {
      NavigationController* nav_controller = tab_tracker_->GetResource(handle);
      TabContents* tab_contents = nav_controller->tab_contents();
      if (tab_contents) {
        *count = static_cast<int>(tab_contents->child_windows_.size());
      }
  }
}

void AutomationProvider::HandleFindInPageRequest(
    int handle, const std::wstring& find_request,
    int forward, int match_case, int* active_ordinal, int* matches_found) {
  NOTREACHED() << "This function has been deprecated."
    << "Please use HandleFindRequest instead.";
  *matches_found = -1;
  return;
}

void AutomationProvider::HandleFindRequest(
    int handle,
    const AutomationMsg_Find_Params& params,
    IPC::Message* reply_message) {
  if (!tab_tracker_->ContainsHandle(handle)) {
    AutomationMsg_FindInPage::WriteReplyParams(reply_message, -1, -1);
    Send(reply_message);
    return;
  }

  NavigationController* nav = tab_tracker_->GetResource(handle);
  TabContents* tab_contents = nav->tab_contents();

  find_in_page_observer_.reset(new
      FindInPageNotificationObserver(this, tab_contents, reply_message));

  tab_contents->set_current_find_request_id(
      FindInPageNotificationObserver::kFindInPageRequestId);
  tab_contents->render_view_host()->StartFinding(
      FindInPageNotificationObserver::kFindInPageRequestId,
      params.search_string, params.forward, params.match_case,
      params.find_next);
}

void AutomationProvider::HandleOpenFindInPageRequest(
    const IPC::Message& message, int handle) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->FindInPage(false, false);
  }
}

void AutomationProvider::GetFindWindowVisibility(int handle, bool* visible) {
  gfx::Point position;
  *visible = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    FindBarTesting* find_bar =
        browser->find_bar()->find_bar()->GetFindBarTesting();
    find_bar->GetFindBarWindowInfo(&position, visible);
  }
}

void AutomationProvider::HandleFindWindowLocationRequest(int handle, int* x,
                                                         int* y) {
  gfx::Point position(0, 0);
  bool visible = false;
  if (browser_tracker_->ContainsHandle(handle)) {
     Browser* browser = browser_tracker_->GetResource(handle);
     FindBarTesting* find_bar =
       browser->find_bar()->find_bar()->GetFindBarTesting();
     find_bar->GetFindBarWindowInfo(&position, &visible);
  }

  *x = position.x();
  *y = position.y();
}

void AutomationProvider::GetBookmarkBarVisibility(int handle, bool* visible,
                                                  bool* animating) {
  *visible = false;
  *animating = false;

#if defined(OS_WIN)
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BrowserWindowTesting* testing =
          browser->window()->GetBrowserWindowTesting();
      BookmarkBarView* bookmark_bar = testing->GetBookmarkBarView();
      if (bookmark_bar) {
        *animating = bookmark_bar->IsAnimating();
        *visible = browser->window()->IsBookmarkBarVisible();
      }
    }
  }
#else
  // TODO(port): Enable when bookmarks ui is ported.
  NOTIMPLEMENTED();
#endif
}

void AutomationProvider::HandleInspectElementRequest(
    int handle, int x, int y, IPC::Message* reply_message) {
  TabContents* tab_contents = GetTabContentsForHandle(handle, NULL);
  if (tab_contents) {
    DCHECK(reply_message_ == NULL);
    reply_message_ = reply_message;

    DevToolsManager::GetInstance()->InspectElement(
        tab_contents->render_view_host(), x, y);
  } else {
    AutomationMsg_InspectElement::WriteReplyParams(reply_message, -1);
    Send(reply_message);
  }
}

void AutomationProvider::ReceivedInspectElementResponse(int num_resources) {
  if (reply_message_) {
    AutomationMsg_InspectElement::WriteReplyParams(reply_message_,
                                                   num_resources);
    Send(reply_message_);
    reply_message_ = NULL;
  }
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

class SetProxyConfigTask : public Task {
 public:
  explicit SetProxyConfigTask(net::ProxyService* proxy_service,
                              const std::string& new_proxy_config)
      : proxy_service_(proxy_service), proxy_config_(new_proxy_config) {}
  virtual void Run() {
    // First, deserialize the JSON string. If this fails, log and bail.
    JSONStringValueSerializer deserializer(proxy_config_);
    std::string error_message;
    scoped_ptr<Value> root(deserializer.Deserialize(&error_message));
    if (!root.get() || root->GetType() != Value::TYPE_DICTIONARY) {
      DLOG(WARNING) << "Received bad JSON string for ProxyConfig: "
                    << error_message;
      return;
    }

    scoped_ptr<DictionaryValue> dict(
        static_cast<DictionaryValue*>(root.release()));
    // Now put together a proxy configuration from the deserialized string.
    net::ProxyConfig pc;
    PopulateProxyConfig(*dict.get(), &pc);

    DCHECK(proxy_service_);
    scoped_ptr<net::ProxyConfigService> proxy_config_service(
        new net::ProxyConfigServiceFixed(pc));
    proxy_service_->ResetConfigService(proxy_config_service.release());
  }

  void PopulateProxyConfig(const DictionaryValue& dict, net::ProxyConfig* pc) {
    DCHECK(pc);
    bool no_proxy = false;
    if (dict.GetBoolean(automation::kJSONProxyNoProxy, &no_proxy)) {
      // Make no changes to the ProxyConfig.
      return;
    }
    bool auto_config;
    if (dict.GetBoolean(automation::kJSONProxyAutoconfig, &auto_config)) {
      pc->auto_detect = true;
    }
    std::string pac_url;
    if (dict.GetString(automation::kJSONProxyPacUrl, &pac_url)) {
      pc->pac_url = GURL(pac_url);
    }
    std::string proxy_bypass_list;
    if (dict.GetString(automation::kJSONProxyBypassList, &proxy_bypass_list)) {
      pc->ParseNoProxyList(proxy_bypass_list);
    }
    std::string proxy_server;
    if (dict.GetString(automation::kJSONProxyServer, &proxy_server)) {
      pc->proxy_rules.ParseFromString(proxy_server);
    }
  }

 private:
  net::ProxyService* proxy_service_;
  std::string proxy_config_;
};


void AutomationProvider::SetProxyConfig(const std::string& new_proxy_config) {
  URLRequestContext* context = Profile::GetDefaultRequestContext();
  // If we don't have a default request context yet then we have to create
  // one.
  bool run_on_ui_thread = false;
  if (!context) {
    FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    DCHECK(profile_manager);
    Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
    DCHECK(profile);
    context = profile->GetRequestContext();
    run_on_ui_thread = true;
  }
  DCHECK(context);
  // Every URLRequestContext should have a proxy service.
  net::ProxyService* proxy_service = context->proxy_service();
  DCHECK(proxy_service);

  // If we just now created the URLRequestContext then we can immediately
  // set the proxy settings on this (the UI) thread. If there was already
  // a URLRequestContext, then run the reset on the IO thread.
  if (run_on_ui_thread) {
    SetProxyConfigTask set_proxy_config_task(proxy_service, new_proxy_config);
    set_proxy_config_task.Run();
  } else {
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        new SetProxyConfigTask(proxy_service, new_proxy_config));
  }
}

void AutomationProvider::GetDownloadDirectory(
    int handle, std::wstring* download_directory) {
  DLOG(INFO) << "Handling download directory request";
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    DownloadManager* dlm = tab->profile()->GetDownloadManager();
    DCHECK(dlm);
    *download_directory = dlm->download_path().ToWStringHack();
  }
}

void AutomationProvider::OpenNewBrowserWindow(bool show,
                                              IPC::Message* reply_message) {
  new BrowserOpenedNotificationObserver(this, reply_message);
  // We may have no current browser windows open so don't rely on
  // asking an existing browser to execute the IDC_NEWWINDOW command
  Browser* browser = Browser::Create(profile_);
  browser->AddBlankTab(true);
  if (show)
    browser->window()->Show();
}

void AutomationProvider::GetWindowForBrowser(int browser_handle,
                                             bool* success,
                                             int* handle) {
  *success = false;
  *handle = 0;

  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    gfx::NativeWindow win = browser->window()->GetNativeHandle();
    // Add() returns the existing handle for the resource if any.
    *handle = window_tracker_->Add(win);
    *success = true;
  }
}

#if defined(OS_WIN) || defined(OS_LINUX)
void AutomationProvider::GetAutocompleteEditForBrowser(
    int browser_handle,
    bool* success,
    int* autocomplete_edit_handle) {
  *success = false;
  *autocomplete_edit_handle = 0;

  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    LocationBar* loc_bar = browser->window()->GetLocationBar();
    AutocompleteEditView* edit_view = loc_bar->location_entry();
    // Add() returns the existing handle for the resource if any.
    *autocomplete_edit_handle = autocomplete_edit_tracker_->Add(edit_view);
    *success = true;
  }
}
#endif  // defined(OS_WIN) || defined(OS_LINUX)

#if defined(OS_WIN)
// TODO(port): Remove windowsisms.
void AutomationProvider::GetBrowserForWindow(int window_handle,
                                             bool* success,
                                             int* browser_handle) {
  *success = false;
  *browser_handle = 0;

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
      *browser_handle = browser_tracker_->Add(browser);
      *success = true;
    }
  }
}
#endif  // defined(OS_WIN)

void AutomationProvider::ShowInterstitialPage(int tab_handle,
                                              const std::string& html_text,
                                              IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* controller = tab_tracker_->GetResource(tab_handle);
    TabContents* tab_contents = controller->tab_contents();

    AddNavigationStatusListener<bool>(controller, reply_message, true,
                                      false, false);
    AutomationInterstitialPage* interstitial =
        new AutomationInterstitialPage(tab_contents,
                                       GURL("about:interstitial"),
                                       html_text);
    interstitial->Show();
    return;
  }

  AutomationMsg_ShowInterstitialPage::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void AutomationProvider::HideInterstitialPage(int tab_handle,
                                              bool* success) {
  *success = false;
  TabContents* tab_contents = GetTabContentsForHandle(tab_handle, NULL);
  if (tab_contents && tab_contents->interstitial_page()) {
    tab_contents->interstitial_page()->DontProceed();
    *success = true;
  }
}

void AutomationProvider::CloseTab(int tab_handle,
                                  bool wait_until_closed,
                                  IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* controller = tab_tracker_->GetResource(tab_handle);
    int index;
    Browser* browser = Browser::GetBrowserForController(controller, &index);
    DCHECK(browser);
    new TabClosedNotificationObserver(this, wait_until_closed, reply_message);
    browser->CloseContents(controller->tab_contents());
    return;
  }

  AutomationMsg_CloseTab::WriteReplyParams(reply_message, false);
}

void AutomationProvider::CloseBrowser(int browser_handle,
                                      IPC::Message* reply_message) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    new BrowserClosedNotificationObserver(browser, this,
                                          reply_message);
    browser->window()->Close();
  } else {
    NOTREACHED();
  }
}

void AutomationProvider::CloseBrowserAsync(int browser_handle) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    browser->window()->Close();
  } else {
    NOTREACHED();
  }
}

#if defined(OS_WIN)
// TODO(port): Remove windowsisms.
void AutomationProvider::CreateExternalTab(HWND parent,
                                           const gfx::Rect& dimensions,
                                           unsigned int style,
                                           bool incognito,
                                           HWND* tab_container_window,
                                           HWND* tab_window,
                                           int* tab_handle) {
  *tab_handle = 0;
  *tab_container_window = NULL;
  *tab_window = NULL;
  ExternalTabContainer *external_tab_container =
      new ExternalTabContainer(this);
  Profile* profile = incognito? profile_->GetOffTheRecordProfile() : profile_;
  external_tab_container->Init(profile, parent, dimensions, style);
  TabContents* tab_contents = external_tab_container->tab_contents();
  if (tab_contents) {
    *tab_handle = tab_tracker_->Add(&tab_contents->controller());
    external_tab_container->set_tab_handle(*tab_handle);
    *tab_container_window = external_tab_container->GetNativeView();
    *tab_window = tab_contents->GetNativeView();
  } else {
    delete external_tab_container;
  }
}
#endif

void AutomationProvider::NavigateInExternalTab(
    int handle, const GURL& url,
    AutomationMsg_NavigationResponseValues* status) {
  *status = AUTOMATION_MSG_NAVIGATION_ERROR;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    tab->LoadURL(url, GURL(), PageTransition::TYPED);
    *status = AUTOMATION_MSG_NAVIGATION_SUCCESS;
  }
}

#if defined(OS_WIN)
// TODO(port): remove windowisms.
void AutomationProvider::SetAcceleratorsForTab(int handle,
                                               HACCEL accel_table,
                                               int accel_entry_count,
                                               bool* status) {
  *status = false;

  ExternalTabContainer* external_tab = GetExternalTabForHandle(handle);
  if (external_tab) {
    external_tab->SetAccelerators(accel_table, accel_entry_count);
    *status = true;
  }
}

void AutomationProvider::ProcessUnhandledAccelerator(
    const IPC::Message& message, int handle, const MSG& msg) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(handle);
  if (external_tab) {
    external_tab->ProcessUnhandledAccelerator(msg);
  }
  // This message expects no response.
}
#endif

void AutomationProvider::WaitForTabToBeRestored(int tab_handle,
                                                IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    restore_tracker_.reset(
        new NavigationControllerRestoredObserver(this, tab, reply_message));
  }
}

void AutomationProvider::SetInitialFocus(const IPC::Message& message,
                                         int handle, bool reverse) {
#if defined(OS_WIN)
  ExternalTabContainer* external_tab = GetExternalTabForHandle(handle);
  if (external_tab) {
    external_tab->FocusThroughTabTraversal(reverse);
  }
  // This message expects no response.
#elif defined(OS_POSIX)
  // TODO(port) enable this function.
  NOTIMPLEMENTED();
#endif
}

// TODO(port): enable these functions.
#if defined(OS_WIN)
void AutomationProvider::GetSecurityState(int handle, bool* success,
                                          SecurityStyle* security_style,
                                          int* ssl_cert_status,
                                          int* mixed_content_status) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    *success = true;
    *security_style = entry->ssl().security_style();
    *ssl_cert_status = entry->ssl().cert_status();
    *mixed_content_status = entry->ssl().content_status();
  } else {
    *success = false;
    *security_style = SECURITY_STYLE_UNKNOWN;
    *ssl_cert_status = 0;
    *mixed_content_status = 0;
  }
}

void AutomationProvider::GetPageType(int handle, bool* success,
                                     NavigationEntry::PageType* page_type) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    *page_type = entry->page_type();
    *success = true;
    // In order to return the proper result when an interstitial is shown and
    // no navigation entry were created for it we need to ask the TabContents.
    if (*page_type == NavigationEntry::NORMAL_PAGE &&
        tab->tab_contents()->showing_interstitial_page())
      *page_type = NavigationEntry::INTERSTITIAL_PAGE;
  } else {
    *success = false;
    *page_type = NavigationEntry::NORMAL_PAGE;
  }
}

void AutomationProvider::ActionOnSSLBlockingPage(int handle, bool proceed,
                                                 IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    if (entry->page_type() == NavigationEntry::INTERSTITIAL_PAGE) {
      TabContents* tab_contents = tab->tab_contents();
      InterstitialPage* ssl_blocking_page =
          InterstitialPage::GetInterstitialPage(tab_contents);
      if (ssl_blocking_page) {
        if (proceed) {
          AddNavigationStatusListener<bool>(tab, reply_message, true, true,
                                            false);
          ssl_blocking_page->Proceed();
          return;
        }
        ssl_blocking_page->DontProceed();
        AutomationMsg_ActionOnSSLBlockingPage::WriteReplyParams(reply_message,
                                                                true);
        Send(reply_message);
        return;
      }
    }
  }
  // We failed.
  AutomationMsg_ActionOnSSLBlockingPage::WriteReplyParams(reply_message,
                                                          false);
  Send(reply_message);
}
#endif  // defined(OS_WIN)

void AutomationProvider::BringBrowserToFront(int browser_handle,
                                             bool* success) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    browser->window()->Activate();
    *success = true;
  } else {
    *success = false;
  }
}

void AutomationProvider::IsPageMenuCommandEnabled(int browser_handle,
                                                  int message_num,
                                                  bool* menu_item_enabled) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    *menu_item_enabled =
        browser->command_updater()->IsCommandEnabled(message_num);
  } else {
    *menu_item_enabled = false;
  }
}

#if defined(OS_WIN)
// TODO(port): Enable this.
void AutomationProvider::PrintNow(int tab_handle,
                                  IPC::Message* reply_message) {
  NavigationController* tab = NULL;
  TabContents* tab_contents = GetTabContentsForHandle(tab_handle, &tab);
  if (tab_contents) {
    FindAndActivateTab(tab);
    notification_observer_list_.AddObserver(
        new DocumentPrintedNotificationObserver(this, reply_message));
    if (tab_contents->PrintNow())
      return;
  }
  AutomationMsg_PrintNow::WriteReplyParams(reply_message, false);
  Send(reply_message);
}
#endif

void AutomationProvider::SavePage(int tab_handle,
                                  const std::wstring& file_name,
                                  const std::wstring& dir_path,
                                  int type,
                                  bool* success) {
  if (!tab_tracker_->ContainsHandle(tab_handle)) {
    *success = false;
    return;
  }

  NavigationController* nav = tab_tracker_->GetResource(tab_handle);
  Browser* browser = FindAndActivateTab(nav);
  DCHECK(browser);
  if (!browser->command_updater()->IsCommandEnabled(IDC_SAVE_PAGE)) {
    *success = false;
    return;
  }

  SavePackage::SavePackageType save_type =
      static_cast<SavePackage::SavePackageType>(type);
  DCHECK(save_type >= SavePackage::SAVE_AS_ONLY_HTML &&
         save_type <= SavePackage::SAVE_AS_COMPLETE_HTML);
  nav->tab_contents()->SavePage(file_name, dir_path, save_type);

  *success = true;
}

#if defined(OS_WIN) || defined(OS_LINUX)
// TODO(port): Enable these.
void AutomationProvider::GetAutocompleteEditText(int autocomplete_edit_handle,
                                                 bool* success,
                                                 std::wstring* text) {
  *success = false;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    *text = autocomplete_edit_tracker_->GetResource(autocomplete_edit_handle)->
        GetText();
    *success = true;
  }
}

void AutomationProvider::SetAutocompleteEditText(int autocomplete_edit_handle,
                                                 const std::wstring& text,
                                                 bool* success) {
  *success = false;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    autocomplete_edit_tracker_->GetResource(autocomplete_edit_handle)->
        SetUserText(text);
    *success = true;
  }
}

void AutomationProvider::AutocompleteEditGetMatches(
    int autocomplete_edit_handle,
    bool* success,
    std::vector<AutocompleteMatchData>* matches) {
  *success = false;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    const AutocompleteResult& result = autocomplete_edit_tracker_->
        GetResource(autocomplete_edit_handle)->model()->result();
    for (AutocompleteResult::const_iterator i = result.begin();
        i != result.end(); ++i)
      matches->push_back(AutocompleteMatchData(*i));
    *success = true;
  }
}

void AutomationProvider::AutocompleteEditIsQueryInProgress(
    int autocomplete_edit_handle,
    bool* success,
    bool* query_in_progress) {
  *success = false;
  *query_in_progress = false;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    *query_in_progress = autocomplete_edit_tracker_->
        GetResource(autocomplete_edit_handle)->model()->query_in_progress();
    *success = true;
  }
}

void AutomationProvider::OnMessageFromExternalHost(int handle,
                                                   const std::string& message,
                                                   const std::string& origin,
                                                   const std::string& target) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    if (!tab) {
      NOTREACHED();
      return;
    }

    TabContents* tab_contents = tab->tab_contents();
    if (!tab_contents) {
      NOTREACHED();
      return;
    }

    RenderViewHost* view_host = tab_contents->render_view_host();
    if (!view_host) {
      return;
    }

    if (AutomationExtensionFunction::InterceptMessageFromExternalHost(
        view_host, message, origin, target)) {
      // Message was diverted.
      return;
    }

    if (ExtensionPortContainer::InterceptMessageFromExternalHost(message,
        origin, target, this, view_host, handle)) {
      // Message was diverted.
      return;
    }

    if (InterceptBrowserEventMessageFromExternalHost(message, origin, target)) {
      // Message was diverted.
      return;
    }

    view_host->ForwardMessageFromExternalHost(message, origin, target);
  }
}

bool AutomationProvider::InterceptBrowserEventMessageFromExternalHost(
      const std::string& message, const std::string& origin,
      const std::string& target) {
  if (target !=
      extension_automation_constants::kAutomationBrowserEventRequestTarget)
    return false;

  if (origin != extension_automation_constants::kAutomationOrigin) {
    LOG(WARNING) << "Wrong origin on automation browser event " << origin;
    return false;
  }

  // The message is a JSON-encoded array with two elements, both strings. The
  // first is the name of the event to dispatch.  The second is a JSON-encoding
  // of the arguments specific to that event.
  scoped_ptr<Value> message_value(JSONReader::Read(message, false));
  if (!message_value.get() || !message_value->IsType(Value::TYPE_LIST)) {
    LOG(WARNING) << "Invalid browser event specified through automation";
    return false;
  }

  const ListValue* args = static_cast<const ListValue*>(message_value.get());

  std::string event_name;
  if (!args->GetString(0, &event_name)) {
    LOG(WARNING) << "No browser event name specified through automation";
    return false;
  }

  std::string json_args;
  if (!args->GetString(1, &json_args)) {
    LOG(WARNING) << "No browser event args specified through automation";
    return false;
  }

  ExtensionMessageService::GetInstance(profile()->GetRequestContext())->
      DispatchEventToRenderers(event_name.c_str(), json_args);

  return true;
}
#endif  // defined(OS_WIN) || defined(OS_LINUX)

TabContents* AutomationProvider::GetTabContentsForHandle(
    int handle, NavigationController** tab) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (tab)
      *tab = nav_controller;
    return nav_controller->tab_contents();
  }
  return NULL;
}

#if defined(OS_WIN)
ExternalTabContainer* AutomationProvider::GetExternalTabForHandle(int handle) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    return ExternalTabContainer::GetContainerForTab(
        tab->tab_contents()->GetNativeView());
  }

  return NULL;
}
#endif  // defined(OS_WIN)

TestingAutomationProvider::TestingAutomationProvider(Profile* profile)
    : AutomationProvider(profile) {
  BrowserList::AddObserver(this);
  registrar_.Add(this, NotificationType::SESSION_END,
                 NotificationService::AllSources());
}

TestingAutomationProvider::~TestingAutomationProvider() {
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
    // If you change this, update Observer for NotificationType::SESSION_END
    // below.
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &TestingAutomationProvider::OnRemoveProvider));
  }
}

void TestingAutomationProvider::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(type == NotificationType::SESSION_END);
  // OnBrowserRemoving does a ReleaseLater. When session end is received we exit
  // before the task runs resulting in this object not being deleted. This
  // Release balance out the Release scheduled by OnBrowserRemoving.
  Release();
}

void TestingAutomationProvider::OnRemoveProvider() {
  AutomationProviderList::GetInstance()->RemoveProvider(this);
}

void AutomationProvider::GetSSLInfoBarCount(int handle, int* count) {
  *count = -1;  // -1 means error.
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (nav_controller)
      *count = nav_controller->tab_contents()->infobar_delegate_count();
  }
}

void AutomationProvider::ClickSSLInfoBarLink(int handle,
                                             int info_bar_index,
                                             bool wait_for_navigation,
                                             IPC::Message* reply_message) {
  bool success = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (nav_controller) {
      int count = nav_controller->tab_contents()->infobar_delegate_count();
      if (info_bar_index >= 0 && info_bar_index < count) {
        if (wait_for_navigation) {
          AddNavigationStatusListener<bool>(nav_controller, reply_message,
                                            true, true, false);
        }
        InfoBarDelegate* delegate =
            nav_controller->tab_contents()->GetInfoBarDelegateAt(
                info_bar_index);
        if (delegate->AsConfirmInfoBarDelegate())
          delegate->AsConfirmInfoBarDelegate()->Accept();
        success = true;
      }
    }
  }
  if (!wait_for_navigation || !success)
    AutomationMsg_ClickSSLInfoBarLink::WriteReplyParams(reply_message,
                                                        success);
}

void AutomationProvider::GetLastNavigationTime(int handle,
                                               int64* last_navigation_time) {
  Time time = tab_tracker_->GetLastNavigationTime(handle);
  *last_navigation_time = time.ToInternalValue();
}

void AutomationProvider::WaitForNavigation(int handle,
                                           int64 last_navigation_time,
                                           IPC::Message* reply_message) {
  NavigationController* controller = NULL;
  if (tab_tracker_->ContainsHandle(handle))
    controller = tab_tracker_->GetResource(handle);

  Time time = tab_tracker_->GetLastNavigationTime(handle);
  if (time.ToInternalValue() > last_navigation_time || !controller) {
    AutomationMsg_WaitForNavigation::WriteReplyParams(reply_message,
                                                      controller != NULL);
    return;
  }

  AddNavigationStatusListener<bool>(controller, reply_message, true, true,
                                    false);
}

void AutomationProvider::SetIntPreference(int handle,
                                          const std::wstring& name,
                                          int value,
                                          bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetInteger(name.c_str(), value);
    *success = true;
  }
}

void AutomationProvider::SetStringPreference(int handle,
                                             const std::wstring& name,
                                             const std::wstring& value,
                                             bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetString(name.c_str(), value);
    *success = true;
  }
}

void AutomationProvider::GetBooleanPreference(int handle,
                                              const std::wstring& name,
                                              bool* success,
                                              bool* value) {
  *success = false;
  *value = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    *value = browser->profile()->GetPrefs()->GetBoolean(name.c_str());
    *success = true;
  }
}

void AutomationProvider::SetBooleanPreference(int handle,
                                              const std::wstring& name,
                                              bool value,
                                              bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetBoolean(name.c_str(), value);
    *success = true;
  }
}

// Gets the current used encoding name of the page in the specified tab.
void AutomationProvider::GetPageCurrentEncoding(
    int tab_handle, std::wstring* current_encoding) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* nav = tab_tracker_->GetResource(tab_handle);
    Browser* browser = FindAndActivateTab(nav);
    DCHECK(browser);

    if (browser->command_updater()->IsCommandEnabled(IDC_ENCODING_MENU))
      *current_encoding = nav->tab_contents()->encoding();
  }
}

// Gets the current used encoding name of the page in the specified tab.
void AutomationProvider::OverrideEncoding(int tab_handle,
                                          const std::wstring& encoding_name,
                                          bool* success) {
  *success = false;
#if defined(OS_WIN)
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* nav = tab_tracker_->GetResource(tab_handle);
    Browser* browser = FindAndActivateTab(nav);
    DCHECK(browser);

    if (browser->command_updater()->IsCommandEnabled(IDC_ENCODING_MENU)) {
      TabContents* tab_contents = nav->tab_contents();
      int selected_encoding_id =
          CharacterEncoding::GetCommandIdByCanonicalEncodingName(encoding_name);
      if (selected_encoding_id) {
        browser->OverrideEncoding(selected_encoding_id);
        *success = true;
      }
    }
  }
#else
  // TODO(port): Enable when encoding-related parts of Browser are ported.
  NOTIMPLEMENTED();
#endif
}

void AutomationProvider::SavePackageShouldPromptUser(bool should_prompt) {
  SavePackage::SetShouldPromptUser(should_prompt);
}

void AutomationProvider::SetEnableExtensionAutomation(bool automation_enabled) {
  AutomationExtensionFunction::SetEnabled(automation_enabled);
}

#if defined(OS_WIN)
// TODO(port): Reposition_Params is win-specific. We'll need to port it.
void AutomationProvider::OnTabReposition(
    int tab_handle, const IPC::Reposition_Params& params) {
  if (!tab_tracker_->ContainsHandle(tab_handle))
    return;

  if (!IsWindow(params.window))
    return;

  unsigned long process_id = 0;
  unsigned long thread_id = 0;

  thread_id = GetWindowThreadProcessId(params.window, &process_id);

  if (thread_id != GetCurrentThreadId()) {
    NOTREACHED();
    return;
  }

  SetWindowPos(params.window, params.window_insert_after, params.left,
               params.top, params.width, params.height, params.flags);

  if (params.set_parent) {
    if (IsWindow(params.parent_window)) {
      if (!SetParent(params.window, params.parent_window))
        DLOG(WARNING) << "SetParent failed. Error 0x%x" << GetLastError();
    }
  }
}

void AutomationProvider::OnForwardContextMenuCommandToChrome(int tab_handle,
                                                             int command) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    if (!tab) {
      NOTREACHED();
      return;
    }

    TabContents* tab_contents = tab->tab_contents();
    if (!tab_contents || !tab_contents->delegate()) {
      NOTREACHED();
      return;
    }

    tab_contents->delegate()->ExecuteContextMenuCommand(command);
  }
}

#endif  // defined(OS_WIN)

void AutomationProvider::GetWindowTitle(int handle, string16* text) {
  gfx::NativeWindow window = window_tracker_->GetResource(handle);
  text->assign(platform_util::GetWindowTitle(window));
}

void AutomationProvider::GetBlockedPopupCount(int handle, int* count) {
  *count = -1;  // -1 is the error code
  if (tab_tracker_->ContainsHandle(handle)) {
      NavigationController* nav_controller = tab_tracker_->GetResource(handle);
      TabContents* tab_contents = nav_controller->tab_contents();
      if (tab_contents) {
        BlockedPopupContainer* container =
            tab_contents->blocked_popup_container();
        if (container) {
          *count = static_cast<int>(container->GetBlockedPopupCount());
        } else {
          // If we don't have a container, we don't have any blocked popups to
          // contain!
          *count = 0;
        }
      }
  }
}
