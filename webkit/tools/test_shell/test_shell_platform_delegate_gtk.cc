// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>  // required by _set_abort_behavior
#include <gtk/gtk.h>

#include "base/command_line.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_platform_delegate.h"

void TestShellPlatformDelegate::PreflightArgs(int *argc, char ***argv) {
  gtk_init(argc, argv);
}

void TestShellPlatformDelegate::SelectUnifiedTheme() {
  // Stop custom gtkrc files from messing with the theme.
  gchar* default_gtkrc_files[] = { NULL };
  gtk_rc_set_default_files(default_gtkrc_files);
  gtk_rc_reparse_all_for_settings(gtk_settings_get_default(), TRUE);

  // Pick a theme that uses Cairo for drawing, since we:
  // 1) currently don't support GTK themes that use the GDK drawing APIs, and
  // 2) need to use a unified theme for layout tests anyway.
  g_object_set(gtk_settings_get_default(),
               "gtk-theme-name", "Mist",
               NULL);
}

TestShellPlatformDelegate::TestShellPlatformDelegate(
    const CommandLine& command_line)
    : command_line_(command_line) {
}

bool TestShellPlatformDelegate::CheckLayoutTestSystemDependencies() {
  return true;
}

void TestShellPlatformDelegate::SuppressErrorReporting() {
}

void TestShellPlatformDelegate::InitializeGUI() {
}

void TestShellPlatformDelegate::SetWindowPositionForRecording(TestShell *) {
}

TestShellPlatformDelegate::~TestShellPlatformDelegate() {
}
