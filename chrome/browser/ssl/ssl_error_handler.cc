// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_handler.h"

#include "base/message_loop.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/ssl/ssl_cert_error_handler.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

SSLErrorHandler::SSLErrorHandler(ResourceDispatcherHost* rdh,
                                 URLRequest* request,
                                 ResourceType::Type resource_type,
                                 const std::string& frame_origin,
                                 const std::string& main_frame_origin,
                                 MessageLoop* ui_loop)
    : ui_loop_(ui_loop),
      io_loop_(MessageLoop::current()),
      manager_(NULL),
      request_id_(0, 0),
      resource_dispatcher_host_(rdh),
      request_url_(request->url()),
      resource_type_(resource_type),
      frame_origin_(frame_origin),
      main_frame_origin_(main_frame_origin),
      request_has_been_notified_(false) {
  DCHECK(MessageLoop::current() != ui_loop);

  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request);
  request_id_.process_id = info->process_id;
  request_id_.request_id = info->request_id;

  if (!ResourceDispatcherHost::RenderViewForRequest(request,
                                                    &render_process_host_id_,
                                                    &tab_contents_id_))
    NOTREACHED();

  // This makes sure we don't disappear on the IO thread until we've given an
  // answer to the URLRequest.
  //
  // Release in CompleteCancelRequest, CompleteContinueRequest,
  // CompleteStartRequest or CompleteTakeNoAction.
  AddRef();
}

void SSLErrorHandler::Dispatch() {
  DCHECK(MessageLoop::current() == ui_loop_);

  TabContents* tab_contents = GetTabContents();
  if (!tab_contents) {
    // We arrived on the UI thread, but the tab we're looking for is no longer
    // here.
    OnDispatchFailed();
    return;
  }

  // Hand ourselves off to the SSLManager.
  manager_ = tab_contents->controller().ssl_manager();
  OnDispatched();
}

TabContents* SSLErrorHandler::GetTabContents() {
  return tab_util::GetTabContentsByID(render_process_host_id_,
                                      tab_contents_id_);
}

void SSLErrorHandler::CancelRequest() {
  DCHECK(MessageLoop::current() == ui_loop_);

  // We need to complete this task on the IO thread.
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
       this, &SSLErrorHandler::CompleteCancelRequest,
       net::ERR_ABORTED));
}

void SSLErrorHandler::DenyRequest() {
  DCHECK(MessageLoop::current() == ui_loop_);

  // We need to complete this task on the IO thread.
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SSLErrorHandler::CompleteCancelRequest,
      net::ERR_INSECURE_RESPONSE));
}

void SSLErrorHandler::ContinueRequest() {
  DCHECK(MessageLoop::current() == ui_loop_);

  // We need to complete this task on the IO thread.
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SSLErrorHandler::CompleteContinueRequest));
}

void SSLErrorHandler::StartRequest(FilterPolicy::Type filter_policy) {
  DCHECK(MessageLoop::current() == ui_loop_);

  // We need to complete this task on the IO thread.
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SSLErrorHandler::CompleteStartRequest, filter_policy));
}

void SSLErrorHandler::TakeNoAction() {
  DCHECK(MessageLoop::current() == ui_loop_);

  // We need to complete this task on the IO thread.
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SSLErrorHandler::CompleteTakeNoAction));
}

void SSLErrorHandler::CompleteCancelRequest(int error) {
  DCHECK(MessageLoop::current() == io_loop_);

  // It is important that we notify the URLRequest only once.  If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);
  if (request_has_been_notified_)
    return;

  URLRequest* request = resource_dispatcher_host_->GetURLRequest(request_id_);
  if (request) {
    // The request can be NULL if it was cancelled by the renderer (as the
    // result of the user navigating to a new page from the location bar).
    DLOG(INFO) << "CompleteCancelRequest() url: " << request->url().spec();
    SSLCertErrorHandler* cert_error = AsSSLCertErrorHandler();
    if (cert_error)
      request->SimulateSSLError(error, cert_error->ssl_info());
    else
      request->SimulateError(error);
  }
  request_has_been_notified_ = true;

  // We're done with this object on the IO thread.
  Release();
}

void SSLErrorHandler::CompleteContinueRequest() {
  DCHECK(MessageLoop::current() == io_loop_);

  // It is important that we notify the URLRequest only once.  If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);
  if (request_has_been_notified_)
    return;

  URLRequest* request = resource_dispatcher_host_->GetURLRequest(request_id_);
  if (request) {
    // The request can be NULL if it was cancelled by the renderer (as the
    // result of the user navigating to a new page from the location bar).
    DLOG(INFO) << "CompleteContinueRequest() url: " << request->url().spec();
    request->ContinueDespiteLastError();
  }
  request_has_been_notified_ = true;

  // We're done with this object on the IO thread.
  Release();
}

void SSLErrorHandler::CompleteStartRequest(FilterPolicy::Type filter_policy) {
  DCHECK(MessageLoop::current() == io_loop_);

  // It is important that we notify the URLRequest only once.  If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);
  if (request_has_been_notified_)
    return;

  URLRequest* request = resource_dispatcher_host_->GetURLRequest(request_id_);
  if (request) {
    // The request can be NULL if it was cancelled by the renderer (as the
    // result of the user navigating to a new page from the location bar).
    DLOG(INFO) << "CompleteStartRequest() url: " << request->url().spec();
    // The request should not have been started (SUCCESS is the initial state).
    DCHECK(request->status().status() == URLRequestStatus::SUCCESS);
    ResourceDispatcherHost::ExtraRequestInfo* info =
        ResourceDispatcherHost::ExtraInfoForRequest(request);
    info->filter_policy = filter_policy;
    request->Start();
  }
  request_has_been_notified_ = true;

  // We're done with this object on the IO thread.
  Release();
}

void SSLErrorHandler::CompleteTakeNoAction() {
  DCHECK(MessageLoop::current() == io_loop_);

  // It is important that we notify the URLRequest only once.  If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);
  if (request_has_been_notified_)
    return;

  request_has_been_notified_ = true;

  // We're done with this object on the IO thread.
  Release();
}
