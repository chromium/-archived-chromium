// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "chrome/test/automation/automation_proxy.h"

#include "base/basictypes.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/ref_counted.h"
#include "base/waitable_event.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#if defined(OS_WIN)
// TODO(port): Enable when dialog_delegate is ported.
#include "views/window/dialog_delegate.h"
#endif

using base::TimeDelta;
using base::TimeTicks;

namespace {

// This object allows messages received on the background thread to be
// properly triaged.
class AutomationMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  AutomationMessageFilter(AutomationProxy* server) : server_(server) {}

  // Return true to indicate that the message was handled, or false to let
  // the message be handled in the default way.
  virtual bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;

    IPC_BEGIN_MESSAGE_MAP(AutomationMessageFilter, message)
      IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_Hello,
                                  OnAutomationHello(message));
      IPC_MESSAGE_HANDLER_GENERIC(
        AutomationMsg_InitialLoadsComplete, server_->SignalInitialLoads());
      IPC_MESSAGE_HANDLER(AutomationMsg_InitialNewTabUILoadComplete,
                          NewTabLoaded);
      IPC_MESSAGE_HANDLER_GENERIC(
        AutomationMsg_InvalidateHandle, server_->InvalidateHandle(message));
      IPC_MESSAGE_UNHANDLED(handled = false);
    IPC_END_MESSAGE_MAP()

    return handled;
  }

  void NewTabLoaded(int load_time) {
    server_->SignalNewTabUITab(load_time);
  }

  void OnAutomationHello(const IPC::Message& hello_message) {
    std::string server_version;
    void* iter = NULL;
    if (!hello_message.ReadString(&iter, &server_version)) {
      // We got an AutomationMsg_Hello from an old automation provider
      // that doesn't send version info. Leave server_version as an empty
      // string to signal a version mismatch.
      LOG(ERROR) << "Pre-versioning protocol detected in automation provider.";
    }

    server_->SignalAppLaunch(server_version);
  }

 private:
  AutomationProxy* server_;
};

}  // anonymous namespace


AutomationProxy::AutomationProxy(int command_execution_timeout_ms)
    : app_launched_(true, false),
      initial_loads_complete_(true, false),
      new_tab_ui_load_complete_(true, false),
      shutdown_event_(new base::WaitableEvent(true, false)),
      app_launch_signaled_(0),
      perform_version_check_(false),
      command_execution_timeout_(
          TimeDelta::FromMilliseconds(command_execution_timeout_ms)) {
  InitializeChannelID();
  InitializeHandleTracker();
  InitializeThread();
  InitializeChannel();
}

AutomationProxy::~AutomationProxy() {
  DCHECK(shutdown_event_.get() != NULL);
  shutdown_event_->Signal();
  // Destruction order is important. Thread has to outlive the channel and
  // tracker has to outlive the thread since we access the tracker inside
  // AutomationMessageFilter::OnMessageReceived.
  channel_.reset();
  thread_.reset();
  tracker_.reset();
}

void AutomationProxy::InitializeChannelID() {
  // The channel counter keeps us out of trouble if we create and destroy
  // several AutomationProxies sequentially over the course of a test run.
  // (Creating the channel sometimes failed before when running a lot of
  // tests in sequence, and our theory is that sometimes the channel ID
  // wasn't getting freed up in time for the next test.)
  static int channel_counter = 0;

  std::ostringstream buf;
  buf << "ChromeTestingInterface:" << base::GetCurrentProcId() <<
         "." << ++channel_counter;
  channel_id_ = buf.str();
}

void AutomationProxy::InitializeThread() {
  scoped_ptr<base::Thread> thread(
      new base::Thread("AutomationProxy_BackgroundThread"));
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  bool thread_result = thread->StartWithOptions(options);
  DCHECK(thread_result);
  thread_.swap(thread);
}

void AutomationProxy::InitializeChannel() {
  DCHECK(shutdown_event_.get() != NULL);

  // TODO(iyengar)
  // The shutdown event could be global on the same lines as the automation
  // provider, where we use the shutdown event provided by the chrome browser
  // process.
  channel_.reset(new IPC::SyncChannel(
    channel_id_,
    IPC::Channel::MODE_SERVER,
    this,  // we are the listener
    new AutomationMessageFilter(this),
    thread_->message_loop(),
    true,
    shutdown_event_.get()));
}

