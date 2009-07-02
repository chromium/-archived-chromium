// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/login_database.h"

#include <algorithm>
#include <limits>

#include "base/file_path.h"
#include "base/time.h"
#include "chrome/common/sqlite_utils.h"

using webkit_glue::PasswordForm;

static const int kCurrentVersionNumber = 1;
static const int kCompatibleVersionNumber = 1;

// Convenience enum for interacting with SQL queries that use all the columns.
typedef enum {
  COLUMN_ORIGIN_URL = 0,
  COLUMN_ACTION_URL,
  COLUMN_USERNAME_ELEMENT,
  COLUMN_USERNAME_VALUE,
  COLUMN_PASSWORD_ELEMENT,
  COLUMN_PASSWORD_VALUE,
  COLUMN_SUBMIT_ELEMENT,
  COLUMN_SIGNON_REALM,
  COLUMN_SSL_VALID,
  COLUMN_PREFERRED,
  COLUMN_DATE_CREATED,
  COLUMN_BLACKLISTED_BY_USER,
  COLUMN_SCHEME
} LoginTableColumns;

LoginDatabase::LoginDatabase() : db_(NULL) {
}

LoginDatabase::~LoginDatabase() {
  if (db_) {
    sqlite3_close(db_);
    db_ = NULL;
  }
}

bool LoginDatabase::Init(const FilePath& db_path) {
  if (OpenSqliteDb(db_path, &db_) != SQLITE_OK) {
    LOG(WARNING) << "Unable to open the password store database.";
    return false;
  }

  // Set pragmas for a small, private database (based on WebDatabase).
  sqlite3_exec(db_, "PRAGMA page_size=2048", NULL, NULL, NULL);
  sqlite3_exec(db_, "PRAGMA cache_size=32", NULL, NULL, NULL);
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  SQLTransaction transaction(db_);
  transaction.Begin();

  // Check the database version.
  if (!meta_table_.Init(std::string(), kCurrentVersionNumber,
                        kCompatibleVersionNumber, db_)) {
    return false;
  }
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Password store database is too new.";
    return false;
  }

  // Initialize the tables.
  if (!InitLoginsTable()) {
    LOG(WARNING) << "Unable to initialize the password store database.";
    return false;
  }

  // If the file on disk is an older database version, bring it up to date.
  MigrateOldVersionsAsNeeded();

  return (transaction.Commit() == SQLITE_OK);
}

void LoginDatabase::MigrateOldVersionsAsNeeded() {
  switch (meta_table_.GetVersionNumber()) {
    case kCurrentVersionNumber:
      // No migration needed.
      return;
  }
}

