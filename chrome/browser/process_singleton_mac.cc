// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

// This class is used to funnel messages to a single instance of the browser
// process. This is needed for several reasons on other platforms.
//
// On Windows, when the user re-opens the application from the shell (e.g. an
// explicit double-click, a shortcut that opens a webpage, etc.) we need to send
// the message to the currently-existing copy of the browser.
//
// On Linux, opening a URL is done by creating an instance of the web browser
// process and passing it the URL to go to on its commandline.
//
// Neither of those cases apply on the Mac. Launch Services ensures that there
// is only one instance of the process, and we get URLs to open via AppleEvents
// and, once again, the Launch Services system. We have no need to manage this
// ourselves.

ProcessSingleton::ProcessSingleton(const FilePath& user_data_dir) {
  // This space intentionally left blank.
}

ProcessSingleton::~ProcessSingleton() {
  // This space intentionally left blank.
}

bool ProcessSingleton::NotifyOtherProcess() {
  // This space intentionally left blank.
  return false;
}

void ProcessSingleton::Create() {
  // This space intentionally left blank.
}
