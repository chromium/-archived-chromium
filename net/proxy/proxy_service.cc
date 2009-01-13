// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_service.h"

#if defined(OS_WIN)
#include <windows.h>
#include <winhttp.h>
#endif

#include <algorithm>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_config_service_fixed.h"
#if defined(OS_WIN)
#include "net/proxy/proxy_config_service_win.h"
#include "net/proxy/proxy_resolver_winhttp.h"
#elif defined(OS_MACOSX)
#include "net/proxy/proxy_resolver_mac.h"
#endif

using base::TimeDelta;
using base::TimeTicks;

namespace net {

// Config getter that fails every time.
class ProxyConfigServiceNull : public ProxyConfigService {
 public:
  virtual int GetProxyConfig(ProxyConfig* config) {
    return ERR_NOT_IMPLEMENTED;
  }
};


// ProxyConfig ----------------------------------------------------------------

// static
ProxyConfig::ID ProxyConfig::last_id_ = ProxyConfig::INVALID_ID;

ProxyConfig::ProxyConfig()
    : auto_detect(false),
      proxy_bypass_local_names(false),
      id_(++last_id_) {
}

bool ProxyConfig::Equals(const ProxyConfig& other) const {
  // The two configs can have different IDs.  We are just interested in if they
  // have the same settings.
  return auto_detect == other.auto_detect &&
         pac_url == other.pac_url &&
         proxy_server == other.proxy_server &&
         proxy_bypass == other.proxy_bypass &&
         proxy_bypass_local_names == other.proxy_bypass_local_names;
}

// ProxyList ------------------------------------------------------------------
void ProxyList::SetVector(const std::vector<std::string>& proxies) {
  proxies_.clear();
  std::vector<std::string>::const_iterator iter = proxies.begin();
  for (; iter != proxies.end(); ++iter) {
    std::string proxy_sever;
    TrimWhitespace(*iter, TRIM_ALL, &proxy_sever);
    proxies_.push_back(proxy_sever);
  }
}

void ProxyList::Set(const std::string& proxy_list) {
  // Extract the different proxies from the list.
  std::vector<std::string> proxies;
  SplitString(proxy_list, L';', &proxies);
  SetVector(proxies);
}

void ProxyList::RemoveBadProxies(const ProxyRetryInfoMap& proxy_retry_info) {
  std::vector<std::string> new_proxy_list;
  std::vector<std::string>::const_iterator iter = proxies_.begin();
  for (; iter != proxies_.end(); ++iter) {
    ProxyRetryInfoMap::const_iterator bad_proxy =
        proxy_retry_info.find(*iter);
    if (bad_proxy != proxy_retry_info.end()) {
      // This proxy is bad. Check if it's time to retry.
      if (bad_proxy->second.bad_until >= TimeTicks::Now()) {
        // still invalid.
        continue;
      }
    }
    new_proxy_list.push_back(*iter);
  }

  proxies_ = new_proxy_list;
}

std::string ProxyList::Get() const {
  if (!proxies_.empty())
    return proxies_[0];

  return std::string();
}

std::string ProxyList::GetAnnotatedList() const {
  std::string proxy_list;
  std::vector<std::string>::const_iterator iter = proxies_.begin();
  for (; iter != proxies_.end(); ++iter) {
    if (!proxy_list.empty())
      proxy_list += ";";
    // Assume every proxy is an HTTP proxy, as that is all we currently support.
    proxy_list += "PROXY ";
    proxy_list += *iter;
  }

  return proxy_list;
}

bool ProxyList::Fallback(ProxyRetryInfoMap* proxy_retry_info) {
  // Number of minutes to wait before retrying a bad proxy server.
  const TimeDelta kProxyRetryDelay = TimeDelta::FromMinutes(5);

  if (proxies_.empty()) {
    NOTREACHED();
    return false;
  }

  // Mark this proxy as bad.
  ProxyRetryInfoMap::iterator iter = proxy_retry_info->find(proxies_[0]);
  if (iter != proxy_retry_info->end()) {
    // TODO(nsylvain): This is not the first time we get this. We should
    // double the retry time. Bug 997660.
    iter->second.bad_until = TimeTicks::Now() + iter->second.current_delay;
  } else {
    ProxyRetryInfo retry_info;
    retry_info.current_delay = kProxyRetryDelay;
    retry_info.bad_until = TimeTicks().Now() + retry_info.current_delay;
    (*proxy_retry_info)[proxies_[0]] = retry_info;
  }

  // Remove this proxy from our list.
  proxies_.erase(proxies_.begin());

  return !proxies_.empty();
}

// ProxyInfo ------------------------------------------------------------------

ProxyInfo::ProxyInfo()
    : config_id_(ProxyConfig::INVALID_ID),
      config_was_tried_(false) {
}

void ProxyInfo::Use(const ProxyInfo& other) {
  proxy_list_ = other.proxy_list_;
}

void ProxyInfo::UseDirect() {
  proxy_list_.Set(std::string());
}

void ProxyInfo::UseNamedProxy(const std::string& proxy_server) {
  proxy_list_.Set(proxy_server);
}

#if defined(OS_WIN)
void ProxyInfo::Apply(HINTERNET request_handle) {
  WINHTTP_PROXY_INFO pi;
  std::wstring proxy;  // We need to declare this variable here because
                       // lpszProxy needs to be valid in WinHttpSetOption.
  if (is_direct()) {
    pi.dwAccessType = WINHTTP_ACCESS_TYPE_NO_PROXY;
    pi.lpszProxy = WINHTTP_NO_PROXY_NAME;
    pi.lpszProxyBypass = WINHTTP_NO_PROXY_BYPASS;
  } else {
    proxy = ASCIIToWide(proxy_list_.Get());
    pi.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    pi.lpszProxy = const_cast<LPWSTR>(proxy.c_str());
    // NOTE: Specifying a bypass list here would serve no purpose.
    pi.lpszProxyBypass = WINHTTP_NO_PROXY_BYPASS;
  }
  WinHttpSetOption(request_handle, WINHTTP_OPTION_PROXY, &pi, sizeof(pi));
}
#endif

std::string ProxyInfo::GetAnnotatedProxyList() {
  return is_direct() ? "DIRECT" : proxy_list_.GetAnnotatedList();
}

// ProxyService::PacRequest ---------------------------------------------------

// We rely on the fact that the origin thread (and its message loop) will not
// be destroyed until after the PAC thread is destroyed.

class ProxyService::PacRequest :
    public base::RefCountedThreadSafe<ProxyService::PacRequest> {
 public:
  PacRequest(ProxyService* service,
             const GURL& pac_url,
             CompletionCallback* callback)
      : service_(service),
        callback_(callback),
        results_(NULL),
        config_id_(service->config_id()),
        pac_url_(pac_url),
        origin_loop_(NULL) {
    // We need to remember original loop if only in case of asynchronous call
    if (callback_)
      origin_loop_ = MessageLoop::current();
  }

  void Query(const GURL& url, ProxyInfo* results) {
    results_ = results;
    // If we have a valid callback then execute Query asynchronously
    if (callback_) {
      AddRef();  // balanced in QueryComplete
      service_->pac_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(this, &ProxyService::PacRequest::DoQuery,
                            service_->resolver(), url, pac_url_));
    } else {
      DoQuery(service_->resolver(), url, pac_url_);
    }
  }