void AutomationProxy::InitializeHandleTracker() {
  tracker_.reset(new AutomationHandleTracker(this));
}

AutomationLaunchResult AutomationProxy::WaitForAppLaunch() {
  AutomationLaunchResult result = AUTOMATION_SUCCESS;
  if (app_launched_.TimedWait(command_execution_timeout_)) {
    if (perform_version_check_) {
      // Obtain our own version number and compare it to what the automation
      // provider sent.
      scoped_ptr<FileVersionInfo> file_version_info(
          FileVersionInfo::CreateFileVersionInfoForCurrentModule());
      DCHECK(file_version_info != NULL);
      std::string version_string(
          WideToASCII(file_version_info->file_version()));

      // Note that we use a simple string comparison since we expect the version
      // to be a punctuated numeric string. Consider using base/Version if we
      // ever need something more complicated here.
      if (server_version_ != version_string) {
        result = AUTOMATION_VERSION_MISMATCH;
      }
    }
  } else {
    result = AUTOMATION_TIMEOUT;
  }
  return result;
}

void AutomationProxy::SignalAppLaunch(const std::string& version_string) {
  // The synchronization of the reading / writing of server_version_ is a bit
  // messy but does work as long as SignalAppLaunch is only called once.
  // Review this if we ever want an AutomationProxy instance to launch
  // multiple AutomationProviders.
  app_launch_signaled_++;
  if (app_launch_signaled_ > 1) {
    NOTREACHED();
    LOG(ERROR) << "Multiple AutomationMsg_Hello messages received";
    return;
  }
  server_version_ = version_string;
  app_launched_.Signal();
}

bool AutomationProxy::WaitForInitialLoads() {
  return initial_loads_complete_.TimedWait(command_execution_timeout_);
}

bool AutomationProxy::WaitForInitialNewTabUILoad(int* load_time) {
  if (new_tab_ui_load_complete_.TimedWait(command_execution_timeout_)) {
    *load_time = new_tab_ui_load_time_;
    new_tab_ui_load_complete_.Reset();
    return true;
  }
  return false;
}

void AutomationProxy::SignalInitialLoads() {
  initial_loads_complete_.Signal();
}

void AutomationProxy::SignalNewTabUITab(int load_time) {
  new_tab_ui_load_time_ = load_time;
  new_tab_ui_load_complete_.Signal();
}

bool AutomationProxy::SavePackageShouldPromptUser(bool should_prompt) {
  return Send(new AutomationMsg_SavePackageShouldPromptUser(0, should_prompt));
}

bool AutomationProxy::SetEnableExtensionAutomation(bool enable_automation) {
  return Send(
      new AutomationMsg_SetEnableExtensionAutomation(0, enable_automation));
}

bool AutomationProxy::GetBrowserWindowCount(int* num_windows) {
  if (!num_windows) {
    NOTREACHED();
    return false;
  }

  bool succeeded = SendWithTimeout(
      new AutomationMsg_BrowserWindowCount(0, num_windows),
      command_execution_timeout_ms(), NULL);

  if (!succeeded) {
    DLOG(ERROR) << "GetWindowCount did not complete in a timely fashion";
    return false;
  }

  return succeeded;
}

bool AutomationProxy::GetNormalBrowserWindowCount(int* num_windows) {
  if (!num_windows) {
    NOTREACHED();
    return false;
  }

  bool succeeded = SendWithTimeout(
      new AutomationMsg_NormalBrowserWindowCount(0, num_windows),
      command_execution_timeout_ms(), NULL);

  if (!succeeded) {
    DLOG(ERROR) << "GetNormalWindowCount did not complete in a timely fashion";
    return false;
  }

  return succeeded;
}

bool AutomationProxy::WaitForWindowCountToBecome(int count,
                                                 int wait_timeout) {
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout);
  while (TimeTicks::Now() - start < timeout) {
    int new_count;
    bool succeeded = GetBrowserWindowCount(&new_count);
    if (!succeeded) {
      // Try again next round, but log it.
      DLOG(ERROR) << "GetBrowserWindowCount returned false";
    } else if (count == new_count) {
      return true;
    }
    PlatformThread::Sleep(automation::kSleepTime);
  }
  // Window count never reached the value we sought.
  return false;
}

