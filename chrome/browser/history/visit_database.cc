// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>
#include <map>
#include <set>

#include "chrome/browser/history/visit_database.h"

#include "chrome/browser/history/url_database.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/url_constants.h"

using base::Time;

// Rows, in order, of the visit table.
#define HISTORY_VISIT_ROW_FIELDS \
  " id,url,visit_time,from_visit,transition,segment_id,is_indexed "

namespace history {

VisitDatabase::VisitDatabase() {
}

VisitDatabase::~VisitDatabase() {
}

bool VisitDatabase::InitVisitTable() {
  if (!DoesSqliteTableExist(GetDB(), "visits")) {
    if (sqlite3_exec(GetDB(), "CREATE TABLE visits("
        "id INTEGER PRIMARY KEY,"
        "url INTEGER NOT NULL," // key of the URL this corresponds to
        "visit_time INTEGER NOT NULL,"
        "from_visit INTEGER,"
        "transition INTEGER DEFAULT 0 NOT NULL,"
        "segment_id INTEGER,"
        // True when we have indexed data for this visit.
        "is_indexed BOOLEAN)",
        NULL, NULL, NULL) != SQLITE_OK)
      return false;
  } else if (!DoesSqliteColumnExist(GetDB(), "visits",
                                    "is_indexed", "BOOLEAN")) {
    // Old versions don't have the is_indexed column, we can just add that and
    // not worry about different database revisions, since old ones will
    // continue to work.
    //
    // TODO(brettw) this should be removed once we think everybody has been
    // updated (added early Mar 2008).
    if (sqlite3_exec(GetDB(),
                     "ALTER TABLE visits ADD COLUMN is_indexed BOOLEAN",
                     NULL, NULL, NULL) != SQLITE_OK)
      return false;
  }

  // Index over url so we can quickly find visits for a page. This will just
  // fail if it already exists and we'll ignore it.
  sqlite3_exec(GetDB(), "CREATE INDEX visits_url_index ON visits (url)",
               NULL, NULL, NULL);

  // Create an index over from visits so that we can efficiently find
  // referrers and redirects. Ignore failures because it likely already exists.
  sqlite3_exec(GetDB(), "CREATE INDEX visits_from_index ON visits (from_visit)",
               NULL, NULL, NULL);

  // Create an index over time so that we can efficiently find the visits in a
  // given time range (most history views are time-based). Ignore failures
  // because it likely already exists.
  sqlite3_exec(GetDB(), "CREATE INDEX visits_time_index ON visits (visit_time)",
               NULL, NULL, NULL);

  return true;
}

bool VisitDatabase::DropVisitTable() {
  // This will also drop the indices over the table.
  return sqlite3_exec(GetDB(), "DROP TABLE visits", NULL, NULL, NULL) ==
      SQLITE_OK;
}

// Must be in sync with HISTORY_VISIT_ROW_FIELDS.
// static
void VisitDatabase::FillVisitRow(SQLStatement& statement, VisitRow* visit) {
  visit->visit_id = statement.column_int64(0);
  visit->url_id = statement.column_int64(1);
  visit->visit_time = Time::FromInternalValue(statement.column_int64(2));
  visit->referring_visit = statement.column_int64(3);
  visit->transition = PageTransition::FromInt(statement.column_int(4));
  visit->segment_id = statement.column_int64(5);
  visit->is_indexed = !!statement.column_int(6);
}

// static
void VisitDatabase::FillVisitVector(SQLStatement& statement,
                                    VisitVector* visits) {
  while (statement.step() == SQLITE_ROW) {
    history::VisitRow visit;
    FillVisitRow(statement, &visit);
    visits->push_back(visit);
  }
}

VisitID VisitDatabase::AddVisit(VisitRow* visit) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "INSERT INTO visits "
      "(url, visit_time, from_visit, transition, segment_id, is_indexed) "
      "VALUES (?,?,?,?,?,?)");
  if (!statement.is_valid())
    return 0;

  statement->bind_int64(0, visit->url_id);
  statement->bind_int64(1, visit->visit_time.ToInternalValue());
  statement->bind_int64(2, visit->referring_visit);
  statement->bind_int64(3, visit->transition);
  statement->bind_int64(4, visit->segment_id);
  statement->bind_int64(5, visit->is_indexed);
  if (statement->step() != SQLITE_DONE)
    return 0;

  visit->visit_id = sqlite3_last_insert_rowid(GetDB());
  return visit->visit_id;
}

void VisitDatabase::DeleteVisit(const VisitRow& visit) {
  // Patch around this visit. Any visits that this went to will now have their
  // "source" be the deleted visit's source.
  SQLITE_UNIQUE_STATEMENT(update_chain, GetStatementCache(),
                          "UPDATE visits SET from_visit=? "
                          "WHERE from_visit=?");
  if (!update_chain.is_valid())
    return;
  update_chain->bind_int64(0, visit.referring_visit);
  update_chain->bind_int64(1, visit.visit_id);
  update_chain->step();

  // Now delete the actual visit.
  SQLITE_UNIQUE_STATEMENT(del, GetStatementCache(),
                          "DELETE FROM visits WHERE id=?");
  if (!del.is_valid())
    return;
  del->bind_int64(0, visit.visit_id);
  del->step();
}

