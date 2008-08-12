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

#include "chrome/browser/meta_table_helper.h"

#include <windows.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/sqlite_utils.h"

// Key used in our meta table for version numbers.
static const char kVersionKey[] = "version";
static const char kCompatibleVersionKey[] = "last_compatible_version";

MetaTableHelper::MetaTableHelper() : db_(NULL) {
}

MetaTableHelper::~MetaTableHelper() {
}

bool MetaTableHelper::Init(const std::string& db_name,
                           int version,
                           sqlite3* db) {
  DCHECK(!db_ && db);
  db_ = db;
  db_name_ = db_name;
  if (!DoesSqliteTableExist(db_, db_name.c_str(), "meta")) {
    // Build the sql.
    std::string sql("CREATE TABLE ");
    if (!db_name.empty()) {
      // Want a table name of the form db_name.meta
      sql.append(db_name);
      sql.push_back('.');
    }
    sql.append("meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
                    "value LONGVARCHAR)");

    if (sqlite3_exec(db_, sql.c_str(), NULL, NULL, NULL) != SQLITE_OK)
      return false;

    // Note: there is no index over the meta table. We currently only have a
    // couple of keys, so it doesn't matter. If we start storing more stuff in
    // there, we should create an index.
    SetVersionNumber(version);
    SetCompatibleVersionNumber(version);
  }
  return true;
}

bool MetaTableHelper::SetValue(const std::string& key,
                               const std::wstring& value) {
  SQLStatement s;
  if (!PrepareSetStatement(&s, key))
    return false;
  s.bind_text16(1, value.c_str());
  return s.step() == SQLITE_DONE;
}

bool MetaTableHelper::GetValue(const std::string& key,
                               std::wstring* value) {
  DCHECK(value);
  SQLStatement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  s.column_string16(0, value);
  return true;
}

bool MetaTableHelper::SetValue(const std::string& key,
                               int value) {
  SQLStatement s;
  if (!PrepareSetStatement(&s, key))
    return false;
  s.bind_int(1, value);
  return s.step() == SQLITE_DONE;
}

bool MetaTableHelper::GetValue(const std::string& key,
                               int* value) {
  DCHECK(value);
  SQLStatement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  *value = s.column_int(0);
  return true;
}

bool MetaTableHelper::SetValue(const std::string& key,
                               int64 value) {
  SQLStatement s;
  if (!PrepareSetStatement(&s, key))
    return false;
  s.bind_int64(1, value);
  return s.step() == SQLITE_DONE;
}

bool MetaTableHelper::GetValue(const std::string& key,
                               int64* value) {
  DCHECK(value);
  SQLStatement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  *value = s.column_int64(0);
  return true;
}

void MetaTableHelper::SetVersionNumber(int version) {
  SetValue(kVersionKey, version);
}

int MetaTableHelper::GetVersionNumber() {
  int version;
  if (!GetValue(kVersionKey, &version))
    return 0;
  return version;
}

void MetaTableHelper::SetCompatibleVersionNumber(int version) {
  SetValue(kCompatibleVersionKey, version);
}

int MetaTableHelper::GetCompatibleVersionNumber() {
  int version;
  if (!GetValue(kCompatibleVersionKey, &version))
    return 0;
  return version;
}

bool MetaTableHelper::PrepareSetStatement(SQLStatement* statement,
                                          const std::string& key) {
  DCHECK(db_ && statement);
  std::string sql("INSERT OR REPLACE INTO ");
  if (db_name_.size() > 0) {
    sql.append(db_name_);
    sql.push_back('.');
  }
  sql.append("meta(key,value) VALUES(?,?)");
  if (statement->prepare(db_, sql.c_str()) != SQLITE_OK) {
    NOTREACHED() << sqlite3_errmsg(db_);
    return false;
  }
  statement->bind_text(0, key.c_str());
  return true;
}

bool MetaTableHelper::PrepareGetStatement(SQLStatement* statement,
                                          const std::string& key) {
  DCHECK(db_ && statement);
  std::string sql("SELECT value FROM ");
  if (db_name_.size() > 0) {
    sql.append(db_name_);
    sql.push_back('.');
  }
  sql.append("meta WHERE key = ?");
  if (statement->prepare(db_, sql.c_str()) != SQLITE_OK) {
    NOTREACHED() << sqlite3_errmsg(db_);
    return false;
  }
  statement->bind_string(0, key);
  if (statement->step() != SQLITE_ROW)
    return false;
  return true;
}