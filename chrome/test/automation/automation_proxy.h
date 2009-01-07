// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_H__
#define CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_H__

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/common/ipc_message.h"
#include "chrome/test/automation/automation_handle_tracker.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/views/dialog_delegate.h"

class AutomationRequest;
class BrowserProxy;
class WindowProxy;
class TabProxy;
class AutocompleteEditProxy;

// This is an interface that AutomationProxy-related objects can use to
// access the message-sending abilities of the Proxy.
class AutomationMessageSender : public IPC::Message::Sender {
 public:
  // Sends a message synchronously (from the perspective of the caller's
  // thread, at least); it doesn't return until a response has been received.
  // This method takes ownership of the request object passed in.  The caller
  // is responsible for deleting the response object when they're done with it.
  // response_type should be set to the message type of the expected response.
  // A response object will only be available if the method returns true.
  // NOTE: This method will overwrite any routing_id on the request message,
  //       since it uses this field to match the response up with the request.
  virtual bool SendAndWaitForResponse(IPC::Message* request,
                                      IPC::Message** response,
                                      int response_type) = 0;

  // Sends a message synchronously; it doesn't return until a response has been
  // received or a timeout has expired.
  // The function returns true if a response is received, and returns false if
  // there is a failure or timeout (in milliseconds). If return after timeout,
  // is_timeout is set to true.
  // See the comments in SendAndWaitForResponse for other details on usage.
  // NOTE: When timeout occurs, the connection between proxy provider may be
  //       in transit state. Specifically, there might be pending IPC messages,
  //       and the proxy provider might be still working on the previous
  //       request.
  virtual bool SendAndWaitForResponseWithTimeout(IPC::Message* request,
                                                 IPC::Message** response,
                                                 int response_type,
                                                 uint32 timeout_ms,
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
  // Returns true if the launch is successful
  bool WaitForAppLaunch();

  // Waits for any initial page loads to complete.
  // NOTE: this only fires once for a run of the application.
  // Returns true if the load is successful
  bool WaitForInitialLoads();

  // Waits for the initial destinations tab to report that it has finished
  // querying.  |load_time| is filled in with how long it took, in milliseconds.
  // NOTE: this only fires once for a run of the application.
  // Returns true if the load is successful.
  bool WaitForInitialNewTabUILoad(int* load_time);

  // Open a new browser window, returning true on success. |show_command|
  // identifies how the window should be shown.
  // False likely indicates an IPC error.
  bool OpenNewBrowserWindow(int show_command);

  // Fills the number of open browser windows into the given variable, returning
  // true on success. False likely indicates an IPC error.
  bool GetBrowserWindowCount(int* num_windows);

  // Block the thread until the window count changes.
  // First parameter is the original window count.
  // The second parameter is updated with the number of window tabs.
  // The third parameter specifies the timeout length for the wait loop.
  // Returns false if the window count does not change before time out.
  // TODO(evanm): this function has a confusing name and semantics; it should
  // be deprecated for WaitForWindowCountToBecome.
  bool WaitForWindowCountToChange(int count, int* new_counter,
                                  int wait_timeout);

  // Block the thread until the window count becomes the provided value.
  // Returns true on success.
  bool WaitForWindowCountToBecome(int target_count, int wait_timeout);

  // Returns whether an app modal dialog window is showing right now (i.e., a
  // javascript alert), and what buttons it contains.
  bool GetShowingAppModalDialog(bool* showing_app_modal_dialog,
                                views::DialogDelegate::DialogButton* button);

  // Simulates a click on a dialog button.
  bool ClickAppModalDialogButton(views::DialogDelegate::DialogButton button);

  // Block the thread until a modal dialog is displayed. Returns true on
  // success.
  bool WaitForAppModalDialog(int wait_timeout);

  // Returns the BrowserProxy for the browser window at the given index,
  // transferring ownership of the pointer to the caller.
  // On failure, returns NULL.
  //
  // Use GetBrowserWindowCount to see how many browser windows you can ask for.
  // Window numbers are 0-based.
  BrowserProxy* GetBrowserWindow(int window_index);

  // Returns the BrowserProxy for the browser window which was last active,
  // transferring ownership of the pointer to the caller.
  // If there was no last active browser window, or the last active browser
  // window no longer exists (for example, if it was closed), returns
  // GetBrowserWindow(0).
  BrowserProxy* GetLastActiveBrowserWindow();

  // Returns the WindowProxy for the currently active window, transferring
  // ownership of the pointer to the caller.
  // On failure, returns NULL.
  WindowProxy* GetActiveWindow();

  // Returns the browser this window corresponds to, or NULL if this window
  // is not a browser.  The caller owns the returned BrowserProxy.
  BrowserProxy* GetBrowserForWindow(WindowProxy* window);

  // Same as GetBrowserForWindow except return NULL if response isn't received
  // before the specified timeout.
  BrowserProxy* GetBrowserForWindowWithTimeout(WindowProxy* window,
                                               uint32 timeout_ms,
                                               bool* is_timeout);

  // Returns the WindowProxy for this browser's window. It can be used to
  // retreive view bounds, simulate clicks and key press events.  The caller
  // owns the returned WindowProxy.
  // On failure, returns NULL.
  WindowProxy* GetWindowForBrowser(BrowserProxy* browser);

  // Returns an AutocompleteEdit for this browser's window. It can be used to
  // manipulate the omnibox.  The caller owns the returned pointer.
  // On failure, returns NULL.
  AutocompleteEditProxy* GetAutocompleteEditForBrowser(BrowserProxy* browser);

  // Tells the browser to enable or disable network request filtering.  Returns
  // false if the message fails to send to the browser.
  bool SetFilteredInet(bool enabled);

  // These methods are intended to be called by the background thread
  // to signal that the given event has occurred, and that any corresponding
  // Wait... function can return.
  void SignalAppLaunch();
  void SignalInitialLoads();
  // load_time is how long, in ms, the tab contents took to load.
  void SignalNewTabUITab(int load_time);

  // Set whether or not running the save page as... command show prompt the
  // user for a download path.  Returns true if the message is successfully
  // sent.
  bool SavePackageShouldPromptUser(bool should_prompt);

  // Returns the ID of the automation IPC channel, so that it can be
  // passed to the app as a launch parameter.
  const std::wstring& channel_id() const { return channel_id_; }

  // AutomationMessageSender implementations.
  virtual bool Send(IPC::Message* message);
  virtual bool SendAndWaitForResponse(IPC::Message* request,
                                      IPC::Message** response,
                                      int response_type);
  virtual bool SendAndWaitForResponseWithTimeout(IPC::Message* request,
                                                 IPC::Message** response,
                                                 int response_type,
                                                 uint32 timeout_ms,
                                                 bool* is_timeout);

  // Returns the current AutomationRequest object.
  AutomationRequest* current_request() { return current_request_; }
  // Clears the current AutomationRequest object.
  void clear_current_request() { current_request_ = NULL; }

  // Wrapper over AutomationHandleTracker::InvalidateHandle. Receives the message
  // from AutomationProxy, unpacks the messages and routes that call to the
  // tracker.
  void InvalidateHandle(const IPC::Message& message);

  // Creates a tab that can hosted in an external process. The function
  // returns a TabProxy representing the tab as well as a window handle
  // that can be reparented in another process.
  TabProxy* CreateExternalTab(HWND* external_tab_container);

  int command_execution_timeout_ms() const {
    return command_execution_timeout_ms_;
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(AutomationProxy);

  void InitializeEvents();
  void InitializeChannelID();
  void InitializeThread();
  void InitializeChannel();
  void InitializeHandleTracker();

  std::wstring channel_id_;
  scoped_ptr<base::Thread> thread_;
  scoped_ptr<IPC::ChannelProxy> channel_;
  scoped_ptr<AutomationHandleTracker> tracker_;

  HANDLE app_launched_;
  HANDLE initial_loads_complete_;
  HANDLE new_tab_ui_load_complete_;
  int new_tab_ui_load_time_;

  AutomationRequest* current_request_;

  // Delay to let the browser execute the command.
  int command_execution_timeout_ms_;
};

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_H__
