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

#include "chrome/common/sqlite_utils.h"

bool DoesSqliteTableExist(sqlite3* db,
                          const char* db_name,
                          const char* table_name) {
  // sqlite doesn't allow binding parameters as table names, so we have to
  // manually construct the sql
  std::string sql("SELECT name FROM ");
  if (db_name && db_name[0]) {
    sql.append(db_name);
    sql.push_back('.');
  }
  sql.append("sqlite_master WHERE type='table' AND name=?");

  SQLStatement statement;
  if (statement.prepare(db, sql.c_str()) != SQLITE_OK)
    return false;

  if (statement.bind_text(0, table_name) != SQLITE_OK)
    return false;

  // we only care about if this matched a row, not the actual data
  return sqlite3_step(statement.get()) == SQLITE_ROW;
}

bool DoesSqliteColumnExist(sqlite3* db,
                           const char* database_name,
                           const char* table_name,
                           const char* column_name,
                           const char* column_type) {
  SQLStatement s;
  std::string sql;
  sql.append("PRAGMA ");
  if (database_name && database_name[0]) {
    // optional database name specified
    sql.append(database_name);
    sql.push_back('.');
  }
  sql.append("TABLE_INFO(");
  sql.append(table_name);
  sql.append(")");

  if (s.prepare(db, sql.c_str()) != SQLITE_OK)
    return false;

  while (s.step() == SQLITE_ROW) {
    if (!strcmp(column_name, s.column_text(1))) {
      if (column_type && column_type[0])
        return !strcmp(column_type, s.column_text(2));
      return true;
    }
  }
  return false;
}

bool DoesSqliteTableHaveRow(sqlite3* db, const char* table_name) {
  SQLStatement s;
  std::string b;
  b.append("SELECT * FROM ");
  b.append(table_name);

  if (s.prepare(db, b.c_str()) != SQLITE_OK)
    return false;

  return s.step() == SQLITE_ROW;
}
