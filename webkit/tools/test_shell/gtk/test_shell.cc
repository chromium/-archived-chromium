// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell.h"

#include <gtk/gtk.h>

#include "base/string_util.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"

WindowList* TestShell::window_list_;

TestShell::TestShell() {
}

TestShell::~TestShell() {
}

// static
void TestShell::InitializeTestShell(bool interactive) {
  window_list_ = new WindowList;
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
