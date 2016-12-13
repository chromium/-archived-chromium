// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_TEST_SUITE_H_
#define APP_TEST_SUITE_H_

#include "build/build_config.h"

#include <string>

#include "app/app_paths.h"
#include "app/resource_bundle.h"
#include "base/path_service.h"
#if defined(OS_MACOSX)
#include "base/mac_util.h"
#endif
#include "base/scoped_nsautorelease_pool.h"
#include "base/test_suite.h"

class AppTestSuite : public TestSuite {
 public:
  AppTestSuite(int argc, char** argv) : TestSuite(argc, argv) {
  }

 protected:

  virtual void Initialize() {
    base::ScopedNSAutoreleasePool autorelease_pool;

    TestSuite::Initialize();

    app::RegisterPathProvider();

#if defined(OS_MACOSX)
    FilePath path;
    PathService::Get(base::DIR_EXE, &path);
    // TODO(port): make a resource bundle for non-app exes.
    path = path.AppendASCII("Chromium.app");
    mac_util::SetOverrideAppBundlePath(path);
#endif

    // Force unittests to run using en-US so if we test against string
    // output, it'll pass regardless of the system language.
    ResourceBundle::InitSharedInstance(L"en-US");
    ResourceBundle::GetSharedInstance().LoadThemeResources();
  }

  virtual void Shutdown() {
    ResourceBundle::CleanupSharedInstance();

#if defined(OS_MACOSX)
    mac_util::SetOverrideAppBundle(NULL);
#endif
    TestSuite::Shutdown();
  }
};

#endif  // APP_TEST_SUITE_H_
