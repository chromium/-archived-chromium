// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/url_database.h"

#include <algorithm>
#include <limits>

#include "base/string_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/sqlite_utils.h"
#include "googleurl/src/gurl.h"

using base::Time;

namespace history {

const char URLDatabase::kURLRowFields[] = HISTORY_URL_ROW_FIELDS;
const int URLDatabase::kNumURLRowFields = 9;

bool URLDatabase::URLEnumerator::GetNextURL(URLRow* r) {
  if (statement_.step() == SQLITE_ROW) {
    FillURLRow(statement_, r);
    return true;
  }
  return false;
}

URLDatabase::URLDatabase() : has_keyword_search_terms_(false) {
}

URLDatabase::~URLDatabase() {
}

// static
std::string URLDatabase::GURLToDatabaseURL(const GURL& gurl) {
  // TODO(brettw): do something fancy here with encoding, etc.
  return gurl.spec();
}

// Convenience to fill a history::URLRow. Must be in sync with the fields in
// kURLRowFields.
void URLDatabase::FillURLRow(SQLStatement& s, history::URLRow* i) {
  DCHECK(i);
  i->id_ = s.column_int64(0);
  i->url_ = GURL(s.column_string(1));
  i->title_ = s.column_wstring(2);
  i->visit_count_ = s.column_int(3);
  i->typed_count_ = s.column_int(4);
  i->last_visit_ = Time::FromInternalValue(s.column_int64(5));
  i->hidden_ = s.column_int(6) != 0;
  i->favicon_id_ = s.column_int64(7);
}

bool URLDatabase::GetURLRow(URLID url_id, URLRow* info) {
  // TODO(brettw) We need check for empty URLs to handle the case where
  // there are old URLs in the database that are empty that got in before
  // we added any checks. We should eventually be able to remove it
  // when all inputs are using GURL (which prohibit empty input).
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls WHERE id=?");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, url_id);
  if (statement->step() == SQLITE_ROW) {
    FillURLRow(*statement, info);
    return true;
  }
  return false;
}

URLID URLDatabase::GetRowForURL(const GURL& url, history::URLRow* info) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls WHERE url=?");
  if (!statement.is_valid())
    return 0;

  std::string url_string = GURLToDatabaseURL(url);
  statement->bind_string(0, url_string);
  if (statement->step() != SQLITE_ROW)
    return 0; // no data

  if (info)
    FillURLRow(*statement, info);
  return statement->column_int64(0);
}

bool URLDatabase::UpdateURLRow(URLID url_id,
                               const history::URLRow& info) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "UPDATE urls SET title=?,visit_count=?,typed_count=?,last_visit_time=?,"
        "hidden=?,favicon_id=?"
      "WHERE id=?");
  if (!statement.is_valid())
    return false;

  statement->bind_wstring(0, info.title());
  statement->bind_int(1, info.visit_count());
  statement->bind_int(2, info.typed_count());
  statement->bind_int64(3, info.last_visit().ToInternalValue());
  statement->bind_int(4, info.hidden() ? 1 : 0);
  statement->bind_int64(5, info.favicon_id());
  statement->bind_int64(6, url_id);
  return statement->step() == SQLITE_DONE;
}

URLID URLDatabase::AddURLInternal(const history::URLRow& info,
                                  bool is_temporary) {
  // This function is used to insert into two different tables, so we have to
  // do some shuffling. Unfortinately, we can't use the macro
  // HISTORY_URL_ROW_FIELDS because that specifies the table name which is
  // invalid in the insert syntax.
  #define ADDURL_COMMON_SUFFIX \
      " (url, title, visit_count, typed_count, "\
      "last_visit_time, hidden, favicon_id) "\
      "VALUES (?,?,?,?,?,?,?)"
  const char* statement_name;
  const char* statement_sql;
  if (is_temporary) {
    statement_name = "AddURLTemporary";
    statement_sql = "INSERT INTO temp_urls" ADDURL_COMMON_SUFFIX;
  } else {
    statement_name = "AddURL";
    statement_sql = "INSERT INTO urls" ADDURL_COMMON_SUFFIX;
  }
  #undef ADDURL_COMMON_SUFFIX

  SqliteCompiledStatement statement(statement_name, 0, GetStatementCache(),
                                    statement_sql);
  if (!statement.is_valid())
    return 0;

  statement->bind_string(0, GURLToDatabaseURL(info.url()));
  statement->bind_wstring(1, info.title());
  statement->bind_int(2, info.visit_count());
  statement->bind_int(3, info.typed_count());
  statement->bind_int64(4, info.last_visit().ToInternalValue());
  statement->bind_int(5, info.hidden() ? 1 : 0);
  statement->bind_int64(6, info.favicon_id());

  if (statement->step() != SQLITE_DONE)
    return 0;
  return sqlite3_last_insert_rowid(GetDB());
}

