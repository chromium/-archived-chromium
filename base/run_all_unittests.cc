// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/test_suite.h"

int main(int argc, char** argv) {
  // Set up an AtExitManager so Singleton objects will be destroyed.
  base::AtExitManager at_exit_manager;

  CommandLine::SetArgcArgv(argc, argv);

  return TestSuite(argc, argv).Run();
}

