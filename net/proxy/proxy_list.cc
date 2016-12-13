// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_list.h"

#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/time.h"

using base::TimeDelta;
using base::TimeTicks;

namespace net {

void ProxyList::Set(const std::string& proxy_uri_list) {
  proxies_.clear();
  StringTokenizer str_tok(proxy_uri_list, ";");
  while (str_tok.GetNext()) {
    ProxyServer uri = ProxyServer::FromURI(
        str_tok.token_begin(), str_tok.token_end());
    // Silently discard malformed inputs.
    if (uri.is_valid())
      proxies_.push_back(uri);
  }
}

void ProxyList::SetSingleProxyServer(const ProxyServer& proxy_server) {
  proxies_.clear();
  if (proxy_server.is_valid())
    proxies_.push_back(proxy_server);
}

void ProxyList::RemoveBadProxies(const ProxyRetryInfoMap& proxy_retry_info) {
  std::vector<ProxyServer> new_proxy_list;
  std::vector<ProxyServer>::const_iterator iter = proxies_.begin();
  for (; iter != proxies_.end(); ++iter) {
    ProxyRetryInfoMap::const_iterator bad_proxy =
        proxy_retry_info.find(iter->ToURI());
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

void ProxyList::RemoveProxiesWithoutScheme(int scheme_bit_field) {
  for (std::vector<ProxyServer>::iterator it = proxies_.begin();
       it != proxies_.end(); ) {
    if (!(scheme_bit_field & it->scheme())) {
      it = proxies_.erase(it);
      continue;
    }
    ++it;
  }
}

ProxyServer ProxyList::Get() const {
  if (!proxies_.empty())
    return proxies_[0];
  return ProxyServer(ProxyServer::SCHEME_DIRECT, std::string(), -1);
}

std::string ProxyList::ToPacString() const {
  std::string proxy_list;
  std::vector<ProxyServer>::const_iterator iter = proxies_.begin();
  for (; iter != proxies_.end(); ++iter) {
    if (!proxy_list.empty())
      proxy_list += ";";
    proxy_list += iter->ToPacString();
  }
  return proxy_list.empty() ? "DIRECT" : proxy_list;
}

void ProxyList::SetFromPacString(const std::string& pac_string) {
  StringTokenizer entry_tok(pac_string, ";");
  proxies_.clear();
  while (entry_tok.GetNext()) {
    ProxyServer uri = ProxyServer::FromPacString(
        entry_tok.token_begin(), entry_tok.token_end());
    // Silently discard malformed inputs.
    if (uri.is_valid())
      proxies_.push_back(uri);
  }
}

bool ProxyList::Fallback(ProxyRetryInfoMap* proxy_retry_info) {
  // Number of minutes to wait before retrying a bad proxy server.
  const TimeDelta kProxyRetryDelay = TimeDelta::FromMinutes(5);

  if (proxies_.empty()) {
    NOTREACHED();
    return false;
  }

  std::string key = proxies_[0].ToURI();

  // Mark this proxy as bad.
  ProxyRetryInfoMap::iterator iter = proxy_retry_info->find(key);
  if (iter != proxy_retry_info->end()) {
    // TODO(nsylvain): This is not the first time we get this. We should
    // double the retry time. Bug 997660.
    iter->second.bad_until = TimeTicks::Now() + iter->second.current_delay;
  } else {
    ProxyRetryInfo retry_info;
    retry_info.current_delay = kProxyRetryDelay;
    retry_info.bad_until = TimeTicks().Now() + retry_info.current_delay;
    (*proxy_retry_info)[key] = retry_info;
  }

  // Remove this proxy from our list.
  proxies_.erase(proxies_.begin());

  return !proxies_.empty();
}

}  // namespace net
