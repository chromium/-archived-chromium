// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/at_exit.h"
#include "base/gfx/native_widget_types.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebString.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitclient_impl.h"
#include "webkit/tools/test_shell/test_webworker_helper.h"
#include "webkit/tools/test_shell/test_worker/test_webworker.h"

using WebKit::WebWorker;
using WebKit::WebWorkerClient;

// Create a global AtExitManager so that our code can use code from base that
// uses Singletons, for example.  We don't care about static constructors here.
static base::AtExitManager global_at_exit_manager;

// Stub WebKit Client.
class WorkerWebKitClientImpl : public webkit_glue::WebKitClientImpl {
 public:
  explicit WorkerWebKitClientImpl(TestWebWorkerHelper* helper)
      : helper_(helper) { }

  // WebKitClient methods:
  virtual WebKit::WebClipboard* clipboard() {
    NOTREACHED();
    return NULL;
  }

  virtual WebKit::WebMimeRegistry* mimeRegistry() {
    NOTREACHED();
    return NULL;
  }

  virtual WebKit::WebSandboxSupport* sandboxSupport() {
    NOTREACHED();
    return NULL;
  }

  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length) {
    NOTREACHED();
    return 0;
  }

  virtual bool isLinkVisited(unsigned long long linkHash) {
    NOTREACHED();
    return false;
  }

  virtual void setCookies(const WebKit::WebURL& url,
                          const WebKit::WebURL& first_party_for_cookies,
                          const WebKit::WebString& value) {
    NOTREACHED();
  }

  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url,
      const WebKit::WebURL& first_party_for_cookies) {
    NOTREACHED();
    return WebKit::WebString();
  }

  virtual void prefetchHostName(const WebKit::WebString&) {
    NOTREACHED();
  }

  virtual bool getFileSize(const WebKit::WebString& path, long long& result) {
    NOTREACHED();
    return false;
  }

  virtual WebKit::WebString defaultLocale() {
    NOTREACHED();
    return WebKit::WebString();
  }

  virtual void callOnMainThread(void (*func)()) {
    helper_->DispatchToMainThread(func);
  }

 private:
  TestWebWorkerHelper* helper_;
};

// WebKit client used in DLL.
static scoped_ptr<WorkerWebKitClientImpl> webkit_client;

#if defined(COMPILER_GCC)
#pragma GCC visibility push(default)
#endif
extern "C" {
// DLL entry points
WebWorker* API_CALL CreateWebWorker(WebWorkerClient* webworker_client,
                                    TestWebWorkerHelper* webworker_helper) {
  if (!WebKit::webKitClient()) {
    webkit_client.reset(new WorkerWebKitClientImpl(webworker_helper));
    WebKit::initialize(webkit_client.get());
  }

#if ENABLE(WORKERS)
  return new TestWebWorker(webworker_client, webworker_helper);
#else
  return NULL;
#endif
}
}  // extern "C"

// WebKit glue stub functions.
namespace webkit_glue {

#if defined(COMPILER_GCC)
// GCC hides the class methods like this by default, even in the scope
// of the "#pragma visibility". Need the attribute.
__attribute__((visibility("default")))
#endif
ResourceLoaderBridge* ResourceLoaderBridge::Create(
    const std::string& method,
    const GURL& url,
    const GURL& first_party_for_cookies,
    const GURL& referrer,
    const std::string& frame_origin,
    const std::string& main_frame_origin,
    const std::string& headers,
    int load_flags,
    int requestor_pid,
    ResourceType::Type request_type,
    int app_cache_context_id,
    int routing_id) {
  return NULL;
}

string16 GetLocalizedString(int message_id) {
  return EmptyString16();
}

StringPiece GetDataResource(int resource_id) {
  return StringPiece();
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

}  // namespace webkit_glue

#if defined(COMPILER_GCC)
#pragma GCC visibility pop
#endif