bool VisitDatabase::GetRowForVisit(VisitID visit_id, VisitRow* out_visit) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits WHERE id=?");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, visit_id);
  if (statement->step() != SQLITE_ROW)
    return false;

  FillVisitRow(*statement, out_visit);
  return true;
}

bool VisitDatabase::UpdateVisitRow(const VisitRow& visit) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "UPDATE visits SET "
      "url=?,visit_time=?,from_visit=?,transition=?,segment_id=?,is_indexed=? "
      "WHERE id=?");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, visit.url_id);
  statement->bind_int64(1, visit.visit_time.ToInternalValue());
  statement->bind_int64(2, visit.referring_visit);
  statement->bind_int64(3, visit.transition);
  statement->bind_int64(4, visit.segment_id);
  statement->bind_int64(5, visit.is_indexed);
  statement->bind_int64(6, visit.visit_id);
  return statement->step() == SQLITE_DONE;
}

bool VisitDatabase::GetVisitsForURL(URLID url_id, VisitVector* visits) {
  visits->clear();

  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT" HISTORY_VISIT_ROW_FIELDS
      "FROM visits "
      "WHERE url=? "
      "ORDER BY visit_time ASC");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, url_id);
  FillVisitVector(*statement, visits);
  return true;
}

void VisitDatabase::GetAllVisitsInRange(Time begin_time, Time end_time,
                                        int max_results,
                                        VisitVector* visits) {
  visits->clear();

  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
      "WHERE visit_time >= ? AND visit_time < ?"
      "ORDER BY visit_time LIMIT ?");
  if (!statement.is_valid())
    return;

  // See GetVisibleVisitsInRange for more info on how these times are bound.
  int64 end = end_time.ToInternalValue();
  statement->bind_int64(0, begin_time.ToInternalValue());
  statement->bind_int64(1, end ? end : std::numeric_limits<int64>::max());
  statement->bind_int64(2,
      max_results ? max_results : std::numeric_limits<int64>::max());

  FillVisitVector(*statement, visits);
}

void VisitDatabase::GetVisitsInRangeForTransition(
    Time begin_time,
    Time end_time,
    int max_results,
    PageTransition::Type transition,
    VisitVector* visits) {
  DCHECK(visits);
  visits->clear();

  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
      "WHERE visit_time >= ? AND visit_time < ? "
      "AND (transition & ?) == ?"
      "ORDER BY visit_time LIMIT ?");
  if (!statement.is_valid())
    return;

  // See GetVisibleVisitsInRange for more info on how these times are bound.
  int64 end = end_time.ToInternalValue();
  statement->bind_int64(0, begin_time.ToInternalValue());
  statement->bind_int64(1, end ? end : std::numeric_limits<int64>::max());
  statement->bind_int(2, PageTransition::CORE_MASK);
  statement->bind_int(3, transition);
  statement->bind_int64(4,
      max_results ? max_results : std::numeric_limits<int64>::max());

  FillVisitVector(*statement, visits);
}

void VisitDatabase::GetVisibleVisitsInRange(Time begin_time, Time end_time,
                                            bool most_recent_visit_only,
                                            int max_count,
                                            VisitVector* visits) {
  visits->clear();
  // The visit_time values can be duplicated in a redirect chain, so we sort
  // by id too, to ensure a consistent ordering just in case.
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
      "WHERE visit_time >= ? AND visit_time < ? "
      "AND (transition & ?) != 0 "  // CHAIN_END
      "AND (transition & ?) NOT IN (?, ?, ?) "  // NO SUBFRAME or
                                                // KEYWORD_GENERATED
      "ORDER BY visit_time DESC, id DESC");
  if (!statement.is_valid())
    return;

  // Note that we use min/max values for querying unlimited ranges of time using
  // the same statement. Since the time has an index, this will be about the
  // same amount of work as just doing a query for everything with no qualifier.
  int64 end = end_time.ToInternalValue();
  statement->bind_int64(0, begin_time.ToInternalValue());
  statement->bind_int64(1, end ? end : std::numeric_limits<int64>::max());
  statement->bind_int(2, PageTransition::CHAIN_END);
  statement->bind_int(3, PageTransition::CORE_MASK);
  statement->bind_int(4, PageTransition::AUTO_SUBFRAME);
  statement->bind_int(5, PageTransition::MANUAL_SUBFRAME);
  statement->bind_int(6, PageTransition::KEYWORD_GENERATED);

  std::set<URLID> found_urls;
  while (statement->step() == SQLITE_ROW) {
    VisitRow visit;
    FillVisitRow(*statement, &visit);
    if (most_recent_visit_only) {
      // Make sure the URL this visit corresponds to is unique if required.
      if (found_urls.find(visit.url_id) != found_urls.end())
        continue;
      found_urls.insert(visit.url_id);
    }
    visits->push_back(visit);

    if (max_count > 0 && static_cast<int>(visits->size()) >= max_count)
      break;
  }
}

