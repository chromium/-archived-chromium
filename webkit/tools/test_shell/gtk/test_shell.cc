// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell.h"

#include <gtk/gtk.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "net/base/mime_util.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"

WebPreferences* TestShell::web_prefs_ = NULL;

WindowList* TestShell::window_list_;

TestShell::TestShell() {
  // Uncomment this line to get a bunch of linker errors.  This is what we need
  // to fix.
  m_webViewHost.reset(WebViewHost::Create(NULL, NULL, *TestShell::web_prefs_));
}

TestShell::~TestShell() {
}

// static
void TestShell::InitializeTestShell(bool interactive) {
  window_list_ = new WindowList;

  web_prefs_ = new WebPreferences;
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

  // It's funny, ok?
  std::wstring path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  file_util::AppendToPath(&path, L"webkit");
  file_util::AppendToPath(&path, L"tools");
  file_util::AppendToPath(&path, L"test_shell");
  file_util::AppendToPath(&path, L"resources");
  file_util::AppendToPath(&path, L"acid3.png");
  GtkWidget* image = gtk_image_new_from_file(WideToUTF8(path).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(m_mainWnd), vbox);
  gtk_widget_show_all(m_mainWnd);

  return true;
}

void TestShell::BindJSObjectsToWindow(WebFrame* frame) {
  NOTIMPLEMENTED();
}

bool TestShell::Navigate(const TestNavigationEntry& entry, bool reload) {
  NOTIMPLEMENTED();
  return true;
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

// The following cookie functions shouldn't live here, but do for now in order
// to get things linking

std::string GetCookies(const GURL &url, const GURL &policy_url) {
  return "";
}

void SetCookie(const GURL &url, const GURL &policy_url, const std::string &cookie) {
}


ResourceLoaderBridge *
ResourceLoaderBridge::Create(WebFrame*, std::string const &, GURL const&, GURL const&, GURL const&, std::string const&, int, int, ResourceType::Type, bool) {
  return NULL;
}

}  // namespace webkit_glue
