// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/url_constants.h"

// TODO(port): Remove this header when last ifdef is removed from this file.
#include "build/build_config.h"

namespace chrome {

const char kAboutScheme[] = "about";
const char kChromeInternalScheme[] = "chrome-internal";
const char kChromeUIScheme[] = "chrome-ui";
const char kDataScheme[] = "data";
const char kExtensionScheme[] = "chrome-extension";
const char kFileScheme[] = "file";
const char kFtpScheme[] = "ftp";
const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kJavaScriptScheme[] = "javascript";
const char kMailToScheme[] = "mailto";
const char kUserScriptScheme[] = "chrome-user-script";
const char kViewCacheScheme[] = "view-cache";
const char kViewSourceScheme[] = "view-source";

const char kStandardSchemeSeparator[] = "://";

const char kAboutBlankURL[] = "about:blank";
const char kAboutCacheURL[] = "about:cache";
const char kAboutMemoryURL[] = "about:memory";

const char kChromeUIDevToolsURL[] = "chrome-ui://devtools/";
const char kChromeUIDownloadsURL[] = "chrome-ui://downloads/";
const char kChromeUIExtensionsURL[] = "chrome-ui://extensions/";
const char kChromeUIHistoryURL[] = "chrome-ui://history/";
const char kChromeUIInspectorURL[] = "chrome-ui://inspector/";
const char kChromeUIIPCURL[] = "chrome-ui://about/ipc";
const char kChromeUINetworkURL[] = "chrome-ui://about/network";
#if defined(OS_LINUX)
// TODO(port): Remove ifdef when we think that Linux splash page is not needed.
const char kChromeUINewTabURL[] = "about:linux-splash";
#else
const char kChromeUINewTabURL[] = "chrome-ui://newtab";
#endif

const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIDownloadsHost[] = "downloads";
const char kChromeUIExtensionsHost[] = "extensions";
const char kChromeUIFavIconPath[] = "favicon";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIInspectorHost[] = "inspector";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUIThumbnailPath[] = "thumb";

}  // namespace chrome
