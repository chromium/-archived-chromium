// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <set>

#include "chrome/browser/history/text_database.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/sqlite_utils.h"

// There are two tables in each database, one full-text search (FTS) table which
// indexes the contents and title of the pages. The other is a regular SQLITE
// table which contains non-indexed information about the page. All columns of
// a FTS table are indexed using the text search algorithm, which isn't what we
// want for things like times. If this were in the FTS table, there would be
// different words in the index for each time number.
//
// "pages" FTS table:
//   url    URL of the page so searches will match the URL.
//   title  Title of the page.
//   body   Body of the page.
//
// "info" regular table:
//   time     Time the corresponding FTS entry was visited.
//
// We do joins across these two tables by using their internal rowids, which we
// keep in sync between the two tables. The internal rowid is the only part of
// an FTS table that is indexed like a normal table, and the index over it is
// free since sqlite always indexes the internal rowid.

using base::Time;

namespace history {

namespace {

static const int kCurrentVersionNumber = 1;
static const int kCompatibleVersionNumber = 1;

// Snippet computation relies on the index of the columns in the original
// create statement. These are the 0-based indices (as strings) of the
// corresponding columns.
const char kTitleColumnIndex[] = "1";
const char kBodyColumnIndex[] = "2";

// The string prepended to the database identifier to generate the filename.
const wchar_t kFilePrefix[] = L"History Index ";
const size_t kFilePrefixLen = arraysize(kFilePrefix) - 1;  // Don't count NULL.

// We do not allow rollback, but this simple scoper makes it easy to always
// remember to commit a begun transaction. This protects against some errors
// caused by a crash in the middle of a transaction, although doesn't give us
// the full protection of a transaction's rollback abilities.
class ScopedTransactionCommitter {
 public:
   ScopedTransactionCommitter(TextDatabase* db) : db_(db) {
    db_->BeginTransaction();
  }
  ~ScopedTransactionCommitter() {
    db_->CommitTransaction();
  }
 private:
  TextDatabase* db_;
};


}  // namespace

TextDatabase::TextDatabase(const std::wstring& path,
                           DBIdent id,
                           bool allow_create)
    : db_(NULL),
      statement_cache_(NULL),
      path_(path),
      ident_(id),
      allow_create_(allow_create),
      transaction_nesting_(0) {
  // Compute the file name.
  file_name_ = path_;
  file_util::AppendToPath(&file_name_, IDToFileName(ident_));
}

TextDatabase::~TextDatabase() {
  if (statement_cache_) {
    // Must release these statements before closing the DB.
    delete statement_cache_;
    statement_cache_ = NULL;
  }
  if (db_) {
    sqlite3_close(db_);
    db_ = NULL;
  }
}

// static
const wchar_t* TextDatabase::file_base() {
  return kFilePrefix;
}

// static
std::wstring TextDatabase::IDToFileName(DBIdent id) {
  // Identifiers are intended to be a combination of the year and month, for
  // example, 200801 for January 2008. We convert this to
  // "History Index 2008-01". However, we don't make assumptions about this
  // scheme: the caller should assign IDs as it feels fit with the knowledge
  // that they will apppear on disk in this form.
  return StringPrintf(L"%ls%d-%02d", file_base(), id / 100, id % 100);
}

// static
TextDatabase::DBIdent TextDatabase::FileNameToID(const std::wstring& file_path){
  std::wstring file_name = file_util::GetFilenameFromPath(file_path);

  // We don't actually check the prefix here. Since the file system could
  // be case insensitive in ways we can't predict (NTFS), checking could
  // potentially be the wrong thing to do. Instead, we just look for a suffix.
  static const size_t kIDStringLength = 7;  // Room for "xxxx-xx".
  if (file_name.length() < kIDStringLength)
    return 0;
  const std::wstring suffix(&file_name[file_name.length() - kIDStringLength]);

  if (suffix.length() != kIDStringLength || suffix[4] != L'-') {
    return 0;
  }

  int year = StringToInt(WideToUTF16Hack(suffix.substr(0, 4)));
  int month = StringToInt(WideToUTF16Hack(suffix.substr(5, 2)));

  return year * 100 + month;
}

bool TextDatabase::Init() {
  // Make sure, if we're not allowed to create the file, that it exists.
  if (!allow_create_) {
    if (!file_util::PathExists(file_name_))
      return false;
  }

  // Attach the database to our index file.
  if (sqlite3_open(WideToUTF8(file_name_).c_str(), &db_) != SQLITE_OK)
    return false;
  statement_cache_ = new SqliteStatementCache(db_);

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  sqlite3_exec(db_, "PRAGMA page_size=4096", NULL, NULL, NULL);

  // The default cache size is 2000 which give >8MB of data. Since we will often
  // have 2-3 of these objects, each with their own 8MB, this adds up very fast.
  // We therefore reduce the size so when there are multiple objects, we're not
  // too big.
  sqlite3_exec(db_, "PRAGMA cache_size=512", NULL, NULL, NULL);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  // Meta table tracking version information.
  if (!meta_table_.Init(std::string(), kCurrentVersionNumber,
                        kCompatibleVersionNumber, db_))
    return false;
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    // This version is too new. We don't bother notifying the user on this
    // error, and just fail to use the file. Normally if they have version skew,
    // they will get it for the main history file and it won't be necessary
    // here. If that's not the case, since this is only indexed data, it's
    // probably better to just not give FTS results than strange errors when
    // everything else is working OK.
    LOG(WARNING) << "Text database is too new.";
    return false;
  }

