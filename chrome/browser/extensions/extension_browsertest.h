// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_TEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_TEST_H_

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"

// Base class for extension browser tests. Provides utilities for loading,
// unloading, and installing extensions.
class ExtensionBrowserTest
    : public InProcessBrowserTest, public NotificationObserver {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line);
  bool LoadExtension(const FilePath& path);
  bool InstallExtension(const FilePath& path);
  void UninstallExtension(const std::string& extension_id);

  bool loaded_;
  bool installed_;
  FilePath test_data_dir_;

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
  bool WaitForExtensionHostsToLoad();

  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_TEST_H_
