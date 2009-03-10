// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_URL_DATABASE_H__
#define CHROME_BROWSER_HISTORY_URL_DATABASE_H__

#include "base/basictypes.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/search_engines/template_url.h"

// Temporary until DBCloseScoper moves elsewhere.
#include "chrome/common/sqlite_compiled_statement.h"

class GURL;
struct sqlite3;
class SqliteStatementCache;

namespace history {

class VisitDatabase;  // For friend statement.

// This class is here temporarily.
// TODO(brettw) Figure out a better place for this or obsolete it.
//
// Helper class that closes the DB, deletes the statement cache, and zeros out
// the pointer when it goes out of scope, does nothing once success is called.
//
// Can either be used by the owner of the DB to automatically close it, or
// during initialization so that it is automatically closed on failure.
//
// properly maintained by Init() on failure. If |statement_cache| is non NULL,
// it is assumed to be bound with |db| and will be cleaned up before the
// database is closed.
class DBCloseScoper {
 public:
  DBCloseScoper() : db_(NULL), statement_cache_(NULL) {
  }

  // The statement cache must be allocated on the heap. The DB must be
  // allocated by sqlite.
  DBCloseScoper(sqlite3** db, SqliteStatementCache** statement_cache)
      : db_(db),
        statement_cache_(statement_cache) {
  }

  ~DBCloseScoper() {
    if (db_) {
      if (*statement_cache_) {
        delete *statement_cache_;
        *statement_cache_ = NULL;
      }

      sqlite3_close(*db_);
      *db_ = NULL;
    }
  }

  void Attach(sqlite3** db, SqliteStatementCache** statement_cache) {
    DCHECK(db_ == NULL && statement_cache_ == NULL);
    db_ = db;
    statement_cache_ = statement_cache;
  }

  void Detach() {
    db_ = NULL;
    statement_cache_ = NULL;
  }

 private:
  sqlite3** db_;
  SqliteStatementCache** statement_cache_;
};

// Encapsulates an SQL database that holds URL info.  This is a subset of the
// full history data.  We split this class' functionality out from the larger
// HistoryDatabase class to support maintaining separate databases of URLs with
// different capabilities (for example, in-memory, or archived).
//
// This is refcounted to support calling InvokeLater() with some of its methods
// (necessary to maintain ordering of DB operations).
class URLDatabase {
 public:
  // Must call CreateURLTable() and CreateURLIndexes() before using to make
  // sure the database is initialized.
  URLDatabase();

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~URLDatabase();

  // Converts a GURL to a string used in the history database. We plan to
  // do more complex operations than just getting the spec out involving
  // punycode, so this function should be used instead of url.spec() when
  // interacting with the database.
  //
  // TODO(brettw) this should be moved out of the public section and the
  // entire public HistoryDatabase interface should use GURL. This should
  // also probably return a string instead since that is what the DB uses
  // internally and we can avoid the extra conversion.
  static std::string GURLToDatabaseURL(const GURL& url);

  // URL table functions -------------------------------------------------------

  // Looks up a url given an id. Fills info with the data. Returns true on
  // success and false otherwise.
  bool GetURLRow(URLID url_id, URLRow* info);

  // Looks up the given URL and if it exists, fills the given pointers with the
  // associated info and returns the ID of that URL. If the info pointer is
  // NULL, no information about the URL will be filled in, only the ID will be
  // returned. Returns 0 if the URL was not found.
  URLID GetRowForURL(const GURL& url, URLRow* info);

  // Given an already-existing row in the URL table, updates that URL's stats.
  // This can not change the URL.  Returns true on success.
  //
  // This will NOT update the title used for full text indexing. If you are
  // setting the title, call SetPageIndexedData with the new title.
  bool UpdateURLRow(URLID url_id, const URLRow& info);

  // Adds a line to the URL database with the given information and returns the
  // row ID. A row with the given URL must not exist. Returns 0 on error.
  //
  // This does NOT add a row to the full text search database. Use
  // HistoryDatabase::SetPageIndexedData to do this.
  URLID AddURL(const URLRow& info) {
    return AddURLInternal(info, false);
  }

  // Delete the row of the corresponding URL. Only the row in the URL table
  // will be deleted, not any other data that may refer to it. Returns true if
  // the row existed and was deleted.
  bool DeleteURLRow(URLID id);

  // URL mass-deleting ---------------------------------------------------------

  // Begins the mass-deleting operation by creating a temporary URL table.
  // The caller than adds the URLs it wants to preseve to the temporary table,
  // and then deletes everything else by calling CommitTemporaryURLTable().
  // Returns true on success.
  bool CreateTemporaryURLTable();

  // Adds a row to the temporary URL table. This must be called between
  // CreateTemporaryURLTable() and CommitTemporaryURLTable() (see those for more
  // info). The ID of the URL will change in the temporary table, so the new ID
  // is returned. Returns 0 on failure.
  URLID AddTemporaryURL(const URLRow& row) {
    return AddURLInternal(row, true);
  }

  // Ends the mass-deleting by replacing the original URL table with the
  // temporary one created in CreateTemporaryURLTable. Returns true on success.
  //
  // This function does not create the supplimentary indices. It is virtual so
  // that the main history database can provide this additional behavior.
  virtual bool CommitTemporaryURLTable();

  // Enumeration ---------------------------------------------------------------

  // A basic enumerator to enumerate urls
  class URLEnumerator {
   public:
    URLEnumerator() : initialized_(false) {
    }

