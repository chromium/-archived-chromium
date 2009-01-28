// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "chrome/test/automation/automation_proxy.h"

#include "base/logging.h"
#include "base/ref_counted.h"
#include "chrome/common/ipc_message_macros.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/views/dialog_delegate.h"

using base::TimeDelta;
using base::TimeTicks;

// This class exists to group together the data and functionality used for
// synchronous automation requests.
class AutomationRequest :
    public base::RefCountedThreadSafe<AutomationRequest> {
public:
  AutomationRequest() {
    static int32 routing_id = 0;
    routing_id_ = ++routing_id;
    received_response_ = ::CreateEvent(NULL, TRUE, FALSE, NULL);
    DCHECK(received_response_);
  }
  ~AutomationRequest() {
    DCHECK(received_response_);
    ::CloseHandle(received_response_);
  }

  // This is called on the foreground thread to block while waiting for a
  // response from the app.
  // The function returns true if response is received, and returns false
  // if there is a failure or timeout.
  bool WaitForResponse(uint32 timeout_ms, bool* is_timeout) {
    uint32 result = ::WaitForSingleObject(received_response_, timeout_ms);
    if (is_timeout)
      *is_timeout = (result == WAIT_TIMEOUT);

    return result != WAIT_FAILED && result != WAIT_TIMEOUT;
  }

  // This is called on the background thread once the response has been
  // received and the foreground thread can resume execution.
  bool SignalResponseReady(const IPC::Message& response) {
    response_.reset(new IPC::Message(response));

    DCHECK(received_response_);
    return !!::SetEvent(received_response_);
  }

  // This can be used to take ownership of the response object that
  // we've received, reducing the need to copy the message.
  void GrabResponse(IPC::Message** response) {
    DCHECK(response);
    *response = response_.get();
    response_.release();
  }

  int32 routing_id() { return routing_id_; }

  const IPC::Message& response() {
    DCHECK(response_.get());
    return *(response_.get());
  }

private:
  DISALLOW_EVIL_CONSTRUCTORS(AutomationRequest);

  int32 routing_id_;
  scoped_ptr<IPC::Message> response_;
  HANDLE received_response_;
};

namespace {

// This object allows messages received on the background thread to be
// properly triaged.
class AutomationMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  AutomationMessageFilter(AutomationProxy* server) : server_(server) {}

  // Return true to indicate that the message was handled, or false to let
  // the message be handled in the default way.
  virtual bool OnMessageReceived(const IPC::Message& message) {
    {
      // Here we're checking to see if it matches the (because there should
      // be at most one) synchronous request.  If the received message is
      // the response to the synchronous request, we clear the server's
      // "current_request" pointer and signal to the request object that
      // the response is ready for processing.  We're clearing current_request
      // here rather than on the foreground thread so that we can assert
      // that both threads perceive it as cleared at the time that the
      // foreground thread wakes up.
      scoped_refptr<AutomationRequest> request = server_->current_request();
      if (request.get() && (message.routing_id() == request->routing_id())) {
        server_->clear_current_request();
        request->SignalResponseReady(message);
        return true;
      }
      if (message.routing_id() > 0) {
        // The message is either the previous async response or arrived
        // after timeout.
        return true;
      }
    }

    bool handled = true;

    IPC_BEGIN_MESSAGE_MAP(AutomationMessageFilter, message)
      IPC_MESSAGE_HANDLER_GENERIC(
        AutomationMsg_Hello, server_->SignalAppLaunch());
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

 private:
  AutomationProxy* server_;
};

}  // anonymous namespace


AutomationProxy::AutomationProxy(int command_execution_timeout_ms)
    : current_request_(NULL),
      command_execution_timeout_ms_(command_execution_timeout_ms) {
  InitializeEvents();
  InitializeChannelID();
  InitializeThread();
  InitializeChannel();
  InitializeHandleTracker();
}

AutomationProxy::~AutomationProxy() {
}

