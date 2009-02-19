// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/resolve_proxy_msg_helper.h"

#include "base/compiler_specific.h"
#include "chrome/browser/profile.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context.h"

ResolveProxyMsgHelper::ResolveProxyMsgHelper(Delegate* delegate,
                                             net::ProxyService* proxy_service)
    : proxy_service_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
          this, &ResolveProxyMsgHelper::OnResolveProxyCompleted)),
      delegate_(delegate),
      proxy_service_override_(proxy_service) {
}

void ResolveProxyMsgHelper::Start(const GURL& url, IPC::Message* reply_msg) {
  // Enqueue the pending request.
  pending_requests_.push_back(PendingRequest(url, reply_msg));

  // If nothing is in progress, start.
  if (pending_requests_.size() == 1)
    StartPendingRequest();
}

void ResolveProxyMsgHelper::OnResolveProxyCompleted(int result) {
  CHECK(!pending_requests_.empty());

  // Notify the delegate of completion.
  const PendingRequest& completed_req = pending_requests_.front();
  delegate_->OnResolveProxyCompleted(completed_req.reply_msg,
                                     result,
                                     proxy_info_.ToPacString());

  // Clear the current (completed) request.
  pending_requests_.pop_front();
  proxy_service_ = NULL;

  // Start the next request.
  if (!pending_requests_.empty())
    StartPendingRequest();
}

void ResolveProxyMsgHelper::StartPendingRequest() {
  PendingRequest& req = pending_requests_.front();

  // Verify the request wasn't started yet.
  DCHECK(NULL == req.pac_req);
  DCHECK(NULL == proxy_service_);

  // Start the request.
  proxy_service_ = GetProxyService();
  int result = proxy_service_->ResolveProxy(
      req.url, &proxy_info_, &callback_, &req.pac_req);

  // Completed synchronously.
  if (result != net::ERR_IO_PENDING)
    OnResolveProxyCompleted(result);
}

net::ProxyService* ResolveProxyMsgHelper::GetProxyService() const {
  // Unit-tests specify their own proxy service to use.
  if (proxy_service_override_)
    return proxy_service_override_;

  // Otherwise use the browser's global proxy service.
  return Profile::GetDefaultRequestContext()->proxy_service();
}

ResolveProxyMsgHelper::~ResolveProxyMsgHelper() {
  // Clear all pending requests.
  if (!pending_requests_.empty()) {
    PendingRequest req = pending_requests_.front();
    proxy_service_->CancelPacRequest(req.pac_req);
  }

  for (PendingRequestList::iterator it = pending_requests_.begin();
       it != pending_requests_.end();
       ++it) {
    delete it->reply_msg;
  }

  proxy_service_ = NULL;
  pending_requests_.clear();
}