bool AutomationProxy::GetShowingAppModalDialog(
    bool* showing_app_modal_dialog,
    MessageBoxFlags::DialogButton* button) {
  if (!showing_app_modal_dialog || !button) {
    NOTREACHED();
    return false;
  }

  int button_int = 0;

  if (!SendWithTimeout(
          new AutomationMsg_ShowingAppModalDialog(
              0, showing_app_modal_dialog, &button_int),
          command_execution_timeout_ms(), NULL)) {
    DLOG(ERROR) << "ShowingAppModalDialog did not complete in a timely fashion";
    return false;
  }

  *button = static_cast<MessageBoxFlags::DialogButton>(button_int);
  return true;
}

bool AutomationProxy::ClickAppModalDialogButton(
    MessageBoxFlags::DialogButton button) {
  bool succeeded = false;

  if (!SendWithTimeout(
          new AutomationMsg_ClickAppModalDialogButton(
              0, button, &succeeded),
          command_execution_timeout_ms(), NULL)) {
    return false;
  }

  return succeeded;
}

bool AutomationProxy::WaitForAppModalDialog(int wait_timeout) {
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout);
  while (TimeTicks::Now() - start < timeout) {
    bool dialog_shown = false;
    MessageBoxFlags::DialogButton button = MessageBoxFlags::DIALOGBUTTON_NONE;
    bool succeeded = GetShowingAppModalDialog(&dialog_shown, &button);
    if (!succeeded) {
      // Try again next round, but log it.
      DLOG(ERROR) << "GetShowingAppModalDialog returned false";
    } else if (dialog_shown) {
      return true;
    }
    PlatformThread::Sleep(automation::kSleepTime);
  }
  // Dialog never shown.
  return false;
}

bool AutomationProxy::WaitForURLDisplayed(GURL url, int wait_timeout) {
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout);
  while (TimeTicks::Now() - start < timeout) {
    int window_count;
    if (!GetBrowserWindowCount(&window_count))
      return false;

    for (int i = 0; i < window_count; i++) {
      scoped_refptr<BrowserProxy> window = GetBrowserWindow(i);
      if (!window.get())
        break;

      int tab_count;
      if (!window->GetTabCount(&tab_count))
        continue;

      for (int j = 0; j < tab_count; j++) {
        scoped_refptr<TabProxy> tab = window->GetTab(j);
        if (!tab.get())
          break;

        GURL tab_url;
        if (!tab->GetCurrentURL(&tab_url))
          continue;

        if (tab_url == url)
          return true;
      }
    }
    PlatformThread::Sleep(automation::kSleepTime);
  }

  return false;
}

bool AutomationProxy::SetFilteredInet(bool enabled) {
  return Send(new AutomationMsg_SetFilteredInet(0, enabled));
}

bool AutomationProxy::SendProxyConfig(const std::string& new_proxy_config) {
  return Send(new AutomationMsg_SetProxyConfig(0, new_proxy_config));
}

void AutomationProxy::Disconnect() {
  channel_.reset();
}

void AutomationProxy::OnMessageReceived(const IPC::Message& msg) {
  // This won't get called unless AutomationProxy is run from
  // inside a message loop.
  NOTREACHED();
}

void AutomationProxy::OnChannelError() {
  DLOG(ERROR) << "Channel error in AutomationProxy.";
}

scoped_refptr<WindowProxy> AutomationProxy::GetActiveWindow() {
  int handle = 0;

  if (!SendWithTimeout(new AutomationMsg_ActiveWindow(0, &handle),
                       command_execution_timeout_ms(), NULL)) {
    return NULL;
  }

  return ProxyObjectFromHandle<WindowProxy>(handle);
}

scoped_refptr<BrowserProxy> AutomationProxy::GetBrowserWindow(
    int window_index) {
  int handle = 0;

  if (!SendWithTimeout(new AutomationMsg_BrowserWindow(0, window_index,
                                                       &handle),
                       command_execution_timeout_ms(), NULL)) {
    DLOG(ERROR) << "GetBrowserWindow did not complete in a timely fashion";
    return NULL;
  }

  return ProxyObjectFromHandle<BrowserProxy>(handle);
}

