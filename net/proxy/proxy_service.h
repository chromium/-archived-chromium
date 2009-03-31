// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_SERVICE_H_
#define NET_PROXY_PROXY_SERVICE_H_

#include <deque>
#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "net/base/completion_callback.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_info.h"

class GURL;
class URLRequestContext;

namespace net {

class ProxyScriptFetcher;
class ProxyConfigService;
class ProxyResolver;

// This class can be used to resolve the proxy server to use when loading a
// HTTP(S) URL.  It uses the given ProxyResolver to handle the actual proxy
// resolution.  See ProxyResolverWinHttp for example.
class ProxyService {
 public:
  // The instance takes ownership of |config_service| and |resolver|.
  ProxyService(ProxyConfigService* config_service,
               ProxyResolver* resolver);

  ~ProxyService();

  // Used internally to handle PAC queries.
  class PacRequest;

  // Returns ERR_IO_PENDING if the proxy information could not be provided
  // synchronously, to indicate that the result will be available when the
  // callback is run.  The callback is run on the thread that calls
  // ResolveProxy.
  //
  // The caller is responsible for ensuring that |results| and |callback|
  // remain valid until the callback is run or until |pac_request| is cancelled
  // via CancelPacRequest.  |pac_request| is only valid while the completion
  // callback is still pending. NULL can be passed for |pac_request| if
  // the caller will not need to cancel the request.
  //
  // We use the three possible proxy access types in the following order, and
  // we only use one of them (no falling back to other access types if the
  // chosen one doesn't work).
  //   1.  named proxy
  //   2.  PAC URL
  //   3.  WPAD auto-detection
  //
  int ResolveProxy(const GURL& url,
                   ProxyInfo* results,
                   CompletionCallback* callback,
                   PacRequest** pac_request);

  // This method is called after a failure to connect or resolve a host name.
  // It gives the proxy service an opportunity to reconsider the proxy to use.
  // The |results| parameter contains the results returned by an earlier call
  // to ResolveProxy.  The semantics of this call are otherwise similar to
  // ResolveProxy.
  //
  // NULL can be passed for |pac_request| if the caller will not need to
  // cancel the request.
  //
  // Returns ERR_FAILED if there is not another proxy config to try.
  //
  int ReconsiderProxyAfterError(const GURL& url,
                                ProxyInfo* results,
                                CompletionCallback* callback,
                                PacRequest** pac_request);

  // Call this method with a non-null |pac_request| to cancel the PAC request.
  void CancelPacRequest(PacRequest* pac_request);

  // Sets the ProxyScriptFetcher dependency. This is needed if the ProxyResolver
  // is of type ProxyResolverWithoutFetch. ProxyService takes ownership of
  // |proxy_script_fetcher|.
  void SetProxyScriptFetcher(ProxyScriptFetcher* proxy_script_fetcher);

  // Creates a proxy service using the specified settings. If |pi| is NULL then
  // the system's default proxy settings will be used (on Windows this will
  // use IE's settings).
  static ProxyService* Create(const ProxyInfo* pi);

  // Creates a proxy service using the specified settings. If |pi| is NULL then
  // the system's default proxy settings will be used. This is basically the
  // same as Create(const ProxyInfo*), however under the hood it uses a
  // different implementation (V8). |url_request_context| is the URL request
  // context that will be used if a PAC script needs to be fetched.
  // ##########################################################################
  // # See the warnings in net/proxy/proxy_resolver_v8.h describing the
  // # multi-threading model. In order for this to be safe to use, *ALL* the
  // # other V8's running in the process must use v8::Locker.
  // ##########################################################################
  static ProxyService* CreateUsingV8Resolver(
      const ProxyInfo* pi,
      URLRequestContext* url_request_context);

  // Creates a proxy service that always fails to fetch the proxy configuration,
  // so it falls back to direct connect.
  static ProxyService* CreateNull();

 private:
  friend class PacRequest;

  ProxyResolver* resolver() { return resolver_.get(); }
  base::Thread* pac_thread() { return pac_thread_.get(); }

  // Identifies the proxy configuration.
  ProxyConfig::ID config_id() const { return config_.id(); }

  // Checks to see if the proxy configuration changed, and then updates config_
  // to reference the new configuration.
  void UpdateConfig();

  // Tries to update the configuration if it hasn't been checked in a while.
  void UpdateConfigIfOld();

  // Returns true if this ProxyService is downloading a PAC script on behalf
  // of ProxyResolverWithoutFetch. Resolve requests will be frozen until
  // the fetch has completed.
  bool IsFetchingPacScript() const {
    return in_progress_fetch_config_id_ != ProxyConfig::INVALID_ID;
  }

