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
