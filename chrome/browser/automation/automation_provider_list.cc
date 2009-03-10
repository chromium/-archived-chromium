// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_list.h"

#include <algorithm>
#include "base/logging.h"
#include "chrome/browser/automation/automation_provider.h"

AutomationProviderList* AutomationProviderList::instance_ = NULL;

AutomationProviderList::AutomationProviderList() {
}

AutomationProviderList::~AutomationProviderList() {
  iterator iter = automation_providers_.begin();
  while (iter != automation_providers_.end()) {
    (*iter)->Release();
    iter = automation_providers_.erase(iter);
    g_browser_process->ReleaseModule();
  }
  instance_ = NULL;
}

bool AutomationProviderList::AddProvider(AutomationProvider* provider) {
  provider->AddRef();
  automation_providers_.push_back(provider);
  g_browser_process->AddRefModule();
  return true;
}

bool AutomationProviderList::RemoveProvider(AutomationProvider* provider) {
  const iterator remove_provider =
    find(automation_providers_.begin(), automation_providers_.end(), provider);
  if (remove_provider != automation_providers_.end()) {
    (*remove_provider)->Release();
    automation_providers_.erase(remove_provider);
    g_browser_process->ReleaseModule();
    return true;
  }
  return false;
}

AutomationProviderList* AutomationProviderList::GetInstance() {
  if (!instance_) {
    instance_ = new AutomationProviderList;
  }
  DCHECK(NULL != instance_);
  return instance_;
}
