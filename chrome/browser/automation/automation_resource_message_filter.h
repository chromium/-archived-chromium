// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_MSG_FILTER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_MSG_FILTER_H_

#include <map>

#include "base/lock.h"
#include "base/platform_thread.h"
#include "chrome/common/ipc_channel_proxy.h"

class URLRequestAutomationJob;
class MessageLoop;

// This class filters out incoming automation IPC messages for network
// requests and processes them on the IPC thread.  As a result, network
// requests are not delayed by costly UI processing that may be occurring
// on the main thread of the browser.  It also means that any hangs in
// starting a network request will not interfere with browser UI.
class AutomationResourceMessageFilter
    : public IPC::ChannelProxy::MessageFilter,
      public IPC::Message::Sender {
 public:
  // Information needed to send IPCs through automation.
  struct AutomationDetails {
    AutomationDetails() : tab_handle(0) {}
    AutomationDetails(int tab, AutomationResourceMessageFilter* flt)
      : tab_handle(tab), filter(flt) {
    }

    int tab_handle;
    scoped_refptr<AutomationResourceMessageFilter> filter;
  };

  // Create the filter.
  AutomationResourceMessageFilter();
  virtual ~AutomationResourceMessageFilter();

  int NewRequestId() {
    return unique_request_id_++;
  }

  // IPC::ChannelProxy::MessageFilter methods:
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelClosing();
  virtual bool OnMessageReceived(const IPC::Message& message);

  // ResourceDispatcherHost::Receiver methods:
  virtual bool Send(IPC::Message* message);

  // Add request to the list of outstanding requests.
  bool RegisterRequest(URLRequestAutomationJob* job);

  // Remove request from the list of outstanding requests.
  void UnRegisterRequest(URLRequestAutomationJob* job);

  // Can be called from the UI thread.
  static bool RegisterRenderView(int renderer_pid, int renderer_id,
      int tab_handle, AutomationResourceMessageFilter* filter);
  static void UnRegisterRenderView(int renderer_pid, int renderer_id);

  // Called only on the IO thread.
  static bool LookupRegisteredRenderView(
      int renderer_pid, int renderer_id, AutomationDetails* details);

 protected:
  static void RegisterRenderViewInIOThread(int renderer_pid, int renderer_id,
      int tab_handle, AutomationResourceMessageFilter* filter);
  static void UnRegisterRenderViewInIOThread(int renderer_pid, int renderer_id);

 private:
  // A unique renderer id is a combination of renderer process id and
  // it's routing id.
  struct RendererId {
    int pid_;
    int id_;

    RendererId() : pid_(0), id_(0) {}
    RendererId(int pid, int id) : pid_(pid), id_(id) {}

    bool operator < (const RendererId& rhs) const {
      return ((pid_ == rhs.pid_) ? (id_ < rhs.id_) : (pid_ < rhs.pid_));
    }
  };

  typedef std::map<RendererId, AutomationDetails> RenderViewMap;
  typedef std::map<int, scoped_refptr<URLRequestAutomationJob> > RequestMap;

  // The channel associated with the automation connection. This pointer is not
  // owned by this class.
  IPC::Channel* channel_;
  static MessageLoop* io_loop_;

  // A unique request id per automation channel.
  int unique_request_id_;

  // Map of outstanding requests.
  RequestMap request_map_;

  // Map of render views interested in diverting url requests over automation.
  static RenderViewMap filtered_render_views_;

  DISALLOW_COPY_AND_ASSIGN(AutomationResourceMessageFilter);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_MSG_FILTER_H_

