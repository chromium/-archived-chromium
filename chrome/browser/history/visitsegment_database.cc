// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/visitsegment_database.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"

using base::Time;

// The following tables are used to store url segment information.
//
// segments
//   id                 Primary key
//   name               A unique string to represent that segment. (URL derived)
//   url_id             ID of the url currently used to represent this segment.
//   pres_index         index used to store a fixed presentation position.
//
// segment_usage
//   id                 Primary key
//   segment_id         Corresponding segment id
//   time_slot          time stamp identifying for what day this entry is about
//   visit_count        Number of visit in the segment
//

namespace history {

VisitSegmentDatabase::VisitSegmentDatabase() {
}

VisitSegmentDatabase::~VisitSegmentDatabase() {
}

bool VisitSegmentDatabase::InitSegmentTables() {
  // Segments table.
  if (!DoesSqliteTableExist(GetDB(), "segments")) {
    if (sqlite3_exec(GetDB(), "CREATE TABLE segments ("
                     "id INTEGER PRIMARY KEY,"
                     "name VARCHAR,"
                     "url_id INTEGER NON NULL,"
                     "pres_index INTEGER DEFAULT -1 NOT NULL)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }

    if (sqlite3_exec(GetDB(), "CREATE INDEX segments_name ON segments(name)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }

  // This was added later, so we need to try to create it even if the table
  // already exists.
  sqlite3_exec(GetDB(), "CREATE INDEX segments_url_id ON segments(url_id)",
               NULL, NULL, NULL);

  // Segment usage table.
  if (!DoesSqliteTableExist(GetDB(), "segment_usage")) {
    if (sqlite3_exec(GetDB(), "CREATE TABLE segment_usage ("
                     "id INTEGER PRIMARY KEY,"
                     "segment_id INTEGER NOT NULL,"
                     "time_slot INTEGER NOT NULL,"
                     "visit_count INTEGER DEFAULT 0 NOT NULL)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
    if (sqlite3_exec(GetDB(),
                     "CREATE INDEX segment_usage_time_slot_segment_id ON "
                     "segment_usage(time_slot, segment_id)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }

  // Added in a later version, so we always need to try to creat this index.
  sqlite3_exec(GetDB(), "CREATE INDEX segments_usage_seg_id "
                        "ON segment_usage(segment_id)",
               NULL, NULL, NULL);

  // Presentation index table.
  //
  // Important note:
  // Right now, this table is only used to store the presentation index.
  // If you need to add more columns, keep in mind that rows are currently
  // deleted when the presentation index is changed to -1.
  // See SetPagePresentationIndex() in this file
  if (!DoesSqliteTableExist(GetDB(), "presentation")) {
    if (sqlite3_exec(GetDB(), "CREATE TABLE presentation("
                     "url_id INTEGER PRIMARY KEY,"
                     "pres_index INTEGER NOT NULL)",
                     NULL, NULL, NULL) != SQLITE_OK)
      return false;
  }
  return true;
}

bool VisitSegmentDatabase::DropSegmentTables() {
  // Dropping the tables will implicitly delete the indices.
  return
      sqlite3_exec(GetDB(), "DROP TABLE segments", NULL, NULL, NULL) ==
          SQLITE_OK &&
      sqlite3_exec(GetDB(), "DROP TABLE segment_usage", NULL, NULL, NULL) ==
          SQLITE_OK;
}

// Note: the segment name is derived from the URL but is not a URL. It is
// a string that can be easily recreated from various URLS. Maybe this should
// be an MD5 to limit the length.
//
// static
std::string VisitSegmentDatabase::ComputeSegmentName(const GURL& url) {
  // TODO(brettw) this should probably use the registry controlled
  // domains service.
  GURL::Replacements r;
  const char kWWWDot[] = "www.";
  const int kWWWDotLen = arraysize(kWWWDot) - 1;

  std::string host = url.host();
  const char* host_c = host.c_str();
  // Remove www. to avoid some dups.
  if (static_cast<int>(host.size()) > kWWWDotLen &&
      LowerCaseEqualsASCII(host_c, host_c + kWWWDotLen, kWWWDot)) {
    r.SetHost(host.c_str(),
              url_parse::Component(kWWWDotLen,
                  static_cast<int>(host.size()) - kWWWDotLen));
  }
  // Remove other stuff we don't want.
  r.ClearUsername();
  r.ClearPassword();
  r.ClearQuery();
  r.ClearRef();
  r.ClearPort();

  return url.ReplaceComponents(r).spec();
}

SegmentID VisitSegmentDatabase::GetSegmentNamed(
    const std::string& segment_name) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT id FROM segments WHERE name = ?");
  if (!statement.is_valid())
    return 0;

  statement->bind_string(0, segment_name);
  if (statement->step() == SQLITE_ROW)
    return statement->column_int64(0);
  return 0;
}

bool VisitSegmentDatabase::UpdateSegmentRepresentationURL(SegmentID segment_id,
                                                          URLID url_id) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "UPDATE segments SET url_id = ? WHERE id = ?");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, url_id);
  statement->bind_int64(1, segment_id);
  return statement->step() == SQLITE_DONE;
}

URLID VisitSegmentDatabase::GetSegmentRepresentationURL(SegmentID segment_id) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT url_id FROM segments WHERE id = ?");
  if (!statement.is_valid())
    return 0;

  statement->bind_int64(0, segment_id);
  if (statement->step() == SQLITE_ROW)
    return statement->column_int64(0);
  return 0;
}

SegmentID VisitSegmentDatabase::CreateSegment(URLID url_id,
                                              const std::string& segment_name) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "INSERT INTO segments (name, url_id) VALUES (?,?)");
  if (!statement.is_valid())
    return false;

  statement->bind_string(0, segment_name);
  statement->bind_int64(1, url_id);
  if (statement->step() == SQLITE_DONE)
    return sqlite3_last_insert_rowid(GetDB());
  return false;
}

bool VisitSegmentDatabase::IncreaseSegmentVisitCount(SegmentID segment_id,
                                                     const Time& ts,
                                                     int amount) {
  Time t = ts.LocalMidnight();

  SQLITE_UNIQUE_STATEMENT(select, GetStatementCache(),
      "SELECT id, visit_count FROM segment_usage "
      "WHERE time_slot = ? AND segment_id = ?");
  if (!select.is_valid())
    return false;

  select->bind_int64(0, t.ToInternalValue());
  select->bind_int64(1, segment_id);
  if (select->step() == SQLITE_ROW) {
    SQLITE_UNIQUE_STATEMENT(update, GetStatementCache(),
        "UPDATE segment_usage SET visit_count = ? WHERE id = ?");
    if (!update.is_valid())
      return false;

    update->bind_int64(0, select->column_int64(1) + static_cast<int64>(amount));
    update->bind_int64(1, select->column_int64(0));
    return update->step() == SQLITE_DONE;

  } else {
    SQLITE_UNIQUE_STATEMENT(insert, GetStatementCache(),
        "INSERT INTO segment_usage "
        "(segment_id, time_slot, visit_count) VALUES (?, ?, ?)");
    if (!insert.is_valid())
      return false;

    insert->bind_int64(0, segment_id);
    insert->bind_int64(1, t.ToInternalValue());
    insert->bind_int64(2, static_cast<int64>(amount));
    return insert->step() == SQLITE_DONE;
  }
}

void VisitSegmentDatabase::QuerySegmentUsage(
    const Time& from_time,
    std::vector<PageUsageData*>* results) {
  // This function gathers the highest-ranked segments in two queries.
  // The first gathers scores for all segments.
  // The second gathers segment data (url, title, etc.) for the highest-ranked
  // segments.
  // TODO(evanm): this disregards the "presentation index", which was what was
  // used to lock results into position.  But the rest of our code currently
  // does as well.

  // How many results we return, as promised in the header file.
  const size_t kResultCount = 9;

  // Gather all the segment scores:
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT segment_id, time_slot, visit_count "
      "FROM segment_usage WHERE time_slot >= ? "
      "ORDER BY segment_id");
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  Time ts = from_time.LocalMidnight();
  statement->bind_int64(0, ts.ToInternalValue());

  const Time now = Time::Now();
  SegmentID last_segment_id = 0;
  PageUsageData* pud = NULL;
  float score = 0;
  while (statement->step() == SQLITE_ROW) {
    SegmentID segment_id = statement->column_int64(0);
    if (segment_id != last_segment_id) {
      if (last_segment_id != 0) {
        pud->SetScore(score);
        results->push_back(pud);
      }

      pud = new PageUsageData(segment_id);
      score = 0;
      last_segment_id = segment_id;
    }

    const Time timeslot = Time::FromInternalValue(statement->column_int64(1));
    const int visit_count = statement->column_int(2);
    int days_ago = (now - timeslot).InDays();

    // Score for this day in isolation.
    float day_visits_score = 1.0f + log(static_cast<float>(visit_count));
    // Recent visits count more than historical ones, so we multiply in a boost
    // related to how long ago this day was.
    // This boost is a curve that smoothly goes through these values:
    // Today gets 3x, a week ago 2x, three weeks ago 1.5x, falling off to 1x
    // at the limit of how far we reach into the past.
    float recency_boost = 1.0f + (2.0f * (1.0f / (1.0f + days_ago/7.0f)));
    score += recency_boost * day_visits_score;
  }

  if (last_segment_id != 0) {
    pud->SetScore(score);
    results->push_back(pud);
  }

  // Limit to the top kResultCount results.
  sort(results->begin(), results->end(), PageUsageData::Predicate);
  if (results->size() > kResultCount)
    results->resize(kResultCount);

  // Now fetch the details about the entries we care about.
  SQLITE_UNIQUE_STATEMENT(statement2, GetStatementCache(),
      "SELECT urls.url, urls.title FROM urls "
      "JOIN segments ON segments.url_id = urls.id "
      "WHERE segments.id = ?");
  if (!statement2.is_valid()) {
    NOTREACHED();
    return;
  }
  for (size_t i = 0; i < results->size(); ++i) {
    PageUsageData* pud = (*results)[i];
    statement2->bind_int64(0, pud->GetID());
    if (statement2->step() == SQLITE_ROW) {
      std::string url;
      std::wstring title;
      statement2->column_string(0, &url);
      statement2->column_wstring(1, &title);
      pud->SetURL(GURL(url));
      pud->SetTitle(title);
    }
    statement2->reset();
  }
}

void VisitSegmentDatabase::DeleteSegmentData(const Time& older_than) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "DELETE FROM segment_usage WHERE time_slot < ?");
  if (!statement.is_valid())
    return;

  statement->bind_int64(0, older_than.LocalMidnight().ToInternalValue());
  if (statement->step() != SQLITE_DONE)
    NOTREACHED();
}

void VisitSegmentDatabase::SetSegmentPresentationIndex(SegmentID segment_id,
                                                       int index) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "UPDATE segments SET pres_index = ? WHERE id = ?");
  if (!statement.is_valid())
    return;

  statement->bind_int(0, index);
  statement->bind_int64(1, segment_id);
  if (statement->step() != SQLITE_DONE)
    NOTREACHED();
}

