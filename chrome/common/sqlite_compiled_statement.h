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

#ifndef CHROME_COMMON_SQLITE_COMPILED_STATEMENT__
#define CHROME_COMMON_SQLITE_COMPILED_STATEMENT__

#include <map>
#include <string>

#include "base/logging.h"
#include "chrome/common/sqlite_utils.h"
#include "chrome/third_party/sqlite/sqlite3.h"

// Stores a list of precompiled sql statements for a database. Each statement
// is given a unique name by the caller.
//
// Note: see comments on descructor.
class SqliteStatementCache {
 public:
  // You must call set_db before anything else if you use this constructor.
  SqliteStatementCache() : db_(NULL) {
  }

  SqliteStatementCache(sqlite3* db) : db_(db) {
  }

  // This object must be deleted before the sqlite connection it is associated
  // with. Otherwise, sqlite seems to keep the file open because there are open
  // statements.
  ~SqliteStatementCache();

  void set_db(sqlite3* db) {
    DCHECK(!db_) << "Setting the database twice";
    db_ = db;
  }

  // Creates or retrieves a cached SQL statement identified by the given
  // (name, number) pair.
  //
  // The name and number can be anything the caller wants, but must uniquely
  // identify the SQL. The caller must ensure that every call with the same
  // number and name has the same SQL.
  //
  // In practice the number and name is supposed to be a file and line number.
  // (See the SQLITE_UNIQUE_STATEMENT macro below.) Recommended practice is to
  // use 0 for the function number if you are not using this scheme, and just
  // use a name you like.
  //
  // On error, NULL is returned. Otherwise, the statement for the given SQL is
  // returned. This pointer is cached and owned by this class.
  //
  // The caller should not cache this value since it may be used by others.
  // The caller should reset the statement when it is complete so that
  // subsequent callers do not get bound stuff.
  SQLStatement* GetStatement(const char* func_name,
                             int func_number,
                             const char* sql) {
    return InternalGetStatement(func_name, func_number, sql);
  }

  // Returns the cached statement if it has already been created, or NULL if it
  // has not.
  SQLStatement* GetExistingStatement(const char* func_name,
                                     int func_number) {
    return InternalGetStatement(func_name, func_number, NULL);
  }

 private:
  // The key used for precompiled function lookup.
  struct FuncID {
    int number;
    std::string name;

    // Used as a key in the map below, so we need this comparator.
    bool operator<(const FuncID& other) const;
  };

  // Backend for GetStatement and GetExistingStatement. If sql is NULL, we will
  // only look for an existing statement and return NULL if there is not a
  // matching one. If it is non-NULL, we will create it if it doesn't exist.
  SQLStatement* InternalGetStatement(const char* func_name,
                                     int func_number,
                                     const char* sql);

  sqlite3* db_;

  // This object owns the statement pointers, which it must manually free.
  typedef std::map<FuncID, SQLStatement*> StatementMap;
  StatementMap statements_;

  DISALLOW_EVIL_CONSTRUCTORS(SqliteStatementCache);
};

// Automatically creates or retrieves a statement from the given cache, and
// automatically resets the statement when it goes out of scope.
class SqliteCompiledStatement {
 public:
  // See SqliteStatementCache::GetStatement for a description of these args.
  SqliteCompiledStatement(const char* func_name,
                          int func_number,
                          SqliteStatementCache& cache,
                          const char* sql);
  ~SqliteCompiledStatement();

  // Call to see if this statement is valid or not. Using this statement will
  // segfault if it is not valid.
  bool is_valid() const { return !!statement_; }

  // Allow accessing this object to be like accessing a statement for
  // convenience. The caller must ensure the statement is_valid() before using
  // these two functions.
  SQLStatement& operator*() {
    DCHECK(statement_) << "Should check is_valid() before using the statement.";
    return *statement_;
  }
  SQLStatement* operator->() {
    DCHECK(statement_) << "Should check is_valid() before using the statement.";
    return statement_;
  }
  SQLStatement* statement() {
    DCHECK(statement_) << "Should check is_valid() before using the statement.";
    return statement_;
  }

 private:
  // The sql statement if valid, NULL if not valid. This pointer is NOT owned
  // by this class, it is owned by the statement cache object.
  SQLStatement* statement_;

  DISALLOW_EVIL_CONSTRUCTORS(SqliteCompiledStatement);
};

// Creates a compiled statement that has a unique name based on the file and
// line number. Example:
//
//     SQLITE_UNIQUE_STATEMENT(var_name, cache, "SELECT * FROM foo");
//     if (!var_name.is_valid())
//       return oops;
//     var_name->bind_XXX(
//     var_name->step();
//     ...
#define SQLITE_UNIQUE_STATEMENT(var_name, cache, sql) \
    SqliteCompiledStatement var_name(__FILE__, __LINE__, cache, sql)

#endif  // CHROME_COMMON_SQLITE_COMPILED_STATEMENT__
