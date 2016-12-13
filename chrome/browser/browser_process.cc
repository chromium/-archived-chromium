// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"

#include "chrome/browser/renderer_host/resource_dispatcher_host.h"

BrowserProcess* g_browser_process = NULL;

DownloadRequestManager* BrowserProcess::download_request_manager() {
  ResourceDispatcherHost* rdh = resource_dispatcher_host();
  return rdh ? rdh->download_request_manager() : NULL;
}