bool URLDatabase::DeleteURLRow(URLID id) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "DELETE FROM urls WHERE id = ?");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, id);
  if (statement->step() != SQLITE_DONE)
    return false;

    // And delete any keyword visits.
  if (!has_keyword_search_terms_)
    return true;

  SQLITE_UNIQUE_STATEMENT(del_keyword_visit, GetStatementCache(),
                          "DELETE FROM keyword_search_terms WHERE url_id=?");
  if (!del_keyword_visit.is_valid())
    return false;
  del_keyword_visit->bind_int64(0, id);
  return (del_keyword_visit->step() == SQLITE_DONE);
}

bool URLDatabase::CreateTemporaryURLTable() {
  return CreateURLTable(true);
}

bool URLDatabase::CommitTemporaryURLTable() {
  // See the comments in the header file as well as
  // HistoryBackend::DeleteAllHistory() for more information on how this works
  // and why it does what it does.
  //
  // Note that the main database overrides this to additionally create the
  // supplimentary indices that the archived database doesn't need.

  // Swap the url table out and replace it with the temporary one.
  if (sqlite3_exec(GetDB(), "DROP TABLE urls",
                   NULL, NULL, NULL) != SQLITE_OK) {
    NOTREACHED();
    return false;
  }
  if (sqlite3_exec(GetDB(), "ALTER TABLE temp_urls RENAME TO urls",
                   NULL, NULL, NULL) != SQLITE_OK) {
    NOTREACHED();
    return false;
  }

  // Create the index over URLs. This is needed for the main, in-memory, and
  // archived databases, so we always do it. The supplimentary indices used by
  // the main database are not created here. When deleting all history, they
  // are created by HistoryDatabase::RecreateAllButStarAndURLTables().
  CreateMainURLIndex();

  return true;
}

bool URLDatabase::InitURLEnumeratorForEverything(URLEnumerator* enumerator) {
  DCHECK(!enumerator->initialized_);
  std::string sql("SELECT ");
  sql.append(kURLRowFields);
  sql.append(" FROM urls");
  if (enumerator->statement_.prepare(GetDB(), sql.c_str()) != SQLITE_OK) {
    NOTREACHED() << "Query statement prep failed";
    return false;
  }
  enumerator->initialized_ = true;
  return true;
}

bool URLDatabase::IsFavIconUsed(FavIconID favicon_id) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT id FROM urls WHERE favicon_id=? LIMIT 1");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, favicon_id);
  return statement->step() == SQLITE_ROW;
}

void URLDatabase::AutocompleteForPrefix(const std::wstring& prefix,
                                        size_t max_results,
                                        std::vector<history::URLRow>* results) {
  // NOTE: this query originally sorted by starred as the second parameter. But
  // as bookmarks is no longer part of the db we no longer include the order
  // by clause.
  results->clear();
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls "
      "WHERE url >= ? AND url < ? AND hidden = 0 "
      "ORDER BY typed_count DESC, visit_count DESC, last_visit_time DESC "
      "LIMIT ?");
  if (!statement.is_valid())
    return;

  // We will find all strings between "prefix" and this string, which is prefix
  // followed by the maximum character size. Use 8-bit strings for everything
  // so we can be sure sqlite is comparing everything in 8-bit mode. Otherwise,
  // it will have to convert strings either to UTF-8 or UTF-16 for comparison.
  std::string prefix_utf8(WideToUTF8(prefix));
  std::string end_query(prefix_utf8);
  end_query.push_back(std::numeric_limits<unsigned char>::max());

  statement->bind_string(0, prefix_utf8);
  statement->bind_string(1, end_query);
  statement->bind_int(2, static_cast<int>(max_results));

  while (statement->step() == SQLITE_ROW) {
    history::URLRow info;
    FillURLRow(*statement, &info);
    if (info.url().is_valid())
      results->push_back(info);
  }
}

