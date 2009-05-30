// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_GNOME
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_GNOME

extern "C" {
#include <gnome-keyring.h>
}

#include "base/lock.h"
#include "chrome/browser/password_manager/password_store.h"

class Profile;
class Task;

// PasswordStore implementation using Gnome Keyring.
class PasswordStoreGnome : public PasswordStore {
 public:
  PasswordStoreGnome();
  virtual ~PasswordStoreGnome();

  virtual bool Init();

 private:
  void AddLoginImpl(const PasswordForm& form);
  void UpdateLoginImpl(const PasswordForm& form);
  void RemoveLoginImpl(const PasswordForm& form);
  void GetLoginsImpl(GetLoginsRequest* request);

  static const GnomeKeyringPasswordSchema kGnomeSchema;

  // Mutex for all interactions with Gnome Keyring.
  Lock gnome_keyring_lock_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreGnome);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_GNOME
