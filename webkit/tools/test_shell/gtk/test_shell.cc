// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell.h"

#include <gtk/gtk.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "net/base/mime_util.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_webview_delegate.h"

WebPreferences* TestShell::web_prefs_ = NULL;

WindowList* TestShell::window_list_;

TestShell::TestShell()
    : delegate_(new TestWebViewDelegate(this)) {
  layout_test_controller_.reset(new LayoutTestController(this));
  navigation_controller_.reset(new TestNavigationController(this));
}

TestShell::~TestShell() {
}

bool TestShell::interactive_ = false;

// static
void TestShell::InitializeTestShell(bool interactive) {
  window_list_ = new WindowList;
  web_prefs_ = new WebPreferences;
  interactive_ = interactive;
}

// static
bool TestShell::CreateNewWindow(const std::wstring& startingURL,
                                TestShell** result) {
  TestShell *shell = new TestShell();
  if (!shell->Initialize(startingURL))
    return false;
  if (result)
    *result = shell;
  TestShell::windowList()->push_back(shell->m_mainWnd);
  return true;
}

bool TestShell::Initialize(const std::wstring& startingURL) {
  m_mainWnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(m_mainWnd), "Test Shell");
  gtk_window_set_default_size(GTK_WINDOW(m_mainWnd), 640, 480);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget* toolbar = gtk_toolbar_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
                     gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK),
                     -1 /* append */);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
                     gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD),
                     -1 /* append */);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
                     gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH),
                     -1 /* append */);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
                     gtk_tool_button_new_from_stock(GTK_STOCK_STOP),
                     -1 /* append */);

  m_editWnd = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(m_editWnd), WideToUTF8(startingURL).c_str());

  GtkToolItem* tool_item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(tool_item), m_editWnd);
  gtk_tool_item_set_expand(tool_item, TRUE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
                     tool_item,
                     -1 /* append */);

  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
  m_webViewHost.reset(WebViewHost::Create(vbox, NULL, *TestShell::web_prefs_));

  if (!startingURL.empty())
    LoadURL(startingURL.c_str());

  gtk_container_add(GTK_CONTAINER(m_mainWnd), vbox);
  gtk_widget_show_all(m_mainWnd);

  return true;
}

void TestShell::TestFinished() {
  NOTIMPLEMENTED();
}

void TestShell::Show(WebView* webview, WindowOpenDisposition disposition) {
  delegate_->Show(webview, disposition);
}

void TestShell::SetFocus(WebWidgetHost* host, bool enable) {
  // TODO(agl): port the body of this function
  NOTIMPLEMENTED();
}

void TestShell::BindJSObjectsToWindow(WebFrame* frame) {
  NOTIMPLEMENTED();
}

void TestShell::DestroyWindow(gfx::WindowHandle windowHandle) {
  NOTIMPLEMENTED();
}

WebView* TestShell::CreateWebView(WebView* webview) {
  NOTIMPLEMENTED();
  return NULL;
}

WebWidget* TestShell::CreatePopupWidget(WebView* webview) {
  NOTIMPLEMENTED();
  return NULL;
}

void TestShell::LoadURL(const wchar_t* url)
{
    LoadURLForFrame(url, NULL);
}

void TestShell::LoadURLForFrame(const wchar_t* url,
                                const wchar_t* frame_name) {
    if (!url)
        return;

    std::wstring frame_string;
    if (frame_name)
        frame_string = frame_name;

    navigation_controller_->LoadEntry(new TestNavigationEntry(
        -1, GURL(WideToUTF8(url)), std::wstring(), frame_string));
}

bool TestShell::Navigate(const TestNavigationEntry& entry, bool reload) {
  WebRequestCachePolicy cache_policy;
  if (reload) {
    cache_policy = WebRequestReloadIgnoringCacheData;
  } else if (entry.GetPageID() != -1) {
    cache_policy = WebRequestReturnCacheDataElseLoad;
  } else {
    cache_policy = WebRequestUseProtocolCachePolicy;
  }

  scoped_ptr<WebRequest> request(WebRequest::Create(entry.GetURL()));
  request->SetCachePolicy(cache_policy);
  // If we are reloading, then WebKit will use the state of the current page.
  // Otherwise, we give it the state to navigate to.
  if (!reload)
    request->SetHistoryState(entry.GetContentState());
    
  request->SetExtraData(
      new TestShellExtraRequestData(entry.GetPageID()));

  // Get the right target frame for the entry.
  WebFrame* frame = webView()->GetMainFrame();
  if (!entry.GetTargetFrame().empty())
      frame = webView()->GetFrameWithName(entry.GetTargetFrame());
  // TODO(mpcomplete): should we clear the target frame, or should
  // back/forward navigations maintain the target frame?

  frame->LoadRequest(request.get());
  // Restore focus to the main frame prior to loading new request.
  // This makes sure that we don't have a focused iframe. Otherwise, that
  // iframe would keep focus when the SetFocus called immediately after
  // LoadRequest, thus making some tests fail (see http://b/issue?id=845337
  // for more details).
  webView()->SetFocusedFrame(frame);
  SetFocus(webViewHost(), true);

  return true;
}

void TestShell::GoBackOrForward(int offset) {
    navigation_controller_->GoToOffset(offset);
}

void TestShell::Reload() {
    navigation_controller_->Reload();
}

std::string TestShell::RewriteLocalUrl(const std::string& url) {
  NOTIMPLEMENTED();
  return "";
}

//-----------------------------------------------------------------------------

namespace webkit_glue {

void PrefetchDns(const std::string& hostname) {}

void PrecacheUrl(const char16* url, int url_length) {}

void AppendToLog(const char* file, int line, const char* msg) {
  logging::LogMessage(file, line).stream() << msg;
}

bool GetMimeTypeFromExtension(const std::wstring &ext, std::string *mime_type) {
  return net::GetMimeTypeFromExtension(ext, mime_type);
}

bool GetMimeTypeFromFile(const std::wstring &file_path,
                         std::string *mime_type) {
  return net::GetMimeTypeFromFile(file_path, mime_type);
}

bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      std::wstring* ext) {
  return net::GetPreferredExtensionForMimeType(mime_type, ext);
}

std::wstring GetLocalizedString(int message_id) {
  NOTREACHED();
  return L"No string for this identifier!";
}

std::string GetDataResource(int resource_id) {
  NOTREACHED();
  return std::string();
}

SkBitmap* GetBitmapResource(int resource_id) {
  return NULL;
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
  //return NPAPI::PluginList::Singleton()->GetPlugins(refresh, plugins);
  NOTIMPLEMENTED();
  return false;
}

bool IsPluginRunningInRendererProcess() {
  return true;
}

bool GetPluginFinderURL(std::string* plugin_finder_url) {
  return false;
}

bool IsDefaultPluginEnabled() {
  return false;
}

std::wstring GetWebKitLocale() {
  return L"en-US";
}

}  // namespace webkit_glue
