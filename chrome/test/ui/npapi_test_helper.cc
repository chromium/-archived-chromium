// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/npapi_test_helper.h"

#include "chrome/common/chrome_switches.h"

NPAPITester::NPAPITester()
    : UITest() {
}

void NPAPITester::SetUp() {
  // We need to copy our test-plugin into the plugins directory so that
  // the browser can load it.
  std::wstring plugins_directory = browser_directory_.ToWStringHack() +
      L"\\plugins";
  std::wstring plugin_src = browser_directory_.ToWStringHack() +
      L"\\npapi_test_plugin.dll";
  plugin_dll_ = plugins_directory + L"\\npapi_test_plugin.dll";

  CreateDirectory(plugins_directory.c_str(), NULL);
  CopyFile(plugin_src.c_str(), plugin_dll_.c_str(), FALSE);

  UITest::SetUp();
}

void NPAPITester::TearDown() {
  DeleteFile(plugin_dll_.c_str());
  UITest::TearDown();
}


// NPAPIVisiblePluginTester members.
void NPAPIVisiblePluginTester::SetUp() {
  show_window_ = true;
  NPAPITester::SetUp();
}

// NPAPIIncognitoTester members.
void NPAPIIncognitoTester::SetUp() {
  launch_arguments_.AppendSwitch(switches::kIncognito);
  NPAPITester::SetUp();
}
