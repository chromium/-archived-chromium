// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/at_exit.h"
#include "base/gfx/native_widget_types.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "chrome/worker/worker_webkitclient_impl.h"
#include "chrome/test/worker/test_webworker.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/screen_info.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webworker.h"
#include "webkit/glue/webworkerclient.h"
#include "webkit/tools/test_shell/test_webworker_helper.h"

// Create a global AtExitManager so that our code can use code from base that
// uses Singletons, for example.  We don't care about static constructors here.
static base::AtExitManager global_at_exit_manager;

// WebKit client used in DLL.
static scoped_ptr<WorkerWebKitClientImpl> webkit_client;

// DLL entry points
WebWorker* API_CALL CreateWebWorker(WebWorkerClient* webworker_client,
                                    TestWebWorkerHelper* webworker_helper) {
  if (!WebKit::webKitClient()) {
    webkit_client.reset(new WorkerWebKitClientImpl());
    WebKit::initialize(webkit_client.get());
  }

#if ENABLE(WORKERS)
  return new TestWebWorker(webworker_client, webworker_helper);
#else
  return NULL;
#endif
}

// WebKit glue functions

namespace webkit_glue {

ResourceLoaderBridge* ResourceLoaderBridge::Create(
    const std::string& method,
    const GURL& url,
    const GURL& policy_url,
    const GURL& referrer,
    const std::string& frame_origin,
    const std::string& main_frame_origin,
    const std::string& headers,
    const std::string& default_mime_type,
    int load_flags,
    int requestor_pid,
    ResourceType::Type request_type,
    int routing_id) {
  return NULL;
}

string16 GetLocalizedString(int message_id) {
  return L"";
}

std::string GetDataResource(int resource_id) {
  return "";
}

void SetMediaPlayerAvailable(bool value) {
}

bool IsMediaPlayerAvailable() {
  return false;
}

void PrecacheUrl(const char16* url, int url_length) {
}

void AppendToLog(const char* file, int line, const char* msg) {
  logging::LogMessage(file, line).stream() << msg;
}

bool GetApplicationDirectory(std::wstring *path) {
  return PathService::Get(base::DIR_EXE, path);
}

GURL GetInspectorURL() {
  return GURL("test-shell-resource://inspector/inspector.html");
}

std::string GetUIResourceProtocol() {
  return "test-shell-resource";
}

bool GetExeDirectory(std::wstring *path) {
  return PathService::Get(base::DIR_EXE, path);
}

bool SpellCheckWord(const wchar_t* word, int word_len,
                    int* misspelling_start, int* misspelling_len) {
  // Report all words being correctly spelled.
  *misspelling_start = 0;
  *misspelling_len = 0;
  return true;
}

ScreenInfo GetScreenInfo(gfx::NativeViewId window) {
  return GetScreenInfoHelper(gfx::NativeViewFromId(window));
}

bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  return false;
}

bool IsPluginRunningInRendererProcess() {
  return false;
}

bool GetPluginFinderURL(std::string* plugin_finder_url) {
  return false;
}

bool IsDefaultPluginEnabled() {
  return false;
}

bool FindProxyForUrl(const GURL& url, std::string* proxy_list) {
  return false;
}

std::wstring GetWebKitLocale() {
  return L"en-US";
}

#if defined(OS_WIN)
HCURSOR LoadCursor(int cursor_id) {
  return NULL;
}

bool EnsureFontLoaded(HFONT font) {
  return true;
}

bool DownloadUrl(const std::string& url, HWND caller_window) {
  return false;
}
#endif

}