void AutomationProxy::InitializeEvents() {
  app_launched_ =
      CreateEvent(NULL,   // Handle cannot be inherited by child processes.
                  TRUE,   // No automatic reset after a waiting thread released.
                  FALSE,  // Initially not signalled.
                  NULL);  // No name.
  DCHECK(app_launched_);

  // See the above call to CreateEvent to understand these parameters.
  initial_loads_complete_ = CreateEvent(NULL, TRUE, FALSE, NULL);
  DCHECK(initial_loads_complete_);

  // See the above call to CreateEvent to understand these parameters.
  new_tab_ui_load_complete_ = CreateEvent(NULL, TRUE, FALSE, NULL);
  DCHECK(new_tab_ui_load_complete_);
}

void AutomationProxy::InitializeChannelID() {
  // The channel counter keeps us out of trouble if we create and destroy
  // several AutomationProxies sequentially over the course of a test run.
  // (Creating the channel sometimes failed before when running a lot of
  // tests in sequence, and our theory is that sometimes the channel ID
  // wasn't getting freed up in time for the next test.)
  static int channel_counter = 0;

  std::wostringstream buf;
  buf << L"ChromeTestingInterface:" << GetCurrentProcessId() <<
         L"." << ++channel_counter;
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
  channel_.reset(new IPC::ChannelProxy(
    channel_id_,
    IPC::Channel::MODE_SERVER,
    this,  // we are the listener
    new AutomationMessageFilter(this),
    thread_->message_loop()));
}

void AutomationProxy::InitializeHandleTracker() {
  tracker_.reset(new AutomationHandleTracker(this));
}

bool AutomationProxy::WaitForAppLaunch() {
  return ::WaitForSingleObject(app_launched_,
                               command_execution_timeout_ms_) == WAIT_OBJECT_0;
}

void AutomationProxy::SignalAppLaunch() {
  ::SetEvent(app_launched_);
}

bool AutomationProxy::WaitForInitialLoads() {
  return ::WaitForSingleObject(initial_loads_complete_,
                               command_execution_timeout_ms_) == WAIT_OBJECT_0;
}

bool AutomationProxy::WaitForInitialNewTabUILoad(int* load_time) {
  if (::WaitForSingleObject(new_tab_ui_load_complete_,
                            command_execution_timeout_ms_) == WAIT_OBJECT_0) {
    *load_time = new_tab_ui_load_time_;
    ::ResetEvent(new_tab_ui_load_complete_);
    return true;
  }
  return false;
}

void AutomationProxy::SignalInitialLoads() {
  ::SetEvent(initial_loads_complete_);
}

void AutomationProxy::SignalNewTabUITab(int load_time) {
  new_tab_ui_load_time_ = load_time;
  ::SetEvent(new_tab_ui_load_complete_);
}

bool AutomationProxy::SavePackageShouldPromptUser(bool should_prompt) {
  return Send(new AutomationMsg_SavePackageShouldPromptUser(0, should_prompt));
}

bool AutomationProxy::GetBrowserWindowCount(int* num_windows) {
  if (!num_windows) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool is_timeout = true;
  bool succeeded = SendAndWaitForResponseWithTimeout(
      new AutomationMsg_BrowserWindowCountRequest(0), &response,
      AutomationMsg_BrowserWindowCountResponse::ID,
      command_execution_timeout_ms_, &is_timeout);
  if (!succeeded)
    return false;

  if (is_timeout) {
    DLOG(ERROR) << "GetWindowCount did not complete in a timely fashion";
    return false;
  }

  void* iter = NULL;
  if (!response->ReadInt(&iter, num_windows)) {
    succeeded = false;
  }

  delete response;
  return succeeded;
}

bool AutomationProxy::WaitForWindowCountToChange(int count, int* new_count,
                                                 int wait_timeout) {
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout);
  while (TimeTicks::Now() - start < timeout) {
    bool succeeded = GetBrowserWindowCount(new_count);
    if (!succeeded) return false;
    if (count != *new_count) return true;
    Sleep(automation::kSleepTime);
  }
  // Window count never changed.
  return false;
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
    Sleep(automation::kSleepTime);
  }
  // Window count never reached the value we sought.
  return false;
}