VisitID VisitDatabase::GetMostRecentVisitForURL(URLID url_id,
                                                VisitRow* visit_row) {
  // The visit_time values can be duplicated in a redirect chain, so we sort
  // by id too, to ensure a consistent ordering just in case.
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
      "WHERE url=? "
      "ORDER BY visit_time DESC, id DESC "
      "LIMIT 1");
  if (!statement.is_valid())
    return 0;

  statement->bind_int64(0, url_id);
  if (statement->step() != SQLITE_ROW)
    return 0;  // No visits for this URL.

  if (visit_row) {
    FillVisitRow(*statement, visit_row);
    return visit_row->visit_id;
  }
  return statement->column_int64(0);
}

bool VisitDatabase::GetMostRecentVisitsForURL(URLID url_id,
                                              int max_results,
                                              VisitVector* visits) {
  visits->clear();

  // The visit_time values can be duplicated in a redirect chain, so we sort
  // by id too, to ensure a consistent ordering just in case.
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT" HISTORY_VISIT_ROW_FIELDS
      "FROM visits "
      "WHERE url=? "
      "ORDER BY visit_time DESC, id DESC "
      "LIMIT ?");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, url_id);
  statement->bind_int(1, max_results);
  FillVisitVector(*statement, visits);
  return true;
}

bool VisitDatabase::GetRedirectFromVisit(VisitID from_visit,
                                         VisitID* to_visit,
                                         GURL* to_url) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT v.id,u.url "
      "FROM visits v JOIN urls u ON v.url = u.id "
      "WHERE v.from_visit = ? "
      "AND (v.transition & ?) != 0");  // IS_REDIRECT_MASK
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, from_visit);
  statement->bind_int(1, PageTransition::IS_REDIRECT_MASK);

  if (statement->step() != SQLITE_ROW)
    return false;  // No redirect from this visit.
  if (to_visit)
    *to_visit = statement->column_int64(0);
  if (to_url)
    *to_url = GURL(statement->column_string(1));
  return true;
}

bool VisitDatabase::GetRedirectToVisit(VisitID to_visit,
                                       VisitID* from_visit,
                                       GURL* from_url) {
  VisitRow row;
  if (!GetRowForVisit(to_visit, &row))
    return false;

  if (from_visit)
    *from_visit = row.referring_visit;

  if (from_url) {
    SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
        "SELECT u.url "
        "FROM visits v JOIN urls u ON v.url = u.id "
        "WHERE v.id = ?");
    statement->bind_int64(0, row.referring_visit);

    if (statement->step() != SQLITE_ROW)
      return false;

    *from_url = GURL(statement->column_string(0));
  }
  return true;
}

bool VisitDatabase::GetVisitCountToHost(const GURL& url,
                                        int* count,
                                        Time* first_visit) {
  if (!url.SchemeIs(chrome::kHttpScheme) && !url.SchemeIs(chrome::kHttpsScheme))
    return false;

  // We need to search for URLs with a matching host/port. One way to query for
  // this is to use the LIKE operator, eg 'url LIKE http://google.com/%'. This
  // is inefficient though in that it doesn't use the index and each entry must
  // be visited. The same query can be executed by using >= and < operator.
  // The query becomes:
  // 'url >= http://google.com/' and url < http://google.com0'.
  // 0 is used as it is one character greater than '/'.
  GURL search_url(url);
  const std::string host_query_min = search_url.GetOrigin().spec();

  if (host_query_min.empty())
    return false;

  std::string host_query_max = host_query_min;
  host_query_max[host_query_max.size() - 1] = '0';

  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT MIN(v.visit_time), COUNT(*) "
      "FROM visits v INNER JOIN urls u ON v.url = u.id "
      "WHERE (u.url >= ? AND u.url < ?)");
  if (!statement.is_valid())
    return false;

  statement->bind_string(0, host_query_min);
  statement->bind_string(1, host_query_max);

  if (statement->step() != SQLITE_ROW) {
    // We've never been to this page before.
    *count = 0;
    return true;
  }

  *first_visit = Time::FromInternalValue(statement->column_int64(0));
  *count = statement->column_int(1);
  return true;
}

bool VisitDatabase::GetStartDate(Time* first_visit) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT MIN(visit_time) FROM visits WHERE visit_time != 0");
  if (!statement.is_valid() || statement->step() != SQLITE_ROW ||
      statement->column_int64(0) == 0) {
    *first_visit = Time::Now();
    return false;
  }
  *first_visit = Time::FromInternalValue(statement->column_int64(0));
  return true;
}

}  // namespace history
