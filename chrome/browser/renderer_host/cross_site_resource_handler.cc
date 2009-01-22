// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/cross_site_resource_handler.h"

#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/web_contents.h"

namespace {
// Task to notify the WebContents that a cross-site response has begun, so that
// WebContents can tell the old page to run its onunload handler.
class CrossSiteNotifyTabTask : public Task {
 public:
  CrossSiteNotifyTabTask(int render_process_host_id,
                         int render_view_id,
                         int request_id)
    : render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id),
      request_id_(request_id) {}

  void Run() {
    RenderViewHost* view =
        RenderViewHost::FromID(render_process_host_id_, render_view_id_);
    if (view) {
      view->OnCrossSiteResponse(render_process_host_id_, request_id_);
    } else {
      // The view couldn't be found.
      // TODO(creis): Should notify the IO thread to proceed anyway, using
      // ResourceDispatcherHost::OnClosePageACK.
    }
  }

 private:
  int render_process_host_id_;
  int render_view_id_;
  int request_id_;
};

class CancelPendingRenderViewTask : public Task {
 public:
  CancelPendingRenderViewTask(int render_process_host_id,
                              int render_view_id)
    : render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id) {}

  void Run() {
    WebContents* web_contents =
        tab_util::GetWebContentsByID(render_process_host_id_, render_view_id_);
    if (web_contents)
      web_contents->CrossSiteNavigationCanceled();
  }

 private:
  int render_process_host_id_;
  int render_view_id_;
};
}

CrossSiteResourceHandler::CrossSiteResourceHandler(
    ResourceHandler* handler,
    int render_process_host_id,
    int render_view_id,
    ResourceDispatcherHost* resource_dispatcher_host)
    : next_handler_(handler),
      render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id),
      has_started_response_(false),
      in_cross_site_transition_(false),
      request_id_(-1),
      completed_during_transition_(false),
      completed_status_(),
      response_(NULL),
      rdh_(resource_dispatcher_host) {}

bool CrossSiteResourceHandler::OnRequestRedirected(int request_id,
                                                   const GURL& new_url) {
  // We should not have started the transition before being redirected.
  DCHECK(!in_cross_site_transition_);
  return next_handler_->OnRequestRedirected(request_id, new_url);
}

bool CrossSiteResourceHandler::OnResponseStarted(int request_id,
                                                 ResourceResponse* response) {
  // At this point, we know that the response is safe to send back to the
  // renderer: it is not a download, and it has passed the SSL and safe
  // browsing checks.
  // We should not have already started the transition before now.
  DCHECK(!in_cross_site_transition_);
  has_started_response_ = true;

  // Look up the request and associated info.
  ResourceDispatcherHost::GlobalRequestID global_id(render_process_host_id_,
                                                    request_id);
  URLRequest* request = rdh_->GetURLRequest(global_id);
  if (!request) {
    DLOG(WARNING) << "Request wasn't found";
    return false;
  }
  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request);

  // If this is a download, just pass the response through without doing a
  // cross-site check.  The renderer will see it is a download and abort the
  // request.
  if (info->is_download) {
    return next_handler_->OnResponseStarted(request_id, response);
  }

  // Tell the renderer to run the onunload event handler, and wait for the
  // reply.
  StartCrossSiteTransition(request_id, response, global_id);
  return true;
}

bool CrossSiteResourceHandler::OnWillRead(int request_id,
                                          char** buf, int* buf_size,
                                          int min_size) {
  return next_handler_->OnWillRead(request_id, buf, buf_size, min_size);
}

bool CrossSiteResourceHandler::OnReadCompleted(int request_id,
                                               int* bytes_read) {
  if (!in_cross_site_transition_) {
    return next_handler_->OnReadCompleted(request_id, bytes_read);
  }
  return true;
}