bool AutomationProxy::GetShowingAppModalDialog(
    bool* showing_app_modal_dialog,
    views::DialogDelegate::DialogButton* button) {
  if (!showing_app_modal_dialog || !button) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool is_timeout = true;
  if (!SendAndWaitForResponseWithTimeout(
          new AutomationMsg_ShowingAppModalDialogRequest(0), &response,
          AutomationMsg_ShowingAppModalDialogResponse::ID,
          command_execution_timeout_ms_, &is_timeout)) {
    return false;
  }

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on exit.
  if (is_timeout) {
    DLOG(ERROR) << "ShowingAppModalDialog did not complete in a timely fashion";
    return false;
  }

  void* iter = NULL;
  int button_int = 0;
  if (!response->ReadBool(&iter, showing_app_modal_dialog) ||
      !response->ReadInt(&iter, &button_int))
    return false;

  *button = static_cast<views::DialogDelegate::DialogButton>(button_int);
  return true;
}

bool AutomationProxy::ClickAppModalDialogButton(
    views::DialogDelegate::DialogButton button) {
  IPC::Message* response = NULL;
  bool is_timeout = true;
  if (!SendAndWaitForResponseWithTimeout(
          new AutomationMsg_ClickAppModalDialogButtonRequest(0, button),
          &response, AutomationMsg_ClickAppModalDialogButtonResponse::ID,
          command_execution_timeout_ms_, &is_timeout)) {
    return false;
  }

  bool succeeded = false;
  void* iter = NULL;
  if (!response->ReadBool(&iter, &succeeded))
    return false;

  return succeeded;
}

bool AutomationProxy::WaitForAppModalDialog(int wait_timeout) {
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout);
  while (TimeTicks::Now() - start < timeout) {
    bool dialog_shown = false;
    views::DialogDelegate::DialogButton button =
        views::DialogDelegate::DIALOGBUTTON_NONE;
    bool succeeded = GetShowingAppModalDialog(&dialog_shown, &button);
    if (!succeeded) {
      // Try again next round, but log it.
      DLOG(ERROR) << "GetShowingAppModalDialog returned false";
    } else if (dialog_shown) {
      return true;
    }
    Sleep(automation::kSleepTime);
  }
  // Dialog never shown.
  return false;
}