bool URLDatabase::FindShortestURLFromBase(const std::string& base,
                                          const std::string& url,
                                          int min_visits,
                                          int min_typed,
                                          bool allow_base,
                                          history::URLRow* info) {
  // Select URLs that start with |base| and are prefixes of |url|.  All parts
  // of this query except the substr() call can be done using the index.  We
  // could do this query with a couple of LIKE or GLOB statements as well, but
  // those wouldn't use the index, and would run into problems with "wildcard"
  // characters that appear in URLs (% for LIKE, or *, ? for GLOB).
  std::string sql("SELECT ");
  sql.append(kURLRowFields);
  sql.append(" FROM urls WHERE url ");
  sql.append(allow_base ? ">=" : ">");
  sql.append(" ? AND url < :end AND url = substr(:end, 1, length(url)) "
             "AND hidden = 0 AND visit_count >= ? AND typed_count >= ? "
             "ORDER BY url LIMIT 1");
  SQLStatement statement;
  if (statement.prepare(GetDB(), sql.c_str()) != SQLITE_OK) {
    NOTREACHED() << "select statement prep failed";
    return false;
  }

  statement.bind_string(0, base);
  statement.bind_string(1, url);   // :end
  statement.bind_int(2, min_visits);
  statement.bind_int(3, min_typed);

  if (statement.step() != SQLITE_ROW)
    return false;

  DCHECK(info);
  FillURLRow(statement, info);
  return true;
}

bool URLDatabase::InitKeywordSearchTermsTable() {
  has_keyword_search_terms_ = true;
  if (!DoesSqliteTableExist(GetDB(), "keyword_search_terms")) {
    if (sqlite3_exec(GetDB(), "CREATE TABLE keyword_search_terms ("
        "keyword_id INTEGER NOT NULL,"      // ID of the TemplateURL.
        "url_id INTEGER NOT NULL,"          // ID of the url.
        "lower_term LONGVARCHAR NOT NULL,"  // The search term, in lower case.
        "term LONGVARCHAR NOT NULL)",       // The actual search term.
        NULL, NULL, NULL) != SQLITE_OK)
      return false;
  }

  // For searching.
  sqlite3_exec(GetDB(), "CREATE INDEX keyword_search_terms_index1 ON "
               "keyword_search_terms (keyword_id, lower_term)",
               NULL, NULL, NULL);

  // For deletion.
  sqlite3_exec(GetDB(), "CREATE INDEX keyword_search_terms_index2 ON "
               "keyword_search_terms (url_id)",
               NULL, NULL, NULL);

  return true;
}

bool URLDatabase::DropKeywordSearchTermsTable() {
  // This will implicitly delete the indices over the table.
  return sqlite3_exec(GetDB(), "DROP TABLE keyword_search_terms",
                      NULL, NULL, NULL) == SQLITE_OK;
}

bool URLDatabase::SetKeywordSearchTermsForURL(URLID url_id,
                                              TemplateURL::IDType keyword_id,
                                              const std::wstring& term) {
  DCHECK(url_id && keyword_id && !term.empty());

  SQLITE_UNIQUE_STATEMENT(exist_statement, GetStatementCache(),
      "SELECT term FROM keyword_search_terms "
      "WHERE keyword_id = ? AND url_id = ?");
  if (!exist_statement.is_valid())
    return false;
  exist_statement->bind_int64(0, keyword_id);
  exist_statement->bind_int64(1, url_id);
  if (exist_statement->step() == SQLITE_ROW)
    return true;  // Term already exists, no need to add it.

  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "INSERT INTO keyword_search_terms (keyword_id, url_id, lower_term, term) "
      "VALUES (?,?,?,?)");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, keyword_id);
  statement->bind_int64(1, url_id);
  statement->bind_wstring(2, l10n_util::ToLower(term));
  statement->bind_wstring(3, term);
  return (statement->step() == SQLITE_DONE);
}

void URLDatabase::DeleteAllSearchTermsForKeyword(
    TemplateURL::IDType keyword_id) {
  DCHECK(keyword_id);
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "DELETE FROM keyword_search_terms WHERE keyword_id=?");
  if (!statement.is_valid())
    return;

  statement->bind_int64(0, keyword_id);
  statement->step();
}

