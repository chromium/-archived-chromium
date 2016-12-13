// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_list.h"

#import "chrome/browser/app_controller_mac.h"

void AutomationProviderList::OnLastProviderRemoved() {
  // We need to explicitly quit the application here because on Mac
  // the controller holds an additional reference to g_browser_process.
  [[NSApp delegate] quit:nil];
}