bool AutomationProxy::GetBrowserLocale(string16* locale) {
  DCHECK(locale != NULL);
  if (!SendWithTimeout(new AutomationMsg_GetBrowserLocale(0, locale),
                       command_execution_timeout_ms(), NULL)) {
    DLOG(ERROR) << "GetBrowserLocale did not complete in a timely fashion";
    return false;
  }

  // An empty locale means that the browser has no UI language
  // which is impossible.
  DCHECK(!locale->empty());
  return !locale->empty();
}

scoped_refptr<BrowserProxy> AutomationProxy::FindNormalBrowserWindow() {
  int handle = 0;

  if (!SendWithTimeout(new AutomationMsg_FindNormalBrowserWindow(0, &handle),
                       command_execution_timeout_ms(), NULL)) {
    return NULL;
  }

  return ProxyObjectFromHandle<BrowserProxy>(handle);
}

scoped_refptr<BrowserProxy> AutomationProxy::GetLastActiveBrowserWindow() {
  int handle = 0;

  if (!SendWithTimeout(new AutomationMsg_LastActiveBrowserWindow(
      0, &handle), command_execution_timeout_ms(), NULL)) {
    DLOG(ERROR) <<
        "GetLastActiveBrowserWindow did not complete in a timely fashion";
    return NULL;
  }

  return ProxyObjectFromHandle<BrowserProxy>(handle);
}

#if defined(OS_POSIX)
base::file_handle_mapping_vector AutomationProxy::fds_to_map() const {
  base::file_handle_mapping_vector map;
  const int ipcfd = channel_->GetClientFileDescriptor();
  if (ipcfd > -1)
    map.push_back(std::make_pair(ipcfd, kPrimaryIPCChannel + 3));
  return map;
}
#endif  // defined(OS_POSIX)

bool AutomationProxy::Send(IPC::Message* message) {
  return SendWithTimeout(message, base::kNoTimeout, NULL);
}

bool AutomationProxy::SendWithTimeout(IPC::Message* message, int timeout,
                                      bool* is_timeout) {
  if (is_timeout)
    *is_timeout = false;

  if (channel_.get()) {
    bool result = channel_->SendWithTimeout(message, timeout);
    if (!result && is_timeout)
      *is_timeout = true;
    return result;
  }

  DLOG(WARNING) << "Channel has been closed; dropping message!";
  delete message;
  return false;
}

void AutomationProxy::InvalidateHandle(const IPC::Message& message) {
  void* iter = NULL;
  int handle;

  if (message.ReadInt(&iter, &handle)) {
    tracker_->InvalidateHandle(handle);
  }
}

bool AutomationProxy::OpenNewBrowserWindow(bool show) {
  return Send(new AutomationMsg_OpenNewBrowserWindow(0, show));
}

#if defined(OS_WIN)
// TODO(port): Replace HWNDs.
scoped_refptr<TabProxy> AutomationProxy::CreateExternalTab(HWND parent,
    const gfx::Rect& dimensions, unsigned int style, bool incognito,
    HWND* external_tab_container, HWND* tab) {
  IPC::Message* response = NULL;
  int handle = 0;

  bool succeeded =
      Send(new AutomationMsg_CreateExternalTab(0, parent, dimensions, style,
                                               incognito,
                                               external_tab_container,
                                               tab,
                                               &handle));
  if (!succeeded) {
    return NULL;
  }

  DCHECK(IsWindow(*external_tab_container));
  DCHECK(tracker_->GetResource(handle) == NULL);
  return new TabProxy(this, tracker_.get(), handle);
}
#endif  // defined(OS_WIN)

template <class T> scoped_refptr<T> AutomationProxy::ProxyObjectFromHandle(
    int handle) {
  if (!handle)
    return NULL;

  // Get AddRef-ed pointer to the object if handle is already seen.
  T* p = static_cast<T*>(tracker_->GetResource(handle));
  if (!p) {
    p = new T(this, tracker_.get(), handle);
    p->AddRef();
  }

  // Since there is no scoped_refptr::attach.
  scoped_refptr<T> result;
  result.swap(&p);
  return result;
}
