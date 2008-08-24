// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test_suite.h"

// Force a test to use an already running browser instance. UI tests only.
const wchar_t UITestSuite::kUseExistingBrowser[] = L"use-existing-browser";

// Timeout for the test in milliseconds.  UI tests only.
const wchar_t UITestSuite::kTestTimeout[] = L"test-timeout";

