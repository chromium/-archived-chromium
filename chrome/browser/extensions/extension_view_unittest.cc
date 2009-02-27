// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

// How long to wait for the extension to put up a javascript alert before giving
// up.
const int kAlertTimeoutMs = 10000;

// The extension we're using as our test case.
const char* kExtensionId = "com.google.myextension1";

// This class starts up an extension process and waits until it tries to put
// up a javascript alert.
class MockExtensionView : public ExtensionView {
 public:
  MockExtensionView(const GURL& url, Profile* profile)
     : ExtensionView(url, profile), got_message_(false) {
    InitHidden();
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        new MessageLoop::QuitTask, kAlertTimeoutMs);
    ui_test_utils::RunMessageLoop();
  }

  bool got_message() { return got_message_; }
 private:
  virtual void RunJavaScriptMessage(
      const std::wstring& message,
      const std::wstring& default_prompt,
      const int flags,
      IPC::Message* reply_msg,
      bool* did_suppress_message) {
    got_message_ = true;
    MessageLoopForUI::current()->Quit();
  }

  bool got_message_;
};

// This class waits for a specific extension to be loaded.
class ExtensionLoadedObserver : public NotificationObserver {
 public:
  explicit ExtensionLoadedObserver() : extension_(NULL) {
    registrar_.Add(this, NotificationType::EXTENSIONS_LOADED,
        NotificationService::AllSources());
    ui_test_utils::RunMessageLoop();
  }

  Extension* extension() { return extension_; }
 private:
  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::EXTENSIONS_LOADED) {
      ExtensionList* extensions = Details<ExtensionList>(details).ptr();
      for (size_t i = 0; i < (*extensions).size(); i++) {
        if ((*extensions)[i]->id() == kExtensionId) {
          extension_ = (*extensions)[i];
          MessageLoopForUI::current()->Quit();
          break;
        }
      }
    } else {
      NOTREACHED();
    }
  }

  NotificationRegistrar registrar_;
  Extension* extension_;
};

}  // namespace

class ExtensionViewTest : public InProcessBrowserTest {
};

IN_PROC_BROWSER_TEST_F(ExtensionViewTest, DISABLED_TestMe) {
  // Get the path to our extension.
  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions").
      AppendASCII("good").AppendASCII("extension1").AppendASCII("1");

  // Load it.
  Profile* profile = browser()->profile();
  profile->GetExtensionsService()->Init();
  profile->GetExtensionsService()->LoadExtension(path);

  // Now wait for it to load, and grab a pointer to it.
  Extension* extension = ExtensionLoadedObserver().extension();
  ASSERT_TRUE(extension);
  GURL url = Extension::GetResourceURL(extension->url(), "index.html");

  // Start the extension process and wait for it to show a javascript alert.
  MockExtensionView view(url, profile);
  EXPECT_TRUE(view.got_message());
}
