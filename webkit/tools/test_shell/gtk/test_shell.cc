// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell.h"

#include <gtk/gtk.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"

WebPreferences* TestShell::web_prefs_ = NULL;

WindowList* TestShell::window_list_;

TestShell::TestShell() {
  // Uncomment this line to get a bunch of linker errors.  This is what we need
  // to fix.
  //m_webViewHost.reset(WebViewHost::Create(NULL, NULL, *TestShell::web_prefs_));
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
