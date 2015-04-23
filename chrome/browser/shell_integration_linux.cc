// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <stdlib.h>

#include <vector>

#include "base/process_util.h"

#if defined(GOOGLE_CHROME_BUILD)
#define DESKTOP_APP_NAME "google-chrome.desktop"
#else  // CHROMIUM_BUILD
#define DESKTOP_APP_NAME "chromium-browser.desktop"
#endif

// We delegate the difficult of setting the default browser in Linux desktop
// environments to a new xdg utility, xdg-settings. We'll have to include a copy
// of it for this to work, obviously, but that's actually the suggested approach
// for xdg utilities anyway.

bool ShellIntegration::SetAsDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("set");
  argv.push_back("default-web-browser");
  argv.push_back(DESKTOP_APP_NAME);

  int success_code;
  base::ProcessHandle handle;
  base::file_handle_mapping_vector no_files;
  if (!base::LaunchApp(argv, no_files, false, &handle))
    return false;

  base::WaitForExitCode(handle, &success_code);
  return success_code == EXIT_SUCCESS;
}

static std::string getDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("get");
  argv.push_back("default-web-browser");
  std::string output;
  base::GetAppOutput(CommandLine(argv), &output);
  // If GetAppOutput() fails, we'll return the empty string.
  return output;
}

bool ShellIntegration::IsDefaultBrowser() {
  std::string browser = getDefaultBrowser();
  // Allow for an optional newline at the end.
  if (browser.length() > 0 && browser[browser.length() - 1] == '\n')
    browser.resize(browser.length() - 1);
  if (!browser.length()) {
    // We don't know what the default browser is; chances are, we can't
    // set it either. So, check to see if we were run in the wrapper
    // and pretend that we are the default unless we were run from it,
    // to avoid warning that we aren't the default when it's useless.
    return !getenv("CHROME_WRAPPER");
  }
  return !browser.compare(DESKTOP_APP_NAME);
}

bool ShellIntegration::IsFirefoxDefaultBrowser() {
  return getDefaultBrowser().find("irefox") != std::string::npos;
}