    // Retreives the next url. Returns false if no more urls are available
    bool GetNextURL(history::URLRow* r);

   private:
    friend class URLDatabase;

    bool initialized_;
    SQLStatement statement_;
  };

  // Initializes the given enumerator to enumerator all URLs in the database
  bool InitURLEnumeratorForEverything(URLEnumerator* enumerator);

  // Favicons ------------------------------------------------------------------

  // Check whether a favicon is used by any URLs in the database.
  bool IsFavIconUsed(FavIconID favicon_id);

  // Autocomplete --------------------------------------------------------------

  // Fills the given array with URLs matching the given prefix. They will be
  // sorted by typed count, then by visit count, then by visit date (most
  // recent first) up to the given maximum number. Called by HistoryURLProvider.
  void AutocompleteForPrefix(const std::wstring& prefix,
                             size_t max_results,
                             std::vector<URLRow>* results);

  // Tries to find the shortest URL beginning with |base| that strictly
  // prefixes |url|, and has minimum visit_ and typed_counts as specified.
  // If found, fills in |info| and returns true; otherwise returns false,
  // leaving |info| unchanged.
  // We allow matches of exactly |base| iff |allow_base| is true.
  bool FindShortestURLFromBase(const std::string& base,
                               const std::string& url,
                               int min_visits,
                               int min_typed,
                               bool allow_base,
                               history::URLRow* info);

  // Keyword Search Terms ------------------------------------------------------

  // Sets the search terms for the specified url/keyword pair.
  bool SetKeywordSearchTermsForURL(URLID url_id,
                                   TemplateURL::IDType keyword_id,
                                   const std::wstring& term);

  // Deletes all search terms for the specified keyword that have been added by
  // way of SetKeywordSearchTermsForURL.
  void DeleteAllSearchTermsForKeyword(TemplateURL::IDType keyword_id);

  // Returns up to max_count of the most recent search terms for the specified
  // keyword.
  void GetMostRecentKeywordSearchTerms(
      TemplateURL::IDType keyword_id,
      const std::wstring& prefix,
      int max_count,
      std::vector<KeywordSearchTermVisit>* matches);

  // Migration -----------------------------------------------------------------

  // Do to a bug we were setting the favicon of about:blank. This forces
  // about:blank to have no icon or title. Returns true on success, false if
  // the favicon couldn't be updated.
  bool MigrateFromVersion11ToVersion12();

 protected:
  friend class VisitDatabase;

  // See HISTORY_URL_ROW_FIELDS below.
  static const char kURLRowFields[];

  // The number of fiends in kURLRowFields. If callers need additional
  // fields, they can add their 0-based index to this value to get the index of
  // fields following kURLRowFields.
  static const int kNumURLRowFields;

  // Drops the starred_id column from urls, returning true on success. This does
  // nothing (and returns true) if the urls doesn't contain the starred_id
  // column.
  bool DropStarredIDFromURLs();

  // Initialization functions. The indexing functions are separate from the
  // table creation functions so the in-memory database and the temporary tables
  // used when clearing history can populate the table and then create the
  // index, which is faster than the reverse.
  //
  // is_temporary is false when generating the "regular" URLs table. The expirer
  // sets this to true to generate the  temporary table, which will have a
  // different name but the same schema.
  bool CreateURLTable(bool is_temporary);
  // We have two tiers of indices for the URL table. The main tier is used by
  // all URL databases, and is an index over the URL itself. The main history
  // DB also creates indices over the favicons and bookmark IDs. The archived
  // and in-memory databases don't need these supplimentary indices so we can
  // save space by not creating them.
  void CreateMainURLIndex();
  void CreateSupplimentaryURLIndices();

  // Ensures the keyword search terms table exists.
  bool InitKeywordSearchTermsTable();

  // Deletes the keyword search terms table.
  bool DropKeywordSearchTermsTable();

  // Inserts the given URL row into the URLs table, using the regular table
  // if is_temporary is false, or the temporary URL table if is temporary is
  // true. The temporary table may only be used in between
  // CreateTemporaryURLTable() and CommitTemporaryURLTable().
  URLID AddURLInternal(const URLRow& info, bool is_temporary);

  // Convenience to fill a history::URLRow. Must be in sync with the fields in
  // kHistoryURLRowFields.
  static void FillURLRow(SQLStatement& s, URLRow* i);

  // Returns the database and statement cache for the functions in this
  // interface. The decendent of this class implements these functions to
  // return its objects.
  virtual sqlite3* GetDB() = 0;
  virtual SqliteStatementCache& GetStatementCache() = 0;

 private:
  // True if InitKeywordSearchTermsTable() has been invoked. Not all subclasses
  // have keyword search terms.
  bool has_keyword_search_terms_;

  DISALLOW_EVIL_CONSTRUCTORS(URLDatabase);
};

// The fields and order expected by FillURLRow(). ID is guaranteed to be first
// so that DISTINCT can be prepended to get distinct URLs.
//
// This is available BOTH as a macro and a static string (kURLRowFields). Use
// the macro if you want to put this in the middle of an otherwise constant
// string, it will save time doing string appends. If you have to build a SQL
// string dynamically anyway, use the constant, it will save space.
#define HISTORY_URL_ROW_FIELDS \
    " urls.id, urls.url, urls.title, urls.visit_count, urls.typed_count, " \
    "urls.last_visit_time, urls.hidden, urls.favicon_id "

}  // history

#endif  // CHROME_BROWSER_HISTORY_URL_DATABASE_H__