  void Cancel() {
    // Clear these to inform QueryComplete that it should not try to
    // access them.
    service_ = NULL;
    callback_ = NULL;
    results_ = NULL;
  }

 private:
  // Runs on the PAC thread if a valid callback is provided.
  void DoQuery(ProxyResolver* resolver,
               const GURL& query_url,
               const GURL& pac_url) {
    int rv = resolver->GetProxyForURL(query_url, pac_url, &results_buf_);
    if (origin_loop_) {
      origin_loop_->PostTask(FROM_HERE,
          NewRunnableMethod(this, &PacRequest::QueryComplete, rv));
    } else {
      QueryComplete(rv);
    }
  }

  // If a valid callback is provided, this runs on the origin thread to
  // indicate that the completion callback should be run.
  void QueryComplete(int result_code) {
    if (service_)
      service_->DidCompletePacRequest(config_id_, result_code);

    if (result_code == OK && results_) {
      results_->Use(results_buf_);
      results_->RemoveBadProxies(service_->proxy_retry_info_);
    }

    if (callback_)
      callback_->Run(result_code);

    if (origin_loop_) {
      Release();  // balances the AddRef in Query.  we may get deleted after
                  // we return.
    }
  }

  // Must only be used on the "origin" thread.
  ProxyService* service_;
  CompletionCallback* callback_;
  ProxyInfo* results_;
  ProxyConfig::ID config_id_;

  // Usable from within DoQuery on the PAC thread.
  ProxyInfo results_buf_;
  GURL pac_url_;
  MessageLoop* origin_loop_;
};

// ProxyService ---------------------------------------------------------------

ProxyService::ProxyService(ProxyConfigService* config_service,
                           ProxyResolver* resolver)
    : config_service_(config_service),
      resolver_(resolver),
      config_is_bad_(false),
      config_has_been_updated_(false) {
}

