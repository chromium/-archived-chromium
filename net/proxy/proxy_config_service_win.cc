// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config_service_win.h"

#include <windows.h>
#include <winhttp.h>

#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_config.h"

#pragma comment(lib, "winhttp.lib")

namespace net {

static void FreeConfig(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* config) {
  if (config->lpszAutoConfigUrl)
    GlobalFree(config->lpszAutoConfigUrl);
  if (config->lpszProxy)
    GlobalFree(config->lpszProxy);
  if (config->lpszProxyBypass)
    GlobalFree(config->lpszProxyBypass);
}

int ProxyConfigServiceWin::GetProxyConfig(ProxyConfig* config) {
  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_config = {0};
  if (!WinHttpGetIEProxyConfigForCurrentUser(&ie_config)) {
    LOG(ERROR) << "WinHttpGetIEProxyConfigForCurrentUser failed: " <<
        GetLastError();
    return ERR_FAILED;  // TODO(darin): Bug 1189288: translate error code.
  }

  if (ie_config.fAutoDetect)
    config->auto_detect = true;
  if (ie_config.lpszProxy)
    config->proxy_rules = WideToASCII(ie_config.lpszProxy);
  if (ie_config.lpszProxyBypass) {
    std::string proxy_bypass = WideToASCII(ie_config.lpszProxyBypass);

    StringTokenizer proxy_server_bypass_list(proxy_bypass, "; \t\n\r");
    while (proxy_server_bypass_list.GetNext()) {
      std::string bypass_url_domain = proxy_server_bypass_list.token();
      if (bypass_url_domain == "<local>")
        config->proxy_bypass_local_names = true;
      else
        config->proxy_bypass.push_back(bypass_url_domain);
    }
  }
  if (ie_config.lpszAutoConfigUrl)
    config->pac_url = GURL(ie_config.lpszAutoConfigUrl);

  FreeConfig(&ie_config);
  return OK;
}

}  // namespace net

