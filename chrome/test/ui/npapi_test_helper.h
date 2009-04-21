// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_HELPER_H_
#define CHROME_TEST_HELPER_H_

#include "chrome/test/ui/ui_test.h"

// Helper class for NPAPI plugin UI tests.
class NPAPITester : public UITest {
 protected:
  NPAPITester();
  virtual void SetUp();
  virtual void TearDown();

private:
  std::wstring plugin_dll_;
};

// Helper class for NPAPI plugin UI tests, which need the browser window
// to be visible.
class NPAPIVisiblePluginTester : public NPAPITester {
  protected:
   virtual void SetUp();
};

// Helper class for NPAPI plugin UI tests which use incognito mode.
class NPAPIIncognitoTester : public NPAPITester {
  protected:
   virtual void SetUp();
};

#endif  // CHROME_TEST_HELPER_H_
