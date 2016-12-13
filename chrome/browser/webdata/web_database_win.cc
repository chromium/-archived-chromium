// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_database.h"

#include "base/logging.h"
#include "base/time.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/common/sqlite_utils.h"

bool WebDatabase::AddIE7Login(const IE7PasswordInfo& info) {
  SQLStatement s;
  if (s.prepare(db_,
                "INSERT OR REPLACE INTO ie7_logins "
                "(url_hash, password_value, date_created) "
                "VALUES (?, ?, ?)") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_wstring(0, info.url_hash);
  s.bind_blob(1, &info.encrypted_data.front(),
              static_cast<int>(info.encrypted_data.size()));
  s.bind_int64(2, info.date_created.ToTimeT());
  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveIE7Login(const IE7PasswordInfo& info) {
  SQLStatement s;
  // Remove a login by UNIQUE-constrained fields.
  if (s.prepare(db_,
                "DELETE FROM ie7_logins WHERE "
                "url_hash = ?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_wstring(0, info.url_hash);

  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::GetIE7Login(const IE7PasswordInfo& info,
                              IE7PasswordInfo* result) {
  DCHECK(result);
  SQLStatement s;
  if (s.prepare(db_,
                "SELECT password_value, date_created FROM ie7_logins "
                "WHERE url_hash == ? ") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_wstring(0, info.url_hash);

  int64 query_result = s.step();
  if (query_result == SQLITE_ROW) {
    s.column_blob_as_vector(0, &result->encrypted_data);
    result->date_created = base::Time::FromTimeT(s.column_int64(1));
    result->url_hash = info.url_hash;
    s.step();
  }
  return query_result == SQLITE_DONE;
}