// static
ProxyService* ProxyService::Create(const ProxyInfo* pi) {
  if (pi) {
    // The ProxyResolver is set to NULL, since it should never be called
    // (because the configuration will never require PAC).
    ProxyService* proxy_service =
        new ProxyService(new ProxyConfigServiceFixed(*pi), NULL);

    // TODO(eroman): remove this WinHTTP hack once it is no more.
    // We keep a copy of the ProxyInfo that was used to create the
    // proxy service, so we can pass it to WinHTTP.
    proxy_service->proxy_info_.reset(new ProxyInfo(*pi));

    return proxy_service;
  }
#if defined(OS_WIN)
  return new ProxyService(new ProxyConfigServiceWin(),
                          new ProxyResolverWinHttp());
#elif defined(OS_MACOSX)
  return new ProxyService(new ProxyConfigServiceMac(),
                          new ProxyResolverMac());
#else
  // This used to be a NOTIMPLEMENTED(), but that logs as an error,
  // screwing up layout tests.
  LOG(WARNING) << "Proxies are not implemented; remove me once that's fixed.";
  // http://code.google.com/p/chromium/issues/detail?id=4523 is the bug
  // to implement this.
  return CreateNull();
#endif
}

// static
ProxyService* ProxyService::CreateNull() {
  // The ProxyResolver is set to NULL, since it should never be called
  // (because the configuration will never require PAC).
  return new ProxyService(new ProxyConfigServiceNull, NULL);
}

int ProxyService::ResolveProxy(const GURL& url, ProxyInfo* result,
                               CompletionCallback* callback,
                               PacRequest** pac_request) {
  // The overhead of calling ProxyConfigService::GetProxyConfig is very low.
  const TimeDelta kProxyConfigMaxAge = TimeDelta::FromSeconds(5);

  // Periodically check for a new config.
  if (!config_has_been_updated_ ||
      (TimeTicks::Now() - config_last_update_time_) > kProxyConfigMaxAge)
    UpdateConfig();
  result->config_id_ = config_.id();

  // Fallback to a "direct" (no proxy) connection if the current configuration
  // is known to be bad.
  if (config_is_bad_) {
    // Reset this flag to false in case the ProxyInfo object is being
    // re-used by the caller.
    result->config_was_tried_ = false;
  } else {
    // Remember that we are trying to use the current proxy configuration.
    result->config_was_tried_ = true;

    if (!config_.proxy_server.empty()) {
      if (ShouldBypassProxyForURL(url)) {
        result->UseDirect();
      } else {
        // If proxies are specified on a per protocol basis, the proxy server
        // field contains a list the format of which is as below:-
        // "scheme1=url:port;scheme2=url:port", etc.
        std::string url_scheme = url.scheme();

        StringTokenizer proxy_server_list(config_.proxy_server, ";");
        while (proxy_server_list.GetNext()) {
          StringTokenizer proxy_server_for_scheme(
              proxy_server_list.token_begin(), proxy_server_list.token_end(),
              "=");

          while (proxy_server_for_scheme.GetNext()) {
            const std::string& proxy_server_scheme =
                proxy_server_for_scheme.token();

            // If we fail to get the proxy server here, it means that
            // this is a regular proxy server configuration, i.e. proxies
            // are not configured per protocol.
            if (!proxy_server_for_scheme.GetNext()) {
              result->UseNamedProxy(proxy_server_scheme);
              return OK;
            }

            if (proxy_server_scheme == url_scheme) {
              result->UseNamedProxy(proxy_server_for_scheme.token());
              return OK;
            }
          }
        }
        // We failed to find a matching proxy server for the current URL
        // scheme. Default to direct.
        result->UseDirect();
      }
      return OK;
    }

    if (config_.pac_url.is_valid() || config_.auto_detect) {
      if (callback) {
        // Create PAC thread for asynchronous mode.
        if (!pac_thread_.get()) {
          pac_thread_.reset(new base::Thread("pac-thread"));
          pac_thread_->Start();
        }
      } else {
        // If this request is synchronous, then there's no point
        // in returning PacRequest instance
        DCHECK(!pac_request);
      }

      scoped_refptr<PacRequest> req =
          new PacRequest(this, config_.pac_url, callback);

      // Strip away any reference fragments and the username/password, as they
      // are not relevant to proxy resolution.
      GURL sanitized_url;
      { // TODO(eroman): The following duplicates logic from
        // HttpUtil::SpecForRequest. Should probably live in net_util.h
        GURL::Replacements replacements;
        replacements.ClearUsername();
        replacements.ClearPassword();
        replacements.ClearRef();
        sanitized_url = url.ReplaceComponents(replacements);
      }

      req->Query(sanitized_url, result);

      if (callback) {
        if (pac_request)
          *pac_request = req;
        return ERR_IO_PENDING;  // Wait for callback.
      }
      return OK;
    }
  }

  // otherwise, we have no proxy config
  result->UseDirect();
  return OK;
}

