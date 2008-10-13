// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/icu_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_switches.h"

// TODO(port): This file is intended to match test_shell_main.cc.
// Remerge this back into test_shell_main once we have enough supporting pieces
// in place.

int main(int argc, char* argv[]) {
  // Make Singletons work.
  base::AtExitManager at_exit_manager;

  gtk_init(&argc, &argv);
  // Only parse the command line after GTK's had a crack at it.
  CommandLine::SetArgcArgv(argc, argv);

  CommandLine parsed_command_line;

  icu_util::Initialize();

  bool layout_test_mode =
      parsed_command_line.HasSwitch(test_shell::kLayoutTests);

  bool interactive = !layout_test_mode;
  TestShell::InitializeTestShell(interactive);

  // Treat the first loose value as the initial URL to open.
  std::wstring uri;

  // Default to a homepage if we're interactive.
  if (interactive) {
    PathService::Get(base::DIR_SOURCE_ROOT, &uri);
    file_util::AppendToPath(&uri, L"webkit");
    file_util::AppendToPath(&uri, L"data");
    file_util::AppendToPath(&uri, L"test_shell");
    file_util::AppendToPath(&uri, L"index.html");
  }

  if (parsed_command_line.GetLooseValueCount() > 0) {
    CommandLine::LooseValueIterator iter =
        parsed_command_line.GetLooseValuesBegin();
    uri = *iter;
  }

  TestShell* shell;
  if (TestShell::CreateNewWindow(uri, &shell)) {
    // TODO(port): the rest of this.  :)
  }

  // TODO(port): use MessageLoop instead.
  gtk_main();

  return 0;
}
