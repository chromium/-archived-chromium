// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_H__
#define CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_H__

#include <string>

#include "app/message_box_flags.h"
#include "base/basictypes.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_sync_channel.h"
#include "chrome/test/automation/automation_handle_tracker.h"
#include "chrome/test/automation/automation_messages.h"

class BrowserProxy;
class TabProxy;
class WindowProxy;

// This is an interface that AutomationProxy-related objects can use to
// access the message-sending abilities of the Proxy.
class AutomationMessageSender : public IPC::Message::Sender {
 public:
  // Sends a message synchronously; it doesn't return until a response has been
  // received or a timeout has expired.
  //
  // Use base::kNoTimeout for no timeout.
  //
  // The function returns true if a response is received, and returns false if
  // there is a failure or timeout (in milliseconds). If return after timeout,
  // is_timeout is set to true.
  // NOTE: When timeout occurs, the connection between proxy provider may be
  //       in transit state. Specifically, there might be pending IPC messages,
  //       and the proxy provider might be still working on the previous
  //       request.
  virtual bool SendWithTimeout(IPC::Message* message, int timeout,
                               bool* is_timeout) = 0;
};

// This is the interface that external processes can use to interact with
// a running instance of the app.
class AutomationProxy : public IPC::Channel::Listener,
                        public AutomationMessageSender {
 public:
  explicit AutomationProxy(int command_execution_timeout_ms);
  virtual ~AutomationProxy();

  // IPC callback
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  // Close the automation IPC channel.
  void Disconnect();

  // Waits for the app to launch and the automation provider to say hello
  // (the app isn't fully done loading by this point).
  // Returns SUCCESS if the launch is successful.
  // Returns TIMEOUT if there was no response by command_execution_timeout_
  // Returns VERSION_MISMATCH if the automation protocol version of the
  // automation provider does not match and if perform_version_check_ is set
  // to true. Note that perform_version_check_ defaults to false, call
  // set_perform_version_check() to set it.
  AutomationLaunchResult WaitForAppLaunch();

  // Waits for any initial page loads to complete.
  // NOTE: this only fires once for a run of the application.
  // Returns true if the load is successful
  bool WaitForInitialLoads();

  // Waits for the initial destinations tab to report that it has finished
  // querying.  |load_time| is filled in with how long it took, in milliseconds.
  // NOTE: this only fires once for a run of the application.
  // Returns true if the load is successful.
  bool WaitForInitialNewTabUILoad(int* load_time);

  // Open a new browser window, returning true on success. |show|
  // identifies whether the window should be shown.
  // False likely indicates an IPC error.
  bool OpenNewBrowserWindow(bool show);

  // Fills the number of open browser windows into the given variable, returning
  // true on success. False likely indicates an IPC error.
  bool GetBrowserWindowCount(int* num_windows);

  // Block the thread until the window count becomes the provided value.
  // Returns true on success.
  bool WaitForWindowCountToBecome(int target_count, int wait_timeout);

  // Fills the number of open normal browser windows (normal type and
  // non-incognito mode) into the given variable, returning true on success.
  // False likely indicates an IPC error.
  bool GetNormalBrowserWindowCount(int* num_windows);

  // Gets the locale of the chrome browser, currently all browsers forked from
  // the main chrome share the same UI locale, returning true on success.
  // False likely indicates an IPC error.
  bool GetBrowserLocale(string16* locale);

  // Returns whether an app modal dialog window is showing right now (i.e., a
  // javascript alert), and what buttons it contains.
  bool GetShowingAppModalDialog(bool* showing_app_modal_dialog,
                                MessageBoxFlags::DialogButton* button);

  // Simulates a click on a dialog button.
  bool ClickAppModalDialogButton(MessageBoxFlags::DialogButton button);

  // Block the thread until a modal dialog is displayed. Returns true on
  // success.
  bool WaitForAppModalDialog(int wait_timeout);

  // Block the thread until one of the tabs in any window (including windows
  // opened after the call) displays given url. Returns true on success.
  bool WaitForURLDisplayed(GURL url, int wait_timeout);

  // Returns the BrowserProxy for the browser window at the given index,
  // transferring ownership of the pointer to the caller.
  // On failure, returns NULL.
  //
  // Use GetBrowserWindowCount to see how many browser windows you can ask for.
  // Window numbers are 0-based.
  scoped_refptr<BrowserProxy> GetBrowserWindow(int window_index);

  // Finds the first browser window that is not incognito mode and of type
  // TYPE_NORMAL, and returns its corresponding BrowserProxy, transferring
  // ownership of the pointer to the caller.
  // On failure, returns NULL.
  scoped_refptr<BrowserProxy> FindNormalBrowserWindow();

  // Returns the BrowserProxy for the browser window which was last active,
  // transferring ownership of the pointer to the caller.
  // TODO: If there was no last active browser window, or the last active
  // browser window no longer exists (for example, if it was closed),
  // returns GetBrowserWindow(0). See crbug.com/10501. As for now this
  // function is flakey.
  scoped_refptr<BrowserProxy> GetLastActiveBrowserWindow();

  // Returns the WindowProxy for the currently active window, transferring
  // ownership of the pointer to the caller.
  // On failure, returns NULL.
  scoped_refptr<WindowProxy> GetActiveWindow();

  // Tells the browser to enable or disable network request filtering.  Returns
  // false if the message fails to send to the browser.
  bool SetFilteredInet(bool enabled);

  // Sends the browser a new proxy configuration to start using. Returns true
  // if the proxy config was successfully sent, false otherwise.
  bool SendProxyConfig(const std::string& new_proxy_config);

  // These methods are intended to be called by the background thread
  // to signal that the given event has occurred, and that any corresponding
  // Wait... function can return.
  void SignalAppLaunch(const std::string& version_string);
  void SignalInitialLoads();
  // load_time is how long, in ms, the tab contents took to load.
  void SignalNewTabUITab(int load_time);

  // Set whether or not running the save page as... command show prompt the
  // user for a download path.  Returns true if the message is successfully
  // sent.
  bool SavePackageShouldPromptUser(bool should_prompt);

  // Turn extension automation mode on and off.  When extension automation
  // mode is turned on, the automation host can overtake extension API calls
  // e.g. to make UI tests for extensions easier to write.  Returns true if
  // the message is successfully sent.
  bool SetEnableExtensionAutomation(bool enable_automation);

  // Returns the ID of the automation IPC channel, so that it can be
  // passed to the app as a launch parameter.
  const std::string& channel_id() const { return channel_id_; }

#if defined(OS_POSIX)
  base::file_handle_mapping_vector fds_to_map() const;
#endif

  // AutomationMessageSender implementations.
  virtual bool Send(IPC::Message* message);
  virtual bool SendWithTimeout(IPC::Message* message, int timeout,
                               bool* is_timeout);

  // Wrapper over AutomationHandleTracker::InvalidateHandle. Receives the
  // message from AutomationProxy, unpacks the messages and routes that call to
  // the tracker.
  void InvalidateHandle(const IPC::Message& message);

#if defined(OS_WIN)
  // TODO(port): Enable when we can replace HWND.

  // Creates a tab that can hosted in an external process. The function
  // returns a TabProxy representing the tab as well as a window handle
  // that can be reparented in another process.
  scoped_refptr<TabProxy> CreateExternalTab(HWND parent,
      const gfx::Rect& dimensions, unsigned int style, bool incognito,
      HWND* external_tab_container, HWND* tab);
#endif  // defined(OS_WIN)

  int command_execution_timeout_ms() const {
    return static_cast<int>(command_execution_timeout_.InMilliseconds());
  }

  // Returns the server version of the server connected. You may only call this
  // method after WaitForAppLaunch() has returned SUCCESS or VERSION_MISMATCH.
  // If you call it before this, the return value is undefined.
  std::string server_version() const {
    return server_version_;
  }

  // Call this while passing true to tell the automation proxy to perform
  // a version check when WaitForAppLaunch() is called. Note that
  // platform_version_check_ defaults to false.
  void set_perform_version_check(bool perform_version_check) {
    perform_version_check_ = perform_version_check;
  }

 protected:
  template <class T> scoped_refptr<T> ProxyObjectFromHandle(int handle);
  void InitializeChannelID();
  void InitializeThread();
  void InitializeChannel();
  void InitializeHandleTracker();

  std::string channel_id_;
  scoped_ptr<base::Thread> thread_;
  scoped_ptr<IPC::SyncChannel> channel_;
  scoped_ptr<AutomationHandleTracker> tracker_;

  base::WaitableEvent app_launched_;
  base::WaitableEvent initial_loads_complete_;
  base::WaitableEvent new_tab_ui_load_complete_;
  int new_tab_ui_load_time_;

  // An event that notifies when we are shutting-down.
  scoped_ptr<base::WaitableEvent> shutdown_event_;

  // The version of the automation provider we are communicating with.
  std::string server_version_;

  // Used to guard against multiple hello messages being received.
  int app_launch_signaled_;

  // Whether to perform a version check between the automation proxy and
  // the automation provider at connection time. Defaults to false, you can
  // set this to true if building the automation proxy into a module with
  // a version resource.
  bool perform_version_check_;

  // Delay to let the browser execute the command.
  base::TimeDelta command_execution_timeout_;

  DISALLOW_COPY_AND_ASSIGN(AutomationProxy);
};

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_H__