bool AutomationProxy::SetFilteredInet(bool enabled) {
  return Send(new AutomationMsg_SetFilteredInet(0, enabled));
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

WindowProxy* AutomationProxy::GetActiveWindow() {
  IPC::Message* response = NULL;
  bool is_timeout = true;
  bool succeeded = SendAndWaitForResponseWithTimeout(
      new AutomationMsg_ActiveWindowRequest(0), &response,
      AutomationMsg_ActiveWindowResponse::ID,
      command_execution_timeout_ms_, &is_timeout);
  if (!succeeded)
    return NULL;

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on exit.
  if (is_timeout) {
    DLOG(ERROR) << "GetActiveWindow did not complete in a timely fashion";
    return NULL;
  }

  void* iter = NULL;
  int handle;
  if (response->ReadInt(&iter, &handle) && (handle != 0))
    return new WindowProxy(this, tracker_.get(), handle);

  return NULL;
}


BrowserProxy* AutomationProxy::GetBrowserWindow(int window_index) {
  IPC::Message* response;
  bool is_timeout = true;
  bool succeeded = SendAndWaitForResponseWithTimeout(
    new AutomationMsg_BrowserWindowRequest(0, window_index), &response,
    AutomationMsg_BrowserWindowResponse::ID,
    command_execution_timeout_ms_, &is_timeout);
  if (!succeeded)
    return NULL;

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on exit.
  if (is_timeout) {
    DLOG(ERROR) << "GetBrowserWindow did not complete in a timely fashion";
    return NULL;
  }

  void* iter = NULL;
  int handle;
  if (!response->ReadInt(&iter, &handle) || (handle == 0)) {
    DLOG(ERROR) << "Bad response from the window getter.";
    return NULL;
  }
  return new BrowserProxy(this, tracker_.get(), handle);
}

BrowserProxy* AutomationProxy::GetLastActiveBrowserWindow() {
  IPC::Message* response;
  bool is_timeout = true;
  bool succeeded = SendAndWaitForResponseWithTimeout(
    new AutomationMsg_LastActiveBrowserWindowRequest(0),
    &response, AutomationMsg_LastActiveBrowserWindowResponse::ID,
    command_execution_timeout_ms_, &is_timeout);
  if (!succeeded)
    return NULL;

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on exit.
  if (is_timeout) {
    DLOG(ERROR) << "GetLastActiveBrowserWindow did not complete in a timely fashion";
    return NULL;
  }

  void* iter = NULL;
  int handle;
  if (!response->ReadInt(&iter, &handle) || (handle == 0)) {
    DLOG(ERROR) << "Bad response from the window getter.";
    return NULL;
  }
  return new BrowserProxy(this, tracker_.get(), handle);
}

bool AutomationProxy::Send(IPC::Message* message) {
  if (channel_.get())
    return channel_->Send(message);

  DLOG(WARNING) << "Channel has been closed; dropping message!";
  delete message;
  return false;
}

bool AutomationProxy::SendAndWaitForResponse(IPC::Message* request,
                                             IPC::Message** response,
                                             int response_type) {
  return SendAndWaitForResponseWithTimeout(request, response, response_type,
                                           INFINITE, NULL);
}

bool AutomationProxy::SendAndWaitForResponseWithTimeout(
    IPC::Message* request,
    IPC::Message** response,
    int response_type,
    uint32 timeout_ms,
    bool* is_timeout) {

  DCHECK(request);
  DCHECK(response);
  DCHECK(!current_request_) <<
    "Only one synchronous request should exist at any given time.";

  scoped_refptr<AutomationRequest> req = new AutomationRequest;
  current_request_ = req;

  // Rewrite the message's routing ID so that we'll recognize the response
  // to it when it comes back.
  request->set_routing_id(req->routing_id());
  bool result = Send(request) && req->WaitForResponse(timeout_ms, is_timeout);
  if (!result || req->response().type() != response_type) {
    // If Send() or WaitForResponse() failed, current_request_ may not have
    // gotten cleared by the background thread, so we'll clear it here.
    current_request_ = NULL;
    return false;
  }
  req->GrabResponse(response);

  return true;
}

void AutomationProxy::InvalidateHandle(const IPC::Message& message) {
  void* iter = NULL;
  int handle;

  if (message.ReadInt(&iter, &handle)) {
    tracker_->InvalidateHandle(handle);
  }
}

bool AutomationProxy::OpenNewBrowserWindow(int show_command) {
  return Send(new AutomationMsg_OpenNewBrowserWindow(0, show_command));
}

TabProxy* AutomationProxy::CreateExternalTab(HWND parent,
                                             const gfx::Rect& dimensions,
                                             unsigned int style,
                                             HWND* external_tab_container) {
  IPC::Message* response = NULL;
  bool succeeded = SendAndWaitForResponse(
    new AutomationMsg_CreateExternalTab(0, parent, dimensions, style),
    &response, AutomationMsg_CreateExternalTabResponse::ID);
  if (!succeeded) {
    return NULL;
  }
  void* iter = NULL;
  int handle = 0;
  TabProxy* tab_proxy = NULL;
  if (IPC::ReadParam(response, &iter, external_tab_container) &&
      IsWindow(*external_tab_container)) {
    if (response->ReadInt(&iter, &handle) &&
        (handle >= 0)) {
      succeeded = true;
      tab_proxy = new TabProxy(this, tracker_.get(), handle);
    }
  } else {
    succeeded = false;
  }
  delete response;
  return tab_proxy;
}