bool LoginDatabase::InitLoginsTable() {
  if (!DoesSqliteTableExist(db_, "logins")) {
    if (sqlite3_exec(db_, "CREATE TABLE logins ("
                     "origin_url VARCHAR NOT NULL, "
                     "action_url VARCHAR, "
                     "username_element VARCHAR, "
                     "username_value VARCHAR, "
                     "password_element VARCHAR, "
                     "password_value BLOB, "
                     "submit_element VARCHAR, "
                     "signon_realm VARCHAR NOT NULL,"
                     "ssl_valid INTEGER NOT NULL,"
                     "preferred INTEGER NOT NULL,"
                     "date_created INTEGER NOT NULL,"
                     "blacklisted_by_user INTEGER NOT NULL,"
                     "scheme INTEGER NOT NULL,"
                     "UNIQUE "
                     "(origin_url, username_element, "
                     "username_value, password_element, "
                     "submit_element, signon_realm))",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
    if (sqlite3_exec(db_, "CREATE INDEX logins_signon ON "
                     "logins (signon_realm)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}


bool LoginDatabase::AddLogin(const PasswordForm& form) {
  SQLStatement s;
  // You *must* change LoginTableColumns if this query changes.
  if (s.prepare(db_,
                "INSERT OR REPLACE INTO logins "
                "(origin_url, action_url, username_element, username_value, "
                " password_element, password_value, submit_element, "
                " signon_realm, ssl_valid, preferred, date_created, "
                " blacklisted_by_user, scheme) "
                "VALUES "
                "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_string(COLUMN_ORIGIN_URL, form.origin.spec());
  s.bind_string(COLUMN_ACTION_URL, form.action.spec());
  s.bind_wstring(COLUMN_USERNAME_ELEMENT, form.username_element);
  s.bind_wstring(COLUMN_USERNAME_VALUE, form.username_value);
  s.bind_wstring(COLUMN_PASSWORD_ELEMENT, form.password_element);
  std::string encrypted_password = EncryptedString(form.password_value);
  s.bind_blob(COLUMN_PASSWORD_VALUE, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s.bind_wstring(COLUMN_SUBMIT_ELEMENT, form.submit_element);
  s.bind_string(COLUMN_SIGNON_REALM, form.signon_realm);
  s.bind_int(COLUMN_SSL_VALID, form.ssl_valid);
  s.bind_int(COLUMN_PREFERRED, form.preferred);
  s.bind_int64(COLUMN_DATE_CREATED, form.date_created.ToTimeT());
  s.bind_int(COLUMN_BLACKLISTED_BY_USER, form.blacklisted_by_user);
  s.bind_int(COLUMN_SCHEME, form.scheme);
  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool LoginDatabase::UpdateLogin(const PasswordForm& form, int* items_changed) {
  SQLStatement s;
  if (s.prepare(db_, "UPDATE logins SET "
                "action_url = ?, "
                "password_value = ?, "
                "ssl_valid = ?, "
                "preferred = ? "
                "WHERE origin_url = ? AND "
                "username_element = ? AND "
                "username_value = ? AND "
                "password_element = ? AND "
                "signon_realm = ?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_string(0, form.action.spec());
  std::string encrypted_password = EncryptedString(form.password_value);
  s.bind_blob(1, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s.bind_int(2, form.ssl_valid);
  s.bind_int(3, form.preferred);
  s.bind_string(4, form.origin.spec());
  s.bind_wstring(5, form.username_element);
  s.bind_wstring(6, form.username_value);
  s.bind_wstring(7, form.password_element);
  s.bind_string(8, form.signon_realm);

  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  if (items_changed) {
    *items_changed = s.changes();
  }
  return true;
}

bool LoginDatabase::RemoveLogin(const PasswordForm& form) {
  SQLStatement s;
  // Remove a login by UNIQUE-constrained fields.
  if (s.prepare(db_,
                "DELETE FROM logins WHERE "
                "origin_url = ? AND "
                "username_element = ? AND "
                "username_value = ? AND "
                "password_element = ? AND "
                "submit_element = ? AND "
                "signon_realm = ? ") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_string(0, form.origin.spec());
  s.bind_wstring(1, form.username_element);
  s.bind_wstring(2, form.username_value);
  s.bind_wstring(3, form.password_element);
  s.bind_wstring(4, form.submit_element);
  s.bind_string(5, form.signon_realm);

  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool LoginDatabase::RemoveLoginsCreatedBetween(const base::Time delete_begin,
                                               const base::Time delete_end) {
  SQLStatement s;
  if (s.prepare(db_,
                "DELETE FROM logins WHERE "
                "date_created >= ? AND date_created < ?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_int64(0, delete_begin.ToTimeT());
  s.bind_int64(1, delete_end.is_null() ? std::numeric_limits<int64>::max()
                                       : delete_end.ToTimeT());

  return s.step() == SQLITE_DONE;
}

void LoginDatabase::InitPasswordFormFromStatement(PasswordForm* form,
                                                  SQLStatement* s) const {
  std::string tmp;
  s->column_string(COLUMN_ORIGIN_URL, &tmp);
  form->origin = GURL(tmp);
  s->column_string(COLUMN_ACTION_URL, &tmp);
  form->action = GURL(tmp);
  s->column_wstring(COLUMN_USERNAME_ELEMENT, &form->username_element);
  s->column_wstring(COLUMN_USERNAME_VALUE, &form->username_value);
  s->column_wstring(COLUMN_PASSWORD_ELEMENT, &form->password_element);
  std::string encrypted_password;
  s->column_blob_as_string(COLUMN_PASSWORD_VALUE, &encrypted_password);
  form->password_value = DecryptedString(encrypted_password);
  s->column_wstring(COLUMN_SUBMIT_ELEMENT, &form->submit_element);
  s->column_string(COLUMN_SIGNON_REALM, &tmp);
  form->signon_realm = tmp;
  form->ssl_valid = (s->column_int(COLUMN_SSL_VALID) > 0);
  form->preferred = (s->column_int(COLUMN_PREFERRED) > 0);
  form->date_created = base::Time::FromTimeT(
      s->column_int64(COLUMN_DATE_CREATED));
  form->blacklisted_by_user = (s->column_int(COLUMN_BLACKLISTED_BY_USER) > 0);
  int scheme_int = s->column_int(COLUMN_SCHEME);
  DCHECK((scheme_int >= 0) && (scheme_int <= PasswordForm::SCHEME_OTHER));
  form->scheme = static_cast<PasswordForm::Scheme>(scheme_int);
}

bool LoginDatabase::GetLogins(const PasswordForm& form,
                              std::vector<PasswordForm*>* forms) const {
  DCHECK(forms);
  SQLStatement s;
  // You *must* change LoginTableColumns if this query changes.
  if (s.prepare(db_,
                "SELECT origin_url, action_url, "
                "username_element, username_value, "
                "password_element, password_value, "
                "submit_element, signon_realm, ssl_valid, preferred, "
                "date_created, blacklisted_by_user, scheme FROM logins "
                "WHERE signon_realm == ? ") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_string(0, form.signon_realm);

  int result;
  while ((result = s.step()) == SQLITE_ROW) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, &s);

    forms->push_back(new_form);
  }
  return result == SQLITE_DONE;
}

bool LoginDatabase::GetAllLogins(std::vector<PasswordForm*>* forms,
                                 bool include_blacklisted) const {
  DCHECK(forms);
  SQLStatement s;
  // You *must* change LoginTableColumns if this query changes.
  std::string stmt = "SELECT origin_url, action_url, "
                     "username_element, username_value, "
                     "password_element, password_value, "
                     "submit_element, signon_realm, ssl_valid, preferred, "
                     "date_created, blacklisted_by_user, scheme FROM logins ";
  if (!include_blacklisted)
    stmt.append("WHERE blacklisted_by_user == 0 ");
  stmt.append("ORDER BY origin_url");

  if (s.prepare(db_, stmt.c_str()) != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  int result;
  while ((result = s.step()) == SQLITE_ROW) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, &s);

    forms->push_back(new_form);
  }
  return result == SQLITE_DONE;
}