  // Callback for when the PAC script has finished downloading.
  void OnScriptFetchCompletion(int result);

  // Returns ERR_IO_PENDING if the request cannot be completed synchronously.
  // Otherwise it fills |result| with the proxy information for |url|.
  // Completing synchronously means we don't need to query ProxyResolver.
  // (ProxyResolver runs on PAC thread.)
  int TryToCompleteSynchronously(const GURL& url, ProxyInfo* result);

  // Set |result| with the proxy to use for |url|, based on |rules|.
  void ApplyProxyRules(const GURL& url,
                       const ProxyConfig::ProxyRules& rules,
                       ProxyInfo* result);

  // Starts the PAC thread if it isn't already running.
  void InitPacThread();

  // Starts the next request from |pending_requests_| is possible.
  // |recent_req| is the request that just got added, or NULL.
  void ProcessPendingRequests(PacRequest* recent_req);

  // Removes the front entry of the requests queue. |expected_req| is our
  // expectation of what the front of the request queue is; it is only used by
  // DCHECK for verification purposes.
  void RemoveFrontOfRequestQueue(PacRequest* expected_req);

  // Called to indicate that a PacRequest completed.  The |config_id| parameter
  // indicates the proxy configuration that was queried.  |result_code| is OK
  // if the PAC file could be downloaded and executed.  Otherwise, it is an
  // error code, indicating a bad proxy configuration.
  void DidCompletePacRequest(int config_id, int result_code);

  // Returns true if the URL passed in should not go through the proxy server.
  // 1. If the bypass proxy list contains the string <local> and the URL
  //    passed in is a local URL, i.e. a URL without a DOT (.)
  // 2. The URL matches one of the entities in the proxy bypass list.
  bool ShouldBypassProxyForURL(const GURL& url);

  scoped_ptr<ProxyConfigService> config_service_;
  scoped_ptr<ProxyResolver> resolver_;
  scoped_ptr<base::Thread> pac_thread_;

  // We store the proxy config and a counter that is incremented each time
  // the config changes.
  ProxyConfig config_;

  // Indicates that the configuration is bad and should be ignored.
  bool config_is_bad_;

  // false if the ProxyService has not been initialized yet.
  bool config_has_been_updated_;

  // The time when the proxy configuration was last read from the system.
  base::TimeTicks config_last_update_time_;

  // Map of the known bad proxies and the information about the retry time.
  ProxyRetryInfoMap proxy_retry_info_;

  // FIFO queue of pending/inprogress requests.
  typedef std::deque<scoped_refptr<PacRequest> > PendingRequestsQueue;
  PendingRequestsQueue pending_requests_;

  // The fetcher to use when downloading PAC scripts for the ProxyResolver.
  // This dependency can be NULL if our ProxyResolver has no need for
  // external PAC script fetching.
  scoped_ptr<ProxyScriptFetcher> proxy_script_fetcher_;

  // Callback for when |proxy_script_fetcher_| is done.
  CompletionCallbackImpl<ProxyService> proxy_script_fetcher_callback_;

  // The ID of the configuration for which we last downloaded a PAC script.
  // If no PAC script has been fetched yet, will be ProxyConfig::INVALID_ID.
  ProxyConfig::ID fetched_pac_config_id_;

  // The error corresponding with |fetched_pac_config_id_|, or OK.
  int fetched_pac_error_;

  // The ID of the configuration for which we are currently downloading the
  // PAC script. If no fetch is in progress, will be ProxyConfig::INVALID_ID.
  ProxyConfig::ID in_progress_fetch_config_id_;

  // The results of the current in progress fetch, or empty string.
  std::string in_progress_fetch_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ProxyService);
};

// Wrapper for invoking methods on a ProxyService synchronously.
class SyncProxyServiceHelper
    : public base::RefCountedThreadSafe<SyncProxyServiceHelper> {
 public:
  SyncProxyServiceHelper(MessageLoop* io_message_loop,
                         ProxyService* proxy_service);

  int ResolveProxy(const GURL& url, ProxyInfo* proxy_info);
  int ReconsiderProxyAfterError(const GURL& url, ProxyInfo* proxy_info);

 private:
  void StartAsyncResolve(const GURL& url);
  void StartAsyncReconsider(const GURL& url);

  void OnCompletion(int result);

  MessageLoop* io_message_loop_;
  ProxyService* proxy_service_;

  base::WaitableEvent event_;
  CompletionCallbackImpl<SyncProxyServiceHelper> callback_;
  ProxyInfo proxy_info_;
  int result_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_SERVICE_H_
