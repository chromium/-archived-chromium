// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/meta_table_helper.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/sqlite_utils.h"

// Key used in our meta table for version numbers.
static const char kVersionKey[] = "version";
static const char kCompatibleVersionKey[] = "last_compatible_version";

// static
void MetaTableHelper::PrimeCache(const std::string& db_name, sqlite3* db) {
  // A statement must be open for the preload command to work. If the meta
  // table doesn't exist, it probably means this is a new database and there
  // is nothing to preload (so it's OK we do nothing).
  SQLStatement dummy;
  if (!DoesSqliteTableExist(db, db_name.c_str(), "meta"))
    return;
  std::string sql("SELECT * from ");
  appendMetaTableName(db_name, &sql);
  if (dummy.prepare(db, sql.c_str()) != SQLITE_OK)
    return;
  if (dummy.step() != SQLITE_ROW)
    return;

  sqlite3Preload(db);
}

MetaTableHelper::MetaTableHelper() : db_(NULL) {
}

MetaTableHelper::~MetaTableHelper() {
}

bool MetaTableHelper::Init(const std::string& db_name,
                           int version,
                           int compatible_version,
                           sqlite3* db) {
  DCHECK(!db_ && db);
  db_ = db;
  db_name_ = db_name;
  if (!DoesSqliteTableExist(db_, db_name.c_str(), "meta")) {
    // Build the sql.
    std::string sql("CREATE TABLE ");
    appendMetaTableName(db_name, &sql);
    sql.append("(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
               "value LONGVARCHAR)");

    if (sqlite3_exec(db_, sql.c_str(), NULL, NULL, NULL) != SQLITE_OK)
      return false;

    // Note: there is no index over the meta table. We currently only have a
    // couple of keys, so it doesn't matter. If we start storing more stuff in
    // there, we should create an index.
    SetVersionNumber(version);
    SetCompatibleVersionNumber(compatible_version);
  }
  return true;
}

bool MetaTableHelper::SetValue(const std::string& key,
                               const std::wstring& value) {
  SQLStatement s;
  if (!PrepareSetStatement(&s, key))
    return false;
  s.bind_wstring(1, value);
  return s.step() == SQLITE_DONE;
}

bool MetaTableHelper::GetValue(const std::string& key,
                               std::wstring* value) {
  DCHECK(value);
  SQLStatement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  s.column_wstring(0, value);
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

// static
void MetaTableHelper::appendMetaTableName(const std::string& db_name,
                                          std::string* sql) {
  if (!db_name.empty()) {
    sql->append(db_name);
    sql->push_back('.');
  }
  sql->append("meta");
}

bool MetaTableHelper::PrepareSetStatement(SQLStatement* statement,
                                          const std::string& key) {
  DCHECK(db_ && statement);
  std::string sql("INSERT OR REPLACE INTO ");
  appendMetaTableName(db_name_, &sql);
  sql.append("(key,value) VALUES(?,?)");
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
  appendMetaTableName(db_name_, &sql);
  sql.append(" WHERE key = ?");
  if (statement->prepare(db_, sql.c_str()) != SQLITE_OK) {
    NOTREACHED() << sqlite3_errmsg(db_);
    return false;
  }
  statement->bind_string(0, key);
  if (statement->step() != SQLITE_ROW)
    return false;
  return true;
}
