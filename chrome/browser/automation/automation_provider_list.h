// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
  list_type automation_providers_;
  static AutomationProviderList* instance_;

  DISALLOW_EVIL_CONSTRUCTORS(AutomationProviderList);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_LIST_H__
