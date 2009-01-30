// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_IN_PROCESS_BROWSER_TEST_H_
#define CHROME_TEST_IN_PROCESS_BROWSER_TEST_H_

#include "chrome/app/scoped_ole_initializer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

class Browser;
class Profile;

// Base class for tests wanting to bring up a browser in the unit test process.
// Writing tests with InProcessBrowserTest is slightly different than that of
// other tests. This is necessitated by InProcessBrowserTest running a message
// loop. To use InProcessBrowserTest do the following:
// . Use the macro IN_PROC_BROWSER_TEST_F to define your test.
// . Your test method is invoked on the ui thread. If you need to block until
//   state changes you'll need to run the message loop from your test method.
//   For example, if you need to wait till a find bar has completely been shown
//   you'll need to invoke ui_test_utils::RunMessageLoop. When the message bar
//   is shown, invoke MessageLoop::current()->Quit() to return control back to
//   your test method.
// . If you subclass and override SetUp, be sure and invoke
//   InProcessBrowserTest::SetUp.
//
// By default InProcessBrowserTest creates a single Browser (as returned from
// the CreateBrowser method). You can obviously create more as needed.

// Browsers created while InProcessBrowserTest is running are shown hidden. Use
// the command line switch --show-windows to make them visible when debugging.
//
// InProcessBrowserTest disables the sandbox when running.
//
// See ui_test_utils for a handful of methods designed for use with this class.
class InProcessBrowserTest : public testing::Test, public NotificationObserver {
 public:
  InProcessBrowserTest();

  // We do this so we can be used in a Task.
  void AddRef() {}
  void Release() {}

  // Configures everything for an in process browser test, then invokes
  // BrowserMain. BrowserMain ends up invoking RunTestOnMainThreadLoop.
  virtual void SetUp();

  // Restores state configured in SetUp.
  virtual void TearDown();

  // Used to track when the browser_ is destroyed. Resets the |browser_| field
  // to NULL.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // Returns the browser created by CreateBrowser.
  Browser* browser() const { return browser_; }

  // Override this rather than TestBody.
  virtual void RunTestOnMainThread() = 0;

  // Starts an HTTP server.
  HTTPTestServer* StartHTTPServer();

  // Creates a browser with a single tab (about:blank), waits for the tab to
  // finish loading and shows the browser.
  //
  // This is invoked from Setup.
  virtual Browser* CreateBrowser(Profile* profile);

 private:
  // Invokes CreateBrowser to create a browser, then RunTestOnMainThread, and
  // destroys the browser.
  void RunTestOnMainThreadLoop();

  // Browser created from CreateBrowser.
  Browser* browser_;

  // Used to track when the browser is deleted.
  NotificationRegistrar registrar_;

  // HTTPServer, created when StartHTTPServer is invoked.
  scoped_refptr<HTTPTestServer> http_server_;

  ScopedOleInitializer ole_initializer_;

  DISALLOW_COPY_AND_ASSIGN(InProcessBrowserTest);
};

#define IN_PROC_BROWSER_TEST_(test_case_name, test_name, parent_class,\
                              parent_id)\
class GTEST_TEST_CLASS_NAME_(test_case_name, test_name) : public parent_class {\
 public:\
  GTEST_TEST_CLASS_NAME_(test_case_name, test_name)() {}\
 protected:\
  virtual void RunTestOnMainThread();\
 private:\
  virtual void TestBody() {}\
  static ::testing::TestInfo* const test_info_;\
  GTEST_DISALLOW_COPY_AND_ASSIGN_(\
      GTEST_TEST_CLASS_NAME_(test_case_name, test_name));\
};\
\
::testing::TestInfo* const GTEST_TEST_CLASS_NAME_(test_case_name, test_name)\
  ::test_info_ =\
    ::testing::internal::MakeAndRegisterTestInfo(\
        #test_case_name, #test_name, "", "", \
        (parent_id), \
        parent_class::SetUpTestCase, \
        parent_class::TearDownTestCase, \
        new ::testing::internal::TestFactoryImpl<\
            GTEST_TEST_CLASS_NAME_(test_case_name, test_name)>);\
void GTEST_TEST_CLASS_NAME_(test_case_name, test_name)::RunTestOnMainThread()

#define IN_PROC_BROWSER_TEST_F(test_fixture, test_name)\
  IN_PROC_BROWSER_TEST_(test_fixture, test_name, test_fixture,\
                    ::testing::internal::GetTypeId<test_fixture>())

#endif  // CHROME_TEST_IN_PROCESS_BROWSER_TEST_H_