bool VisitSegmentDatabase::DeleteSegmentForURL(URLID url_id) {
  SQLITE_UNIQUE_STATEMENT(select, GetStatementCache(),
      "SELECT id FROM segments WHERE url_id = ?");
  if (!select.is_valid())
    return false;

  SQLITE_UNIQUE_STATEMENT(delete_seg, GetStatementCache(),
      "DELETE FROM segments WHERE id = ?");
  if (!delete_seg.is_valid())
    return false;

  SQLITE_UNIQUE_STATEMENT(delete_usage, GetStatementCache(),
      "DELETE FROM segment_usage WHERE segment_id = ?");
  if (!delete_usage.is_valid())
    return false;

  bool r = true;
  select->bind_int64(0, url_id);
  // In theory there could not be more than one segment using that URL but we
  // loop anyway to cleanup any inconsistency.
  while (select->step() == SQLITE_ROW) {
    SegmentID segment_id = select->column_int64(0);

    delete_usage->bind_int64(0, segment_id);
    if (delete_usage->step() != SQLITE_DONE) {
      NOTREACHED();
      r = false;
    }

    delete_seg->bind_int64(0, segment_id);
    if (delete_seg->step() != SQLITE_DONE) {
      NOTREACHED();
      r = false;
    }
    delete_usage->reset();
    delete_seg->reset();
  }
  return r;
}

}  // namespace history
