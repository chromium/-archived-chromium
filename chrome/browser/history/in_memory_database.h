// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_MEMORY_DB_H__
#define CHROME_BROWSER_HISTORY_HISTORY_MEMORY_DB_H__

#include "base/basictypes.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/common/sqlite_compiled_statement.h"

struct sqlite3;

namespace history {

// Class used for a fast in-memory cache of typed URLs. Used for inline
// autocomplete since it is fast enough to be called synchronously as the user
// is typing.
class InMemoryDatabase : public URLDatabase {
 public:
  InMemoryDatabase();
  virtual ~InMemoryDatabase();

  // Creates an empty in-memory database.
  bool InitFromScratch();

  // Initializes the database by directly slurping the data from the given
  // file. Conceptually, the InMemoryHistoryBackend should do the populating
  // after this object does some common initialization, but that would be
  // much slower.
  bool InitFromDisk(const std::wstring& history_name);

 protected:
  // Implemented for URLDatabase.
  virtual sqlite3* GetDB();
  virtual SqliteStatementCache& GetStatementCache();

 private:
  // Initializes the database connection, this is the shared code between
  // InitFromScratch() and InitFromDisk() above. Returns true on success.
  bool InitDB();

  // The close scoper will free the database and delete the statement cache in
  // the correct order automatically when we are destroyed.
  DBCloseScoper db_closer_;
  sqlite3* db_;
  SqliteStatementCache* statement_cache_;

  DISALLOW_EVIL_CONSTRUCTORS(InMemoryDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_MEMORY_DB_H__
