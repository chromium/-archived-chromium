// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "net/http/http_proxy_service.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>

#include "base/message_loop.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"

namespace net {

// HttpProxyConfig ------------------------------------------------------------

// static
HttpProxyConfig::ID HttpProxyConfig::last_id_ = HttpProxyConfig::INVALID_ID;

HttpProxyConfig::HttpProxyConfig()
    : auto_detect(false),
      id_(++last_id_) {
}

bool HttpProxyConfig::Equals(const HttpProxyConfig& other) const {
  // The two configs can have different IDs.  We are just interested in if they
  // have the same settings.
  return auto_detect == other.auto_detect &&
         pac_url == other.pac_url &&
         proxy_server == other.proxy_server &&
         proxy_bypass == other.proxy_bypass;
}

// HttpProxyList --------------------------------------------------------------
void HttpProxyList::SetVector(const std::vector<std::wstring>& proxies) {
  proxies_.clear();
  std::vector<std::wstring>::const_iterator iter = proxies.begin();
  for (; iter != proxies.end(); ++iter) {
    std::wstring proxy_sever;
    TrimWhitespace(*iter, TRIM_ALL, &proxy_sever);
    proxies_.push_back(proxy_sever);
  }
}

void HttpProxyList::Set(const std::wstring& proxy_list) {
  // Extract the different proxies from the list.
  std::vector<std::wstring> proxies;
  SplitString(proxy_list, L';', &proxies);
  SetVector(proxies);
}

void HttpProxyList::RemoveBadProxies(const HttpProxyRetryInfoMap&
                                         http_proxy_retry_info) {
  std::vector<std::wstring> new_proxy_list;
  std::vector<std::wstring>::const_iterator iter = proxies_.begin();
  for (; iter != proxies_.end(); ++iter) {
    HttpProxyRetryInfoMap::const_iterator bad_proxy =
        http_proxy_retry_info.find(*iter);
    if (bad_proxy != http_proxy_retry_info.end()) {
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

std::wstring HttpProxyList::Get() const {
  if (!proxies_.empty())
    return proxies_[0];

  return std::wstring();
}

const std::vector<std::wstring>& HttpProxyList::GetVector() const {
  return proxies_;
}

std::wstring HttpProxyList::GetList() const {
  std::wstring proxy_list;
  std::vector<std::wstring>::const_iterator iter = proxies_.begin();
  for (; iter != proxies_.end(); ++iter) {
    if (!proxy_list.empty())
      proxy_list += L';';

    proxy_list += *iter;
  }

  return proxy_list;
}

bool HttpProxyList::Fallback(HttpProxyRetryInfoMap* http_proxy_retry_info) {
  // Number of minutes to wait before retrying a bad proxy server.
  const TimeDelta kProxyRetryDelay = TimeDelta::FromMinutes(5);

  if (proxies_.empty()) {
    NOTREACHED();
    return false;
  }

  // Mark this proxy as bad.
  HttpProxyRetryInfoMap::iterator iter =
      http_proxy_retry_info->find(proxies_[0]);
  if (iter != http_proxy_retry_info->end()) {
    // TODO(nsylvain): This is not the first time we get this. We should
    // double the retry time. Bug 997660.
    iter->second.bad_until = TimeTicks::Now() + iter->second.current_delay;
  } else {
    HttpProxyRetryInfo retry_info;
    retry_info.current_delay = kProxyRetryDelay;
    retry_info.bad_until = TimeTicks().Now() + retry_info.current_delay;
    (*http_proxy_retry_info)[proxies_[0]] = retry_info;
  }

  // Remove this proxy from our list.
  proxies_.erase(proxies_.begin());

  return !proxies_.empty();
}

// HttpProxyInfo --------------------------------------------------------------

HttpProxyInfo::HttpProxyInfo()
    : config_id_(HttpProxyConfig::INVALID_ID),
      config_was_tried_(false) {
}

void HttpProxyInfo::Use(const HttpProxyInfo& other) {
  proxy_list_.SetVector(other.proxy_list_.GetVector());
}

void HttpProxyInfo::UseDirect() {
  proxy_list_.Set(std::wstring());
}

void HttpProxyInfo::UseNamedProxy(const std::wstring& proxy_server) {
  proxy_list_.Set(proxy_server);
}

void HttpProxyInfo::Apply(HINTERNET request_handle) {
  WINHTTP_PROXY_INFO pi;
  std::wstring proxy;  // We need to declare this variable here because
                       // lpszProxy needs to be valid in WinHttpSetOption.
  if (is_direct()) {
    pi.dwAccessType = WINHTTP_ACCESS_TYPE_NO_PROXY;
    pi.lpszProxy = WINHTTP_NO_PROXY_NAME;
    pi.lpszProxyBypass = WINHTTP_NO_PROXY_BYPASS;
  } else {
    proxy = proxy_list_.Get();
    pi.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    pi.lpszProxy = const_cast<LPWSTR>(proxy.c_str());
    // NOTE: Specifying a bypass list here would serve no purpose.
    pi.lpszProxyBypass = WINHTTP_NO_PROXY_BYPASS;
  }
  WinHttpSetOption(request_handle, WINHTTP_OPTION_PROXY, &pi, sizeof(pi));
}

// HttpProxyService::PacRequest -----------------------------------------------

// We rely on the fact that the origin thread (and its message loop) will not
// be destroyed until after the PAC thread is destroyed.

class HttpProxyService::PacRequest :
    public base::RefCountedThreadSafe<HttpProxyService::PacRequest> {
 public:
  PacRequest(HttpProxyService* service,
             const std::wstring& pac_url,
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

  void Query(const std::wstring& url, HttpProxyInfo* results) {
    results_ = results;
    // If we have a valid callback then execute Query asynchronously
    if (callback_) {
      AddRef();  // balanced in QueryComplete
      service_->pac_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &HttpProxyService::PacRequest::DoQuery,
                            service_->resolver(),
                            url,
                            pac_url_));
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
  void DoQuery(HttpProxyResolver* resolver,
               const std::wstring& query_url,
               const std::wstring& pac_url) {
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
      results_->RemoveBadProxies(service_->http_proxy_retry_info_);
    }

    if (callback_)
      callback_->Run(result_code);

    if (origin_loop_) {
      Release();  // balances the AddRef in Query.  we may get deleted after
                  // we return.
    }
  }

  // Must only be used on the "origin" thread.
  HttpProxyService* service_;
  CompletionCallback* callback_;
  HttpProxyInfo* results_;
  HttpProxyConfig::ID config_id_;

  // Usable from within DoQuery on the PAC thread.
  HttpProxyInfo results_buf_;
  std::wstring pac_url_;
  MessageLoop* origin_loop_;
};

// HttpProxyService -----------------------------------------------------------

HttpProxyService::HttpProxyService(HttpProxyResolver* resolver)
    : resolver_(resolver),
      config_is_bad_(false) {
  UpdateConfig();
}

int HttpProxyService::ResolveProxy(const GURL& url, HttpProxyInfo* result,
                                   CompletionCallback* callback,
                                   PacRequest** pac_request) {
  // The overhead of calling WinHttpGetIEProxyConfigForCurrentUser is very low.
  const TimeDelta kProxyConfigMaxAge = TimeDelta::FromSeconds(5);

  // Periodically check for a new config.
  if ((TimeTicks::Now() - config_last_update_time_) > kProxyConfigMaxAge)
    UpdateConfig();
  result->config_id_ = config_.id();

  // Fallback to a "direct" (no proxy) connection if the current configuration
  // is known to be bad.
  if (config_is_bad_) {
    // Reset this flag to false in case the HttpProxyInfo object is being
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
        std::wstring url_scheme = ASCIIToWide(url.scheme());

        WStringTokenizer proxy_server_list(config_.proxy_server, L";");
        while (proxy_server_list.GetNext()) {
          WStringTokenizer proxy_server_for_scheme(
              proxy_server_list.token_begin(), proxy_server_list.token_end(),
              L"=");

          while (proxy_server_for_scheme.GetNext()) {
            const std::wstring& proxy_server_scheme =
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

    if (!config_.pac_url.empty() || config_.auto_detect) {
      if (callback) {
        // Create PAC thread for asynchronous mode.
        if (!pac_thread_.get()) {
          pac_thread_.reset(new Thread("pac-thread"));
          pac_thread_->Start();
        }
      } else {
        // If this request is synchronous, then there's no point
        // in returning PacRequest instance
        DCHECK(!pac_request);
      }

      scoped_refptr<PacRequest> req =
          new PacRequest(this, config_.pac_url, callback);
      req->Query(UTF8ToWide(url.spec()), result);

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

int HttpProxyService::ReconsiderProxyAfterError(const GURL& url,
                                                HttpProxyInfo* result,
                                                CompletionCallback* callback,
                                                PacRequest** pac_request) {
  bool was_direct = result->is_direct();
  if (!was_direct && result->Fallback(&http_proxy_retry_info_))
    return OK;

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
  if (re_resolve)
    return ResolveProxy(url, result, callback, pac_request);

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

void HttpProxyService::CancelPacRequest(PacRequest* pac_request) {
  pac_request->Cancel();
}

void HttpProxyService::DidCompletePacRequest(int config_id, int result_code) {
  // If we get an error that indicates a bad PAC config, then we should
  // remember that, and not try the PAC config again for a while.

  // Our config may have already changed.
  if (result_code == OK || config_id != config_.id())
    return;

  // Remember that this configuration doesn't work.
  config_is_bad_ = true;
}

void HttpProxyService::UpdateConfig() {
  HttpProxyConfig latest;
  if (resolver_->GetProxyConfig(&latest) != OK)
    return;
  config_last_update_time_ = TimeTicks::Now();

  if (latest.Equals(config_))
    return;

  config_ = latest;
  config_is_bad_ = false;
}

bool HttpProxyService::ShouldBypassProxyForURL(const GURL& url) {
  std::wstring url_domain = ASCIIToWide(url.scheme());
  if (!url_domain.empty())
    url_domain += L"://";

  url_domain += ASCIIToWide(url.host());
  StringToLowerASCII(url_domain);

  WStringTokenizer proxy_server_bypass_list(config_.proxy_bypass, L";");
  while (proxy_server_bypass_list.GetNext()) {
    std::wstring bypass_url_domain = proxy_server_bypass_list.token();
    if (bypass_url_domain == L"<local>") {
      // Any name without a DOT (.) is considered to be local.
      if (url.host().find(L'.') == std::wstring::npos)
        return true;
      continue;
    }

    // The proxy server bypass list can contain entities with http/https
    // If no scheme is specified then it indicates that all schemes are
    // allowed for the current entry. For matching this we just use
    // the protocol scheme of the url passed in.
    if (bypass_url_domain.find(L"://") == std::wstring::npos) {
      std::wstring bypass_url_domain_with_scheme = ASCIIToWide(url.scheme());
      bypass_url_domain_with_scheme += L"://";
      bypass_url_domain_with_scheme += bypass_url_domain;

      bypass_url_domain = bypass_url_domain_with_scheme;
    }

    StringToLowerASCII(bypass_url_domain);

    if (MatchPattern(url_domain, bypass_url_domain))
      return true;
  }

  return false;
}

}  // namespace net
