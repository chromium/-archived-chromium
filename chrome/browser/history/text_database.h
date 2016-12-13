// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TEXT_DATABASE_H__
#define CHROME_BROWSER_HISTORY_TEXT_DATABASE_H__

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/meta_table_helper.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "googleurl/src/gurl.h"

struct sqlite3;

namespace history {

// Encapsulation of a full-text indexed database file.
class TextDatabase {
 public:
  typedef int DBIdent;

  typedef std::set<GURL> URLSet;

  // Returned from the search function.
  struct Match {
    // URL of the match.
    GURL url;

    // The title is returned because the title in the text database and the URL
    // database may differ. This happens because we capture the title when the
    // body is captured, and don't update it later.
    std::wstring title;

    // Time the page that was returned was visited.
    base::Time time;

    // Identifies any found matches in the title of the document. These are not
    // included in the snippet.
    Snippet::MatchPositions title_match_positions;

    // Snippet of the match we generated from the body.
    Snippet snippet;
  };

  // Note: You must call init which must succeed before using this class.
  //
  // Computes the matches for the query, returning results in decreasing order
  // of visit time.
  //
  // This function will attach the new database to the given database
  // connection. This allows one sqlite3 object to share many TextDatabases,
  // meaning that they will all share the same cache, which allows us to limit
  // the total size that text indexing databasii can take up.
  //
  // |file_name| is the name of the file on disk.
  //
  // ID is the identifier for the database. It should uniquely identify it among
  // other databases on disk and in the sqlite connection.
  //
  // |allow_create| indicates if we want to allow creation of the file if it
  // doesn't exist. For files associated with older time periods, we don't want
  // to create them if they don't exist, so this flag would be false.
  TextDatabase(const FilePath& path,
               DBIdent id,
               bool allow_create);
  ~TextDatabase();

  // Initializes the database connection and creates the file if the class
  // was created with |allow_create|. If the file couldn't be opened or
  // created, this will return false. No other functions should be called
  // after this.
  bool Init();

  // Allows updates to be batched. This gives higher performance when multiple
  // updates are happening because every insert doesn't require a sync to disk.
  // Transactions can be nested, only the outermost one will actually count.
  void BeginTransaction();
  void CommitTransaction();

  // For testing, returns the file name of the database so it can be deleted
  // after the test. This is valid even before Init() is called.
  const FilePath& file_name() const { return file_name_; }

  // Returns a NULL-terminated string that is the base of history index files,
  // which is the part before the database identifier. For example
  // "History Index *". This is for finding existing database files.
  static const FilePath::CharType* file_base();

  // Converts a filename on disk (optionally including a path) to a database
  // identifier. If the filename doesn't have the correct format, returns 0.
  static DBIdent FileNameToID(const FilePath& file_path);

  // Changing operations -------------------------------------------------------

  // Adds the given data to the page. Returns true on success. The data should
  // already be converted to UTF-8.
  bool AddPageData(base::Time time,
                   const std::string& url,
                   const std::string& title,
                   const std::string& contents);

  // Deletes the indexed data exactly matching the given URL/time pair.
  void DeletePageData(base::Time time, const std::string& url);

  // Optimizes the tree inside the database. This will, in addition to making
  // access faster, remove any deleted data from the database (normally it is
  // added again as "removed" and it is manually cleaned up when it decides to
  // optimize it naturally). It is bad for privacy if a user is deleting a
  // page from history but it still exists in the full text database in some
  // form. This function will clean that up.
  void Optimize();

  // Querying ------------------------------------------------------------------

  // Executes the given query. See QueryOptions for more info on input.
  //
  // The results are appended to any existing ones in |*results|, and the first
  // time considered for the output is in |first_time_searched|
  // (see QueryResults for more).
  //
  // When |options.most_recent_visit_only|, any URLs found will be added to
  // |unique_urls|. If a URL is already in the set, additional results will not
  // be added (giving the ability to uniquify URL results, with the most recent
  // If |most_recent_visit_only| is not set, |unique_urls| will be untouched.
  //
  // Callers must run QueryParser on the user text and pass the results of the
  // QueryParser to this method as the query string.
  void GetTextMatches(const std::string& query,
                      const QueryOptions& options,
                      std::vector<Match>* results,
                      URLSet* unique_urls,
                      base::Time* first_time_searched);

  // Converts the given database identifier to a filename. This does not include
  // the path, just the file and extension.
  static FilePath IDToFileName(DBIdent id);

 private:
  // Ensures that the tables and indices are created. Returns true on success.
  bool CreateTables();

  // See the constructor.
  sqlite3* db_;
  SqliteStatementCache* statement_cache_;

  const FilePath path_;
  const DBIdent ident_;
  const bool allow_create_;

  // Full file name of the file on disk, computed in Init().
  FilePath file_name_;

  // Nesting levels of transactions. Since sqlite only allows one open
  // transaction, we simulate nested transactions by mapping the outermost one
  // to a real transaction. Since this object never needs to do ROLLBACK, losing
  // the ability for all transactions to rollback is inconsequential.
  int transaction_nesting_;

  MetaTableHelper meta_table_;

  DISALLOW_EVIL_CONSTRUCTORS(TextDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TEXT_DATABASE_H__