void URLDatabase::GetMostRecentKeywordSearchTerms(
    TemplateURL::IDType keyword_id,
    const std::wstring& prefix,
    int max_count,
    std::vector<KeywordSearchTermVisit>* matches) {
  // NOTE: the keyword_id can be zero if on first run the user does a query
  // before the TemplateURLModel has finished loading. As the chances of this
  // occurring are small, we ignore it.
  if (!keyword_id)
    return;

  DCHECK(!prefix.empty());
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT DISTINCT kv.term, u.last_visit_time "
      "FROM keyword_search_terms kv "
      "JOIN urls u ON kv.url_id = u.id "
      "WHERE kv.keyword_id = ? AND kv.lower_term >= ? AND kv.lower_term < ? "
      "ORDER BY u.last_visit_time DESC LIMIT ?");
  if (!statement.is_valid())
    return;

  // NOTE: Keep this ToLower() call in sync with search_provider.cc.
  const std::wstring lower_prefix = l10n_util::ToLower(prefix);
  // This magic gives us a prefix search.
  std::wstring next_prefix = lower_prefix;
  next_prefix[next_prefix.size() - 1] =
      next_prefix[next_prefix.size() - 1] + 1;
  statement->bind_int64(0, keyword_id);
  statement->bind_wstring(1, lower_prefix);
  statement->bind_wstring(2, next_prefix);
  statement->bind_int(3, max_count);

  KeywordSearchTermVisit visit;
  while (statement->step() == SQLITE_ROW) {
    visit.term = statement->column_wstring(0);
    visit.time = Time::FromInternalValue(statement->column_int64(1));
    matches->push_back(visit);
  }
}

bool URLDatabase::MigrateFromVersion11ToVersion12() {
  URLRow about_row;
  if (GetRowForURL(GURL("about:blank"), &about_row)) {
    about_row.set_favicon_id(0);
    return UpdateURLRow(about_row.id(), about_row);
  }
  return true;
}

bool URLDatabase::DropStarredIDFromURLs() {
  if (!DoesSqliteColumnExist(GetDB(), "urls", "starred_id", NULL))
    return true;  // urls is already updated, no need to continue.

  // Create a temporary table to contain the new URLs table.
  if (!CreateTemporaryURLTable()) {
    NOTREACHED();
    return false;
  }

  // Copy the contents.
  const char* insert_statement =
      "INSERT INTO temp_urls (id, url, title, visit_count, typed_count, "
      "last_visit_time, hidden, favicon_id) "
      "SELECT id, url, title, visit_count, typed_count, last_visit_time, "
      "hidden, favicon_id FROM urls";
  if (sqlite3_exec(GetDB(), insert_statement, NULL, NULL, NULL) != SQLITE_OK) {
    NOTREACHED();
    return false;
  }

  // Rename/commit the tmp table.
  CommitTemporaryURLTable();

  // This isn't created by CommitTemporaryURLTable.
  CreateSupplimentaryURLIndices();

  return true;
}

bool URLDatabase::CreateURLTable(bool is_temporary) {
  const char* name = is_temporary ? "temp_urls" : "urls";
  if (DoesSqliteTableExist(GetDB(), name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append("("
      "id INTEGER PRIMARY KEY,"
      "url LONGVARCHAR,"
      "title LONGVARCHAR,"
      "visit_count INTEGER DEFAULT 0 NOT NULL,"
      "typed_count INTEGER DEFAULT 0 NOT NULL,"
      "last_visit_time INTEGER NOT NULL,"
      "hidden INTEGER DEFAULT 0 NOT NULL,"
      "favicon_id INTEGER DEFAULT 0 NOT NULL)");

  return sqlite3_exec(GetDB(), sql.c_str(), NULL, NULL, NULL) == SQLITE_OK;
}

void URLDatabase::CreateMainURLIndex() {
  // Index over URLs so we can quickly look up based on URL.  Ignore errors as
  // this likely already exists (and the same below).
  sqlite3_exec(GetDB(), "CREATE INDEX urls_url_index ON urls (url)", NULL, NULL,
               NULL);
}

void URLDatabase::CreateSupplimentaryURLIndices() {
  // Add a favicon index.  This is useful when we delete urls.
  sqlite3_exec(GetDB(),
               "CREATE INDEX urls_favicon_id_INDEX ON urls (favicon_id)",
               NULL, NULL, NULL);
}

}  // namespace history

