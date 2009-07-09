// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_resource_message_filter.h"

#include "base/message_loop.h"
#include "chrome/browser/automation/url_request_automation_job.h"


MessageLoop* AutomationResourceMessageFilter::io_loop_ = NULL;
AutomationResourceMessageFilter::RenderViewMap
    AutomationResourceMessageFilter::filtered_render_views_;

AutomationResourceMessageFilter::AutomationResourceMessageFilter()
    : channel_(NULL), unique_request_id_(1) {
  URLRequestAutomationJob::InitializeInterceptor();
}

AutomationResourceMessageFilter::~AutomationResourceMessageFilter() {
}

// Called on the IPC thread:
void AutomationResourceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(channel_ == NULL);
  channel_ = channel;
  io_loop_ = MessageLoop::current();
}

// Called on the IPC thread:
void AutomationResourceMessageFilter::OnChannelConnected(int32 peer_pid) {
}

// Called on the IPC thread:
void AutomationResourceMessageFilter::OnChannelClosing() {
  channel_ = NULL;
  request_map_.clear();
  filtered_render_views_.clear();
}

// Called on the IPC thread:
bool AutomationResourceMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  int request_id = URLRequestAutomationJob::MayFilterMessage(message);
  if (request_id) {
    RequestMap::iterator it = request_map_.find(request_id);
    if (it != request_map_.end()) {
      URLRequestAutomationJob* job = it->second;
      DCHECK(job);
      if (job) {
        job->OnMessage(message);
        return true;
      }
    }
  }

  return false;
}

// Called on the IPC thread:
bool AutomationResourceMessageFilter::Send(IPC::Message* message) {
  // This has to be called on the IO thread.
  DCHECK_EQ(io_loop_, MessageLoop::current());
  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

bool AutomationResourceMessageFilter::RegisterRequest(
    URLRequestAutomationJob* job) {
  if (!job) {
    NOTREACHED();
    return false;
  }

  DCHECK_EQ(io_loop_, MessageLoop::current());
  DCHECK(request_map_.end() == request_map_.find(job->id()));
  request_map_[job->id()] = job;
  return true;
}

void AutomationResourceMessageFilter::UnRegisterRequest(
    URLRequestAutomationJob* job) {
  DCHECK_EQ(io_loop_, MessageLoop::current());
  DCHECK(request_map_.find(job->id()) != request_map_.end());
  request_map_.erase(job->id());
}

bool AutomationResourceMessageFilter::RegisterRenderView(
    int renderer_pid, int renderer_id, int tab_handle,
    AutomationResourceMessageFilter* filter) {
  if (!renderer_pid || !renderer_id || !tab_handle) {
    NOTREACHED();
    return false;
  }

  DCHECK(io_loop_);
  io_loop_->PostTask(FROM_HERE, NewRunnableFunction(
      AutomationResourceMessageFilter::RegisterRenderViewInIOThread,
      renderer_pid, renderer_id, tab_handle, filter));
  return true;
}

void AutomationResourceMessageFilter::UnRegisterRenderView(
    int renderer_pid, int renderer_id) {
  DCHECK(io_loop_);
  io_loop_->PostTask(FROM_HERE, NewRunnableFunction(
      AutomationResourceMessageFilter::UnRegisterRenderViewInIOThread,
      renderer_pid, renderer_id));
}

void AutomationResourceMessageFilter::RegisterRenderViewInIOThread(
    int renderer_pid, int renderer_id,
    int tab_handle, AutomationResourceMessageFilter* filter) {
  DCHECK(filtered_render_views_.find(RendererId(renderer_pid, renderer_id)) ==
      filtered_render_views_.end());
  filtered_render_views_[RendererId(renderer_pid, renderer_id)] =
      AutomationDetails(tab_handle, filter);
}

void AutomationResourceMessageFilter::UnRegisterRenderViewInIOThread(
    int renderer_pid, int renderer_id) {
  DCHECK(filtered_render_views_.find(RendererId(renderer_pid, renderer_id)) !=
      filtered_render_views_.end());
  filtered_render_views_.erase(RendererId(renderer_pid, renderer_id));
}

bool AutomationResourceMessageFilter::LookupRegisteredRenderView(
    int renderer_pid, int renderer_id, AutomationDetails* details) {
  bool found = false;
  RenderViewMap::iterator it = filtered_render_views_.find(RendererId(
      renderer_pid, renderer_id));
  if (it != filtered_render_views_.end()) {
    found = true;
    if (details)
      *details = it->second;
  }

  return found;
}

