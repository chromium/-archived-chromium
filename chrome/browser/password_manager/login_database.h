// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_LOGIN_DATABASE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_LOGIN_DATABASE_H_

#include <string>
#include <vector>

#include "chrome/browser/meta_table_helper.h"
#include "webkit/glue/password_form.h"

class FilePath;
struct sqlite3;

// Base class for database storage of login information, intended as a helper
// for PasswordStore on platforms that need internal storage of some or all of
// the login information.
// Subclasses need to provide EncryptedString and DecryptedString
// implementations, which will be used to encrypt the password in the database.
class LoginDatabase {
 public:
  LoginDatabase();
  virtual ~LoginDatabase();

  // Initialize the database with an sqlite file at the given path.
  // If false is returned, no other method should be called.
  bool Init(const FilePath& db_path);

  // Adds |form| to the list of remembered password forms.
  bool AddLogin(const webkit_glue::PasswordForm& form);

  // Updates remembered password form. Returns true on success and sets
  // items_changed (if non-NULL) to the number of logins updated.
  bool UpdateLogin(const webkit_glue::PasswordForm& form, int* items_changed);

  // Removes |form| from the list of remembered password forms.
  bool RemoveLogin(const webkit_glue::PasswordForm& form);

  // Removes all logins created from |delete_begin| onwards (inclusive) and
  // before |delete_end|. You may use a null Time value to do an unbounded
  // delete in either direction.
  bool RemoveLoginsCreatedBetween(const base::Time delete_begin,
                                  const base::Time delete_end);

  // Loads a list of matching password forms into the specified vector |forms|.
  // The list will contain all possibly relevant entries to the observed |form|,
  // including blacklisted matches.
  bool GetLogins(const webkit_glue::PasswordForm& form,
                 std::vector<webkit_glue::PasswordForm*>* forms) const;

  // Loads the complete list of password forms into the specified vector |forms|
  // if include_blacklisted is true, otherwise only loads those which are
  // actually autofillable; i.e haven't been blacklisted by the user selecting
  // the 'Never for this site' button.
  bool GetAllLogins(std::vector<webkit_glue::PasswordForm*>* forms,
                    bool include_blacklisted) const;

 protected:
  // Returns an encrypted version of plain_text.
  virtual std::string EncryptedString(const std::wstring& plain_text) const = 0;

  // Returns a decrypted version of cipher_text.
  virtual std::wstring DecryptedString(const std::string& cipher_text)
      const = 0;

  bool InitLoginsTable();
  void MigrateOldVersionsAsNeeded();

 private:
  // Fills |form| from the values in the given statement (which is assumed to
  // be of the form used by GetLogins/GetAllLogins).
  void InitPasswordFormFromStatement(webkit_glue::PasswordForm* form,
                                     SQLStatement* s) const;

  sqlite3* db_;
  MetaTableHelper meta_table_;

  DISALLOW_COPY_AND_ASSIGN(LoginDatabase);
};


#endif  // CHROME_BROWSER_PASSWORD_MANAGER_LOGIN_DATABASE_H_
