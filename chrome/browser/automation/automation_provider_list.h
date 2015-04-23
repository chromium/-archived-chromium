// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_LIST_H__
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_LIST_H__

#include <vector>
#include "base/basictypes.h"

class AutomationProvider;

// Stores a list of all AutomationProvider objects.
class AutomationProviderList {
 public:
  ~AutomationProviderList();
  typedef std::vector<AutomationProvider*> list_type;
  typedef list_type::iterator iterator;
  typedef list_type::const_iterator const_iterator;

  // Adds and removes automation providers from the global list.
  bool AddProvider(AutomationProvider* provider);
  bool RemoveProvider(AutomationProvider* provider);

  const_iterator begin() {
    return automation_providers_.begin();
  }

  const_iterator end() {
    return automation_providers_.end();
  }

  size_t size() {
    return automation_providers_.size();
  }

  static AutomationProviderList* GetInstance();

private:
  AutomationProviderList();
  void OnLastProviderRemoved();
  list_type automation_providers_;
  static AutomationProviderList* instance_;

  DISALLOW_EVIL_CONSTRUCTORS(AutomationProviderList);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_LIST_H__
