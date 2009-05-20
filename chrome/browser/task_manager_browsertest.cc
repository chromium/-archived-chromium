// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager.h"

#include "chrome/test/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class TaskManagerBrowserTest : public InProcessBrowserTest {
};

// Regression test for http://crbug.com/11180
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, ShutdownWhileOpen) {
  TaskManager::Open();
}

// TODO(phajdan.jr): Write another test which explicitly closes TaskManager.
// This requires a small refactoring.
