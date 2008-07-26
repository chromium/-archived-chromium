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

#ifndef CHROME_BROWSER_HISTORY_ARCHIVED_DATABASE_H__
#define CHROME_BROWSER_HISTORY_ARCHIVED_DATABASE_H__

#include "base/basictypes.h"
#include "chrome/browser/history/download_database.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/visit_database.h"
#include "chrome/browser/meta_table_helper.h"

struct sqlite3;

namespace history {

// Encapsulates the database operations for archived history.
//
// IMPORTANT NOTE: The IDs in this system for URLs and visits will be
// different than those in the main database. This is to eliminate the
// dependency between them so we can deal with each one on its own.
class ArchivedDatabase : public URLDatabase,
                         public VisitDatabase {
 public:
  // Must call Init() before using other members.
  ArchivedDatabase();
  virtual ~ArchivedDatabase();

  // Initializes the database connection. This must return true before any other
  // functions on this class are called.
  bool Init(const std::wstring& file_name);

  // Transactions on the database. We support nested transactions and only
  // commit when the outermost one is committed (sqlite doesn't support true
  // nested transactions).
  void BeginTransaction();
  void CommitTransaction();

 private:
  // Implemented for the specialized databases.
  virtual sqlite3* GetDB();
  virtual SqliteStatementCache& GetStatementCache();

  // The database.
  //
  // The close scoper will free the database and delete the statement cache in
  // the correct order automatically when we are destroyed.
  DBCloseScoper db_closer_;
  sqlite3* db_;
  SqliteStatementCache* statement_cache_;

  // The number of nested transactions currently in progress.
  int transaction_nesting_;

  MetaTableHelper meta_table_;

  DISALLOW_EVIL_CONSTRUCTORS(ArchivedDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_ARCHIVED_DATABASE_H__
