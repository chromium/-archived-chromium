// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_KWALLET
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_KWALLET

#include <dbus/dbus-glib.h>
#include <glib.h>

#include <string>
#include <vector>

#include "base/lock.h"
#include "base/thread.h"
#include "chrome/browser/password_manager/password_store.h"
#include "webkit/glue/password_form.h"

class Pickle;
class Profile;
class Task;

class PasswordStoreKWallet : public PasswordStore {
 public:
  PasswordStoreKWallet();
  virtual ~PasswordStoreKWallet();

  bool Init();

 private:
  typedef std::vector<PasswordForm*> PasswordFormList;

  // Implements PasswordStore interface.
  void AddLoginImpl(const PasswordForm& form);
  void UpdateLoginImpl(const PasswordForm& form);
  void RemoveLoginImpl(const PasswordForm& form);
  void GetLoginsImpl(GetLoginsRequest* request);

  // Initialisation.
  bool StartKWalletd();
  bool InitWallet();

  // Reads a list of PasswordForms from the wallet that match the signon_realm
  // of key.
  void GetLoginsList(PasswordFormList* forms, const PasswordForm& key,
                     int wallet_handle);

  // Writes a list of PasswordForms to the wallet with the signon_realm from
  // key.  Overwrites any existing list for this key.
  void SetLoginsList(const PasswordFormList& forms, const PasswordForm& key,
                     int wallet_handle);

  // Checks if the last dbus call returned an error.  If it did, logs the error
  // message, frees it and returns true.
  // This must be called after every dbus call.
  bool CheckError();

  // Opens the wallet and ensures that the "Chrome Form Data" folder exists.
  // Returns kInvalidWalletHandle on error.
  int WalletHandle();

  // Compares two PasswordForms and returns true if they are the same.
  // Checks only the fields that we persist in KWallet, and ignores
  // password_value.
  static bool CompareForms(const PasswordForm& a, const PasswordForm& b);

  // Serializes a list of PasswordForms to be stored in the wallet.
  static void SerializeValue(const PasswordFormList& forms, Pickle* pickle);

  // Deserializes a list of PasswordForms from the wallet.
  static void DeserializeValue(const PasswordForm& key, const Pickle& pickle,
                               PasswordFormList* forms);

  // Convenience function to read a GURL from a Pickle.  Assumes the URL has
  // been written as a std::string.
  static void ReadGURL(const Pickle& pickle, void** iter, GURL* url);

  // Name of the application - will appear in kwallet's dialogs.
  static const char* kAppId;
  // Name of the folder to store passwords in.
  static const char* kKWalletFolder;

  // DBUS stuff.
  static const char* kKWalletServiceName;
  static const char* kKWalletPath;
  static const char* kKWalletInterface;
  static const char* kKLauncherServiceName;
  static const char* kKLauncherPath;
  static const char* kKLauncherInterface;

  // Invalid handle returned by WalletHandle().
  static const int kInvalidKWalletHandle = -1;

  // Controls all access to kwallet dbus calls.
  Lock kwallet_lock_;

  // Error from the last dbus call.  NULL when there's no error.  Freed and
  // cleared by CheckError().
  GError* error_;
  // Connection to the dbus session bus.
  DBusGConnection* connection_;
  // Proxy to the kwallet dbus service.
  DBusGProxy* proxy_;

  // The name of the wallet we've opened.  Set during Init().
  std::string wallet_name_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreKWallet);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_KWALLET
