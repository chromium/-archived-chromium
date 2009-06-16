// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/password_manager/password_store.h"

class MacKeychain;

class PasswordStoreMac : public PasswordStore {
 public:
  // Takes ownership of |keychain|, which must not be NULL.
  explicit PasswordStoreMac(MacKeychain* keychain);
  virtual ~PasswordStoreMac() {}

 private:
  void AddLoginImpl(const webkit_glue::PasswordForm& form);
  void UpdateLoginImpl(const webkit_glue::PasswordForm& form);
  void RemoveLoginImpl(const webkit_glue::PasswordForm& form);
  void GetLoginsImpl(GetLoginsRequest* request);

  scoped_ptr<MacKeychain> keychain_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreMac);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_
