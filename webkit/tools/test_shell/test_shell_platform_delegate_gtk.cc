// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>  // required by _set_abort_behavior
#include <gtk/gtk.h>

#include "base/command_line.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_platform_delegate.h"

// Hooks Gtk's logs so that we can ignore harmless warnings.
static void GtkLogHandler(const gchar* log_domain,
                          GLogLevelFlags log_level,
                          const gchar* message,
                          gpointer userdata) {
  if (strstr(message, "Loading IM context type") ||
      strstr(message, "wrong ELF class: ELFCLASS64")) {
    // GTK outputs some warnings when it attempts to load 64bit shared
    // objects from 32bit binaries. Suppress them as they are not
    // actual errors. The warning messages are like
    //
    // (test_shell:2476): Gtk-WARNING **:
    // /usr/lib/gtk-2.0/2.10.0/immodules/im-uim.so: wrong ELF class: ELFCLASS64
    //
    // (test_shell:2476): Gtk-WARNING **: Loading IM context type 'uim' failed
    //
    // Related bug: http://crbug.com/9643
  } else {
    g_log_default_handler(log_domain, log_level, message, userdata);
  }
}

static void SetUpGtkLogHandler() {
  g_log_set_handler("Gtk", G_LOG_LEVEL_WARNING, GtkLogHandler, NULL);
}

void TestShellPlatformDelegate::PreflightArgs(int *argc, char ***argv) {
  gtk_init(argc, argv);
  SetUpGtkLogHandler();
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