  return CreateTables();
}

void TextDatabase::BeginTransaction() {
  if (!transaction_nesting_)
    sqlite3_exec(db_, "BEGIN TRANSACTION", NULL, NULL, NULL);
  transaction_nesting_++;
}

void TextDatabase::CommitTransaction() {
  DCHECK(transaction_nesting_);
  transaction_nesting_--;
  if (!transaction_nesting_)
    sqlite3_exec(db_, "COMMIT", NULL, NULL, NULL);
}

bool TextDatabase::CreateTables() {
  // FTS table of page contents.
  if (!DoesSqliteTableExist(db_, "pages")) {
    if (sqlite3_exec(db_,
                     "CREATE VIRTUAL TABLE pages USING fts2("
                     "TOKENIZE icu,"
                     "url LONGVARCHAR,"
                     "title LONGVARCHAR,"
                     "body LONGVARCHAR)", NULL, NULL, NULL) != SQLITE_OK)
      return false;
  }

  // Non-FTS table containing URLs and times so we can efficiently find them
  // using a regular index (all FTS columns are special and are treated as
  // full-text-search, which is not what we want when retrieving this data).
  if (!DoesSqliteTableExist(db_, "info")) {
    // Note that there is no point in creating an index over time. Since
    // we must always query the entire FTS table (it can not efficiently do
    // subsets), we will always end up doing that first, and joining the info
    // table off of that.
    if (sqlite3_exec(db_, "CREATE TABLE info(time INTEGER NOT NULL)",
                     NULL, NULL, NULL) != SQLITE_OK)
      return false;
  }

  // Create the index. This will fail when the index already exists, so we just
  // ignore the error.
  sqlite3_exec(db_, "CREATE INDEX info_time ON info(time)", NULL, NULL, NULL);
  return true;
}

bool TextDatabase::AddPageData(Time time,
                               const std::string& url,
                               const std::string& title,
                               const std::string& contents) {
  ScopedTransactionCommitter committer(this);

  // Add to the pages table.
  SQLITE_UNIQUE_STATEMENT(add_to_pages, *statement_cache_,
      "INSERT INTO pages (url, title, body) VALUES (?,?,?)");
  if (!add_to_pages.is_valid())
    return false;
  add_to_pages->bind_string(0, url);
  add_to_pages->bind_string(1, title);
  add_to_pages->bind_string(2, contents);
  if (add_to_pages->step() != SQLITE_DONE) {
    NOTREACHED() << sqlite3_errmsg(db_);
    return false;
  }

  int64 rowid = sqlite3_last_insert_rowid(db_);

  // Add to the info table with the same rowid.
  SQLITE_UNIQUE_STATEMENT(add_to_info, *statement_cache_,
      "INSERT INTO info (rowid, time) VALUES (?,?)");
  if (!add_to_info.is_valid())
    return false;
  add_to_info->bind_int64(0, rowid);
  add_to_info->bind_int64(1, time.ToInternalValue());
  if (add_to_info->step() != SQLITE_DONE) {
    NOTREACHED() << sqlite3_errmsg(db_);
    return false;
  }

  return true;
}

