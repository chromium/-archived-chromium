// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_RESOLVE_PROXY_MSG_HELPER_
#define CHROME_BROWSER_NET_RESOLVE_PROXY_MSG_HELPER_

#include <deque>
#include <string>

#include "chrome/common/ipc_message.h"
#include "net/base/completion_callback.h"
#include "googleurl/src/gurl.h"
#include "net/proxy/proxy_service.h"

// This class holds the common logic used to respond to the messages:
// {PluginProcessHostMsg_ResolveProxy, ViewHostMsg_ResolveProxy}.
//
// This involves kicking off a ProxyResolve request on the IO thread using
// the specified proxy service.
//
// When the request completes, it calls the delegate's OnProxyResolveCompleted()
// method, passing it the result (error code + proxy list), as well as the
// IPC::Message pointer that had been stored.
//
// When an instance of ResolveProxyMsgHelper is destroyed, it cancels any
// outstanding proxy resolve requests with the proxy service. It also deletes
// the stored IPC::Message pointers for pending requests.
//
// This object is expected to live on the IO thread.
class ResolveProxyMsgHelper {
 public:
  class Delegate {
   public:
    // Callback for when the proxy resolve request has completed.
    //   |reply_msg|  -- The same pointer that the request was started with.
    //   |result|     -- The network error code from ProxyService.
    //   |proxy_list| -- The PAC string result from ProxyService.
    virtual void OnResolveProxyCompleted(IPC::Message* reply_msg,
                                         int result,
                                         const std::string& proxy_list) = 0;
    virtual ~Delegate() {}
  };

  // Construct a ResolveProxyMsgHelper instance that notifies request
  // completion to |delegate|. Note that |delegate| must be live throughout
  // our lifespan. If |proxy_service| is NULL, then the current profile's
  // proxy service will be used.
  explicit ResolveProxyMsgHelper(Delegate* delegate,
                                 net::ProxyService* proxy_service);

  // Resolve proxies for |url|. Completion is notified through the delegate.
  // If multiple requests are started at the same time, they will run in
  // FIFO order, with only 1 being outstanding at a time.
  void Start(const GURL& url, IPC::Message* reply_msg);

  // Destruction cancels the current outstanding request, and clears the
  // pending queue.
  ~ResolveProxyMsgHelper();

 private:
  // Callback for the ProxyService (bound to |callback_|).
  void OnResolveProxyCompleted(int result);

  // Start the first pending request.
  void StartPendingRequest();

  // Get the proxy service instance to use.
  net::ProxyService* GetProxyService() const;

  // A PendingRequest is a resolve request that is in progress, or queued.
  struct PendingRequest {
   public:
     PendingRequest(const GURL& url, IPC::Message* reply_msg) :
         url(url), reply_msg(reply_msg), pac_req(NULL) { }

     // The URL of the request.
     GURL url;

     // Data to pass back to the delegate on completion (we own it until then).
     IPC::Message* reply_msg;

     // Handle for cancelling the current request if it has started (else NULL).
     net::ProxyService::PacRequest* pac_req;
  };

  // Members for the current outstanding proxy request.
  net::ProxyService* proxy_service_;
  net::CompletionCallbackImpl<ResolveProxyMsgHelper> callback_;
  net::ProxyInfo proxy_info_;

  // FIFO queue of pending requests. The first entry is always the current one.
  typedef std::deque<PendingRequest> PendingRequestList;
  PendingRequestList pending_requests_;

  Delegate* delegate_;

  // Specified by unit-tests, to use this proxy service in place of the
  // global one.
  net::ProxyService* proxy_service_override_;
};

#endif  // CHROME_BROWSER_NET_RESOLVE_PROXY_MSG_HELPER_