int ProxyService::ReconsiderProxyAfterError(const GURL& url,
                                            ProxyInfo* result,
                                            CompletionCallback* callback,
                                            PacRequest** pac_request) {
  // Check to see if we have a new config since ResolveProxy was called.  We
  // want to re-run ResolveProxy in two cases: 1) we have a new config, or 2) a
  // direct connection failed and we never tried the current config.

  bool re_resolve = result->config_id_ != config_.id();
  if (!re_resolve) {
    UpdateConfig();
    if (result->config_id_ != config_.id()) {
      // A new configuration!
      re_resolve = true;
    } else if (!result->config_was_tried_) {
      // We never tried the proxy configuration since we thought it was bad,
      // but because we failed to establish a connection, let's try the proxy
      // configuration again to see if it will work now.
      config_is_bad_ = false;
      re_resolve = true;
    }
  }
  if (re_resolve) {
    // If we have a new config or the config was never tried, we delete the
    // list of bad proxies and we try again.
    proxy_retry_info_.clear();
    return ResolveProxy(url, result, callback, pac_request);
  }

  // We don't have new proxy settings to try, fallback to the next proxy
  // in the list.
  bool was_direct = result->is_direct();
  if (!was_direct && result->Fallback(&proxy_retry_info_))
    return OK;

  if (!config_.auto_detect && !config_.proxy_server.empty()) {
    // If auto detect is on, then we should try a DIRECT connection
    // as the attempt to reach the proxy failed.
    return ERR_FAILED;
  }

  // If we already tried a direct connection, then just give up.
  if (was_direct)
    return ERR_FAILED;

  // Try going direct.
  result->UseDirect();
  return OK;
}

void ProxyService::CancelPacRequest(PacRequest* pac_request) {
  pac_request->Cancel();
}

void ProxyService::DidCompletePacRequest(int config_id, int result_code) {
  // If we get an error that indicates a bad PAC config, then we should
  // remember that, and not try the PAC config again for a while.

  // Our config may have already changed.
  if (result_code == OK || config_id != config_.id())
    return;

  // Remember that this configuration doesn't work.
  config_is_bad_ = true;
}

void ProxyService::UpdateConfig() {
  config_has_been_updated_ = true;

  ProxyConfig latest;
  if (config_service_->GetProxyConfig(&latest) != OK)
    return;
  config_last_update_time_ = TimeTicks::Now();

  if (latest.Equals(config_))
    return;

  config_ = latest;
  config_is_bad_ = false;

  // We have a new config, we should clear the list of bad proxies.
  proxy_retry_info_.clear();
}

bool ProxyService::ShouldBypassProxyForURL(const GURL& url) {
  std::string url_domain = url.scheme();
  if (!url_domain.empty())
    url_domain += "://";

  url_domain += url.host();
  // This isn't superfluous; GURL case canonicalization doesn't hit the embedded
  // percent-encoded characters.
  StringToLowerASCII(&url_domain);

  if (config_.proxy_bypass_local_names) {
    if (url.host().find('.') == std::string::npos)
      return true;
  }
  
  for(std::vector<std::string>::const_iterator i = config_.proxy_bypass.begin();
      i != config_.proxy_bypass.end(); ++i) {
    std::string bypass_url_domain = *i;

    // The proxy server bypass list can contain entities with http/https
    // If no scheme is specified then it indicates that all schemes are
    // allowed for the current entry. For matching this we just use
    // the protocol scheme of the url passed in.
    if (bypass_url_domain.find("://") == std::string::npos) {
      std::string bypass_url_domain_with_scheme = url.scheme();
      bypass_url_domain_with_scheme += "://";
      bypass_url_domain_with_scheme += bypass_url_domain;

      bypass_url_domain = bypass_url_domain_with_scheme;
    }

    StringToLowerASCII(&bypass_url_domain);

    if (MatchPattern(url_domain, bypass_url_domain))
      return true;
      
    // Some systems (the Mac, for example) allow CIDR-style specification of
    // proxy bypass for IP-specified hosts (e.g.  "10.0.0.0/8"; see
    // http://www.tcd.ie/iss/internet/osx_proxy.php for a real-world example).
    // That's kinda cool so we'll provide that for everyone.
    // TODO(avi): implement here
  }

  return false;
}

}  // namespace net