void TextDatabase::DeletePageData(Time time, const std::string& url) {
  // First get all rows that match. Selecing on time (which has an index) allows
  // us to avoid brute-force searches on the full-text-index table (there will
  // generally be only one match per time).
  SQLITE_UNIQUE_STATEMENT(select_ids, *statement_cache_,
      "SELECT info.rowid "
      "FROM info JOIN pages ON info.rowid = pages.rowid "
      "WHERE info.time=? AND pages.url=?");
  if (!select_ids.is_valid())
    return;
  select_ids->bind_int64(0, time.ToInternalValue());
  select_ids->bind_string(1, url);
  std::set<int64> rows_to_delete;
  while (select_ids->step() == SQLITE_ROW)
    rows_to_delete.insert(select_ids->column_int64(0));

  // Delete from the pages table.
  SQLITE_UNIQUE_STATEMENT(delete_page, *statement_cache_,
      "DELETE FROM pages WHERE rowid=?");
  if (!delete_page.is_valid())
    return;
  for (std::set<int64>::const_iterator i = rows_to_delete.begin();
       i != rows_to_delete.end(); ++i) {
    delete_page->bind_int64(0, *i);
    delete_page->step();
    delete_page->reset();
  }

  // Delete from the info table.
  SQLITE_UNIQUE_STATEMENT(delete_info, *statement_cache_,
      "DELETE FROM info WHERE rowid=?");
  if (!delete_info.is_valid())
    return;
  for (std::set<int64>::const_iterator i = rows_to_delete.begin();
       i != rows_to_delete.end(); ++i) {
    delete_info->bind_int64(0, *i);
    delete_info->step();
    delete_info->reset();
  }
}

void TextDatabase::Optimize() {
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "SELECT OPTIMIZE(pages) FROM pages LIMIT 1");
  if (!statement.is_valid())
    return;
  statement->step();
}

void TextDatabase::GetTextMatches(const std::string& query,
                                  const QueryOptions& options,
                                  std::vector<Match>* results,
                                  URLSet* found_urls,
                                  Time* first_time_searched) {
  *first_time_searched = options.begin_time;

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
    "SELECT url, title, time, offsets(pages), body "
        "FROM pages LEFT OUTER JOIN info ON pages.rowid = info.rowid "
        "WHERE pages MATCH ? AND time >= ? AND time < ? "
        "ORDER BY time DESC "
        "LIMIT ?");
  if (!statement.is_valid())
    return;

  // When their values indicate "unspecified", saturate the numbers to the max
  // or min to get the correct result.
  int64 effective_begin_time = options.begin_time.is_null() ?
      0 : options.begin_time.ToInternalValue();
  int64 effective_end_time = options.end_time.is_null() ?
      std::numeric_limits<int64>::max() : options.end_time.ToInternalValue();
  int effective_max_count = options.max_count ?
      options.max_count : std::numeric_limits<int>::max();

  statement->bind_string(0, query);
  statement->bind_int64(1, effective_begin_time);
  statement->bind_int64(2, effective_end_time);
  statement->bind_int(3, effective_max_count);

  while (statement->step() == SQLITE_ROW) {
    // TODO(brettw) allow canceling the query in the middle.
    // if (canceled_or_something)
    //   break;

    GURL url(statement->column_string(0));
    if (options.most_recent_visit_only) {
      URLSet::const_iterator found_url = found_urls->find(url);
      if (found_url != found_urls->end())
        continue;  // Don't add this duplicate when unique URLs are requested.
    }

    // Fill the results into the vector (avoid copying the URL with Swap()).
    results->resize(results->size() + 1);
    Match& match = results->at(results->size() - 1);
    match.url.Swap(&url);

    match.title = UTF8ToWide(statement->column_string(1));
    match.time = Time::FromInternalValue(statement->column_int64(2));

    // Extract any matches in the title.
    std::string offsets_str = statement->column_string(3);
    Snippet::ExtractMatchPositions(offsets_str, kTitleColumnIndex,
                                   &match.title_match_positions);
    Snippet::ConvertMatchPositionsToWide(statement->column_string(1),
                                         &match.title_match_positions);

    // Extract the matches in the body.
    Snippet::MatchPositions match_positions;
    Snippet::ExtractMatchPositions(offsets_str, kBodyColumnIndex,
                                   &match_positions);

    // Compute the snippet based on those matches.
    std::string body = statement->column_string(4);
    match.snippet.ComputeSnippet(match_positions, body);
  }

  // When we have returned all the results possible (or determined that there
  // are none), then we have searched all the time requested, so we can
  // set the first_time_searched to that value.
  if (results->size() == 0 ||
      options.max_count == 0 ||  // Special case for wanting all the results.
      static_cast<int>(results->size()) < options.max_count) {
    *first_time_searched = options.begin_time;
  } else {
    // Since we got the results in order, we know the last item is the last
    // time we considered.
    *first_time_searched = results->back().time;
  }

  statement->reset();
}

}  // namespace history
