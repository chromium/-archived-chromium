// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_SERVICE_H_
#define NET_PROXY_PROXY_SERVICE_H_

#include <map>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"

#if defined(OS_WIN)
typedef LPVOID HINTERNET;  // From winhttp.h
#endif

class GURL;

namespace net {

class ProxyConfigService;
class ProxyInfo;
class ProxyResolver;

// Proxy configuration used to by the ProxyService.
class ProxyConfig {
 public:
  typedef int ID;

  // Indicates an invalid proxy config.
  enum { INVALID_ID = 0 };

  ProxyConfig();
  // Default copy-constructor and assignment operator are OK!

  // Used to numerically identify this configuration.
  ID id() const { return id_; }

  // True if the proxy configuration should be auto-detected.
  bool auto_detect;

  // If non-empty, indicates the URL of the proxy auto-config file to use.
  GURL pac_url;

  // If non-empty, indicates the proxy server to use (of the form host:port).
  // If proxies depend on the scheme, a string of the format
  // "scheme1=url[:port];scheme2=url[:port]" may be provided here.
  std::string proxy_server;

  // Indicates a list of hosts that should bypass any proxy configuration.  For
  // these hosts, a direct connection should always be used.
  std::vector<std::string> proxy_bypass;

  // Indicates whether local names (no dots) bypass proxies.
  bool proxy_bypass_local_names;

  // Returns true if the given config is equivalent to this config.
  bool Equals(const ProxyConfig& other) const;

 private:
  static int last_id_;
  int id_;
};

// Contains the information about when to retry a proxy server.
struct ProxyRetryInfo {
  // We should not retry until this time.
  base::TimeTicks bad_until;

  // This is the current delay. If the proxy is still bad, we need to increase
  // this delay.
  base::TimeDelta current_delay;
};

// Map of proxy servers with the associated RetryInfo structures.
typedef std::map<std::string, ProxyRetryInfo> ProxyRetryInfoMap;

// This class can be used to resolve the proxy server to use when loading a
// HTTP(S) URL.  It uses the given ProxyResolver to handle the actual proxy
// resolution.  See ProxyResolverWinHttp for example.
class ProxyService {
 public:
  // The instance takes ownership of |config_service| and |resolver|.
  ProxyService(ProxyConfigService* config_service,
               ProxyResolver* resolver);

  // Used internally to handle PAC queries.
  class PacRequest;

  // Returns OK if proxy information could be provided synchronously.  Else,
  // ERR_IO_PENDING is returned to indicate that the result will be available
  // when the callback is run.  The callback is run on the thread that calls
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

  // Create a proxy service using the specified settings. If |pi| is NULL then
  // the system's default proxy settings will be used (on Windows this will
  // use IE's settings).
  static ProxyService* Create(const ProxyInfo* pi);

  // Create a proxy service that always fails to fetch the proxy configuration,
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

  DISALLOW_COPY_AND_ASSIGN(ProxyService);
};

// This class is used to hold a list of proxies returned by GetProxyForUrl or
// manually configured. It handles proxy fallback if multiple servers are
// specified.
// TODO(eroman): The proxy list should work for multiple proxy types.
// See http://crbug.com/469.
class ProxyList {
 public:
  // Initializes the proxy list to a string containing one or more proxy servers
  // delimited by a semicolon.
  void Set(const std::string& proxy_list);

  // Initializes the proxy list to a vector containing one or more proxy
  // servers.
  void SetVector(const std::vector<std::string>& proxy_list);

  // Remove all proxies known to be bad from the proxy list.
  void RemoveBadProxies(const ProxyRetryInfoMap& proxy_retry_info);

  // Returns the first valid proxy server in the list.
  std::string Get() const;

  // Returns a PAC-style semicolon-separated list of valid proxy servers.
  // For example: "PROXY xxx.xxx.xxx.xxx:xx; SOCKS yyy.yyy.yyy:yy".
  // Since ProxyList is currently just used for HTTP, this will return only
  // entries of type "PROXY" or "DIRECT".
  std::string GetAnnotatedList() const;

  // Marks the current proxy server as bad and deletes it from the list.  The
  // list of known bad proxies is given by proxy_retry_info.  Returns true if
  // there is another server available in the list.
  bool Fallback(ProxyRetryInfoMap* proxy_retry_info);

 private:
  // List of proxies.
  std::vector<std::string> proxies_;
};

// This object holds proxy information returned by ResolveProxy.
class ProxyInfo {
 public:
  ProxyInfo();
  // Default copy-constructor and assignment operator are OK!

  // Use the same proxy server as the given |proxy_info|.
  void Use(const ProxyInfo& proxy_info);

  // Use a direct connection.
  void UseDirect();

  // Use a specific proxy server, of the form:  <hostname> [":" <port>]
  // This may optionally be a semi-colon delimited list of proxy servers.
  void UseNamedProxy(const std::string& proxy_server);

#if defined(OS_WIN)
  // Apply this proxy information to the given WinHTTP request handle.
  void Apply(HINTERNET request_handle);
#endif

  // Returns true if this proxy info specifies a direct connection.
  bool is_direct() const { return proxy_list_.Get().empty(); }

  // Returns the first valid proxy server.
  std::string proxy_server() const { return proxy_list_.Get(); }

  // See description in ProxyList::GetAnnotatedList().
  std::string GetAnnotatedProxyList();

  // Marks the current proxy as bad. Returns true if there is another proxy
  // available to try in proxy list_.
  bool Fallback(ProxyRetryInfoMap* proxy_retry_info) {
    return proxy_list_.Fallback(proxy_retry_info);
  }

  // Remove all proxies known to be bad from the proxy list.
  void RemoveBadProxies(const ProxyRetryInfoMap& proxy_retry_info) {
    proxy_list_.RemoveBadProxies(proxy_retry_info);
  }

 private:
  friend class ProxyService;

  // If proxy_list_ is set to empty, then a "direct" connection is indicated.
  ProxyList proxy_list_;

  // This value identifies the proxy config used to initialize this object.
  ProxyConfig::ID config_id_;

  // This flag is false when the proxy configuration was known to be bad when
  // this proxy info was initialized.  In such cases, we know that if this
  // proxy info does not yield a connection that we might want to reconsider
  // the proxy config given by config_id_.
  bool config_was_tried_;
};

// Synchronously fetch the system's proxy configuration settings. Called on
// the IO Thread.
class ProxyConfigService {
 public:
  virtual ~ProxyConfigService() {}

  // Get the proxy configuration.  Returns OK if successful or an error code if
  // otherwise.  |config| should be in its initial state when this method is
  // called.
  virtual int GetProxyConfig(ProxyConfig* config) = 0;
};

// Synchronously resolve the proxy for a URL, using a PAC script. Called on the
// PAC Thread.
class ProxyResolver {
 public:
  virtual ~ProxyResolver() {}

  // Query the proxy auto-config file (specified by |pac_url|) for the proxy to
  // use to load the given |query_url|.  Returns OK if successful or an error
  // code otherwise.
  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& pac_url,
                             ProxyInfo* results) = 0;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_SERVICE_H_

