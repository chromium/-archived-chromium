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

#include "chrome/common/sqlite_compiled_statement.h"

#include "base/logging.h"
#include "chrome/common/stl_util-inl.h"

// SqliteStatementCache -------------------------------------------------------

SqliteStatementCache::~SqliteStatementCache() {
  STLDeleteContainerPairSecondPointers(statements_.begin(), statements_.end());
  statements_.clear();
  db_ = NULL;
}

void SqliteStatementCache::set_db(sqlite3* db) {
  DCHECK(!db_) << "Setting the database twice";
  db_ = db;
}

SQLStatement* SqliteStatementCache::InternalGetStatement(const char* func_name,
                                                         int func_number,
                                                         const char* sql) {
  FuncID id;
  id.name = func_name;
  id.number = func_number;

  StatementMap::const_iterator found = statements_.find(id);
  if (found != statements_.end())
    return found->second;

  if (!sql)
    return NULL;  // Don't create a new statement when we were not given SQL.

  // Create a new statement.
  SQLStatement* statement = new SQLStatement();
  if (statement->prepare(db_, sql) != SQLITE_OK) {
    const char* err_msg = sqlite3_errmsg(db_);
    NOTREACHED() << "SQL preparation error for: " << err_msg;
    return NULL;
  }

  statements_[id] = statement;
  return statement;
}

bool SqliteStatementCache::FuncID::operator<(const FuncID& other) const {
  // Compare numbers first since they are faster than strings and they are
  // almost always unique.
  if (number != other.number)
    return number < other.number;
  return name < other.name;
}

// SqliteCompiledStatement ----------------------------------------------------

SqliteCompiledStatement::SqliteCompiledStatement(const char* func_name,
                                                 int func_number,
                                                 SqliteStatementCache& cache,
                                                 const char* sql) {
  statement_ = cache.GetStatement(func_name, func_number, sql);
}

SqliteCompiledStatement::~SqliteCompiledStatement() {
  // Reset the statement so that subsequent callers don't get crap in it.
  if (statement_)
    statement_->reset();
}

SQLStatement& SqliteCompiledStatement::operator*() {
  DCHECK(statement_) << "Should check is_valid() before using the statement.";
  return *statement_;
}
SQLStatement* SqliteCompiledStatement::operator->() {
  DCHECK(statement_) << "Should check is_valid() before using the statement.";
  return statement_;
}
SQLStatement* SqliteCompiledStatement::statement() {
  DCHECK(statement_) << "Should check is_valid() before using the statement.";
  return statement_;
}