bool CrossSiteResourceHandler::OnResponseCompleted(
    int request_id,
    const URLRequestStatus& status) {
  if (!in_cross_site_transition_) {
    if (has_started_response_) {
      // We've already completed the transition, so just pass it through.
      return next_handler_->OnResponseCompleted(request_id, status);
    } else {
      // Some types of failures will call OnResponseCompleted without calling
      // CrossSiteResourceHandler::OnResponseStarted.
      if (status.status() == URLRequestStatus::CANCELED) {
        // Here the request was canceled, which happens when selecting "take me
        // back" from an interstitial.  Nothing to do but cancel the pending
        // render view host.
        CancelPendingRenderViewTask* task =
            new CancelPendingRenderViewTask(render_process_host_id_,
                                            render_view_id_);
        rdh_->ui_loop()->PostTask(FROM_HERE, task);
        return next_handler_->OnResponseCompleted(request_id, status);
      } else {
        // An error occured, we should wait now for the cross-site transition,
        // so that the error message (e.g., 404) can be displayed to the user.
        // Also continue with the logic below to remember that we completed
        // during the cross-site transition.
        ResourceDispatcherHost::GlobalRequestID global_id(
            render_process_host_id_, request_id);
        StartCrossSiteTransition(request_id, NULL, global_id);
      }
    }
  }

  // We have to buffer the call until after the transition completes.
  completed_during_transition_ = true;
  completed_status_ = status;

  // Return false to tell RDH not to notify the world or clean up the
  // pending request.  We will do so in ResumeResponse.
  return false;
}

// We can now send the response to the new renderer, which will cause
// WebContents to swap in the new renderer and destroy the old one.
void CrossSiteResourceHandler::ResumeResponse() {
  DCHECK(request_id_ != -1);
  DCHECK(in_cross_site_transition_);
  in_cross_site_transition_ = false;

  // Find the request for this response.
  ResourceDispatcherHost::GlobalRequestID global_id(render_process_host_id_,
                                                    request_id_);
  URLRequest* request = rdh_->GetURLRequest(global_id);
  if (!request) {
    DLOG(WARNING) << "Resuming a request that wasn't found";
    return;
  }
  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request);

  if (has_started_response_) {
    // Send OnResponseStarted to the new renderer.
    DCHECK(response_);
    next_handler_->OnResponseStarted(request_id_, response_);

    // Unpause the request to resume reading.  Any further reads will be
    // directed toward the new renderer.
    rdh_->PauseRequest(render_process_host_id_, request_id_, false);
  }

  // Remove ourselves from the ExtraRequestInfo.
  info->cross_site_handler = NULL;

  // If the response completed during the transition, notify the next
  // event handler.
  if (completed_during_transition_) {
    next_handler_->OnResponseCompleted(request_id_, completed_status_);

    // Since we didn't notify the world or clean up the pending request in
    // RDH::OnResponseCompleted during the transition, we should do it now.
    rdh_->NotifyResponseCompleted(request, render_process_host_id_);
    rdh_->RemovePendingRequest(render_process_host_id_, request_id_);
  }
}

// Prepare to render the cross-site response in a new RenderViewHost, by
// telling the old RenderViewHost to run its onunload handler.
void CrossSiteResourceHandler::StartCrossSiteTransition(
    int request_id,
    ResourceResponse* response,
    ResourceDispatcherHost::GlobalRequestID global_id) {
  in_cross_site_transition_ = true;
  request_id_ = request_id;
  response_ = response;

  // Store this handler on the ExtraRequestInfo, so that RDH can call our
  // ResumeResponse method when the close ACK is received.
  URLRequest* request = rdh_->GetURLRequest(global_id);
  if (!request) {
    DLOG(WARNING) << "Cross site response for a request that wasn't found";
    return;
  }
  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request);
  info->cross_site_handler = this;

  if (has_started_response_) {
    // Pause the request until the old renderer is finished and the new
    // renderer is ready.
    rdh_->PauseRequest(render_process_host_id_, request_id, true);
  }
  // If our OnResponseStarted wasn't called, then we're being called by
  // OnResponseCompleted after a failure.  We don't need to pause, because
  // there will be no reads.

  // Tell the tab responsible for this request that a cross-site response is
  // starting, so that it can tell its old renderer to run its onunload
  // handler now.  We will wait to hear the corresponding ClosePage_ACK.
  CrossSiteNotifyTabTask* task =
      new CrossSiteNotifyTabTask(render_process_host_id_,
                                 render_view_id_,
                                 request_id);
  rdh_->ui_loop()->PostTask(FROM_HERE, task);
}
