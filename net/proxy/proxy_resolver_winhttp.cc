// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_winhttp.h"

#include <windows.h>
#include <winhttp.h>

#include "base/histogram.h"
#include "net/base/net_errors.h"

#pragma comment(lib, "winhttp.lib")

namespace net {

// A small wrapper for histogramming purposes ;-)
static BOOL CallWinHttpGetProxyForUrl(HINTERNET session, LPCWSTR url,
                                      WINHTTP_AUTOPROXY_OPTIONS* options,
                                      WINHTTP_PROXY_INFO* results) {
  TimeTicks time_start = TimeTicks::Now();
  BOOL rv = WinHttpGetProxyForUrl(session, url, options, results);
  TimeDelta time_delta = TimeTicks::Now() - time_start;
  // Record separately success and failure times since they will have very
  // different characteristics.
  if (rv) {
    UMA_HISTOGRAM_LONG_TIMES(L"Net.GetProxyForUrl_OK", time_delta);
  } else {
    UMA_HISTOGRAM_LONG_TIMES(L"Net.GetProxyForUrl_FAIL", time_delta);
  }
  return rv;
}

static void FreeConfig(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* config) {
  if (config->lpszAutoConfigUrl)
    GlobalFree(config->lpszAutoConfigUrl);
  if (config->lpszProxy)
    GlobalFree(config->lpszProxy);
  if (config->lpszProxyBypass)
    GlobalFree(config->lpszProxyBypass);
}

static void FreeInfo(WINHTTP_PROXY_INFO* info) {
  if (info->lpszProxy)
    GlobalFree(info->lpszProxy);
  if (info->lpszProxyBypass)
    GlobalFree(info->lpszProxyBypass);
}

ProxyResolverWinHttp::ProxyResolverWinHttp()
    : session_handle_(NULL) {
}

ProxyResolverWinHttp::~ProxyResolverWinHttp() {
  CloseWinHttpSession();
}

int ProxyResolverWinHttp::GetProxyConfig(ProxyConfig* config) {
  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_config = {0};
  if (!WinHttpGetIEProxyConfigForCurrentUser(&ie_config)) {
    LOG(ERROR) << "WinHttpGetIEProxyConfigForCurrentUser failed: " <<
        GetLastError();
    return ERR_FAILED;  // TODO(darin): Bug 1189288: translate error code.
  }

  if (ie_config.fAutoDetect)
    config->auto_detect = true;
  if (ie_config.lpszProxy)
    config->proxy_server = WideToASCII(ie_config.lpszProxy);
  if (ie_config.lpszProxyBypass)
    config->proxy_bypass = WideToASCII(ie_config.lpszProxyBypass);
  if (ie_config.lpszAutoConfigUrl)
    config->pac_url = WideToASCII(ie_config.lpszAutoConfigUrl);

  FreeConfig(&ie_config);
  return OK;
}

int ProxyResolverWinHttp::GetProxyForURL(const std::string& query_url,
                                         const std::string& pac_url,
                                         ProxyInfo* results) {
  // If we don't have a WinHTTP session, then create a new one.
  if (!session_handle_ && !OpenWinHttpSession())
    return ERR_FAILED;

  // If we have been given an empty PAC url, then use auto-detection.
  //
  // NOTE: We just use DNS-based auto-detection here like Firefox.  We do this
  // to avoid WinHTTP's auto-detection code, which while more featureful (it
  // supports DHCP based auto-detection) also appears to have issues.
  //
  WINHTTP_AUTOPROXY_OPTIONS options = {0};
  options.fAutoLogonIfChallenged = TRUE;
  options.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
  options.lpszAutoConfigUrl =
      pac_url.empty() ? L"http://wpad/wpad.dat" : ASCIIToWide(pac_url).c_str();

  WINHTTP_PROXY_INFO info = {0};
  DCHECK(session_handle_);
  if (!CallWinHttpGetProxyForUrl(
          session_handle_, ASCIIToWide(query_url).c_str(), &options, &info)) {
    DWORD error = GetLastError();
    LOG(ERROR) << "WinHttpGetProxyForUrl failed: " << error;

    // If we got here because of RPC timeout during out of process PAC
    // resolution, no further requests on this session are going to work.
    if ((ERROR_WINHTTP_TIMEOUT == error) ||
        (ERROR_WINHTTP_AUTO_PROXY_SERVICE_ERROR == error)) {
      CloseWinHttpSession();
    }

    return ERR_FAILED;  // TODO(darin): Bug 1189288: translate error code.
  }

  int rv = OK;

  switch (info.dwAccessType) {
    case WINHTTP_ACCESS_TYPE_NO_PROXY:
      results->UseDirect();
      break;
    case WINHTTP_ACCESS_TYPE_NAMED_PROXY:
      results->UseNamedProxy(WideToASCII(info.lpszProxy));
      break;
    default:
      NOTREACHED();
      rv = ERR_FAILED;
  }

  FreeInfo(&info);
  return rv;
}

bool ProxyResolverWinHttp::OpenWinHttpSession() {
  DCHECK(!session_handle_);
  session_handle_ = WinHttpOpen(NULL,
                                WINHTTP_ACCESS_TYPE_NO_PROXY,
                                WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS,
                                0);
  if (!session_handle_)
    return false;

  // Since this session handle will never be used for WinHTTP connections,
  // these timeouts don't really mean much individually.  However, WinHTTP's
  // out of process PAC resolution will use a combined (sum of all timeouts)
  // value to wait for an RPC reply.
  BOOL rv = WinHttpSetTimeouts(session_handle_, 10000, 10000, 5000, 5000);
  DCHECK(rv);

  return true;
}

void ProxyResolverWinHttp::CloseWinHttpSession() {
  if (session_handle_) {
    WinHttpCloseHandle(session_handle_);
    session_handle_ = NULL;
  }
}

}  // namespace net

