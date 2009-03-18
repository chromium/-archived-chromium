// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/text_database_manager.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/history/history_publisher.h"
#include "chrome/common/mru_cache.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace history {

namespace {

// The number of database files we will be attached to at once.
const int kCacheDBSize = 5;

std::string ConvertStringForIndexer(
    const std::wstring& input) {
  // TODO(evanm): other transformations here?
  return WideToUTF8(CollapseWhitespace(input, false));
}

// Data older than this will be committed to the full text index even if we
// haven't gotten a title and/or body.
const int kExpirationSec = 20;

}  // namespace

// TextDatabaseManager::PageInfo -----------------------------------------------

TextDatabaseManager::PageInfo::PageInfo(URLID url_id,
                                        VisitID visit_id,
                                        Time visit_time)
    : url_id_(url_id),
      visit_id_(visit_id),
      visit_time_(visit_time) {
  added_time_ = TimeTicks::Now();
}

void TextDatabaseManager::PageInfo::set_title(const std::wstring& ttl) {
  if (ttl.empty())  // Make the title nonempty when we set it for EverybodySet.
    title_ = L" ";
  else
    title_ = ttl;
}

void TextDatabaseManager::PageInfo::set_body(const std::wstring& bdy) {
  if (bdy.empty())  // Make the body nonempty when we set it for EverybodySet.
    body_ = L" ";
  else
    body_ = bdy;
}

bool TextDatabaseManager::PageInfo::Expired(TimeTicks now) const {
  return now - added_time_ > TimeDelta::FromSeconds(kExpirationSec);
}

// TextDatabaseManager ---------------------------------------------------------

TextDatabaseManager::TextDatabaseManager(const FilePath& dir,
                                         URLDatabase* url_database,
                                         VisitDatabase* visit_database)
    : dir_(dir),
      db_(NULL),
      url_database_(url_database),
      visit_database_(visit_database),
      recent_changes_(RecentChangeList::NO_AUTO_EVICT),
      transaction_nesting_(0),
      db_cache_(DBCache::NO_AUTO_EVICT),
      present_databases_loaded_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)),
      history_publisher_(NULL) {
}

TextDatabaseManager::~TextDatabaseManager() {
  if (transaction_nesting_)
    CommitTransaction();
}

// static
TextDatabase::DBIdent TextDatabaseManager::TimeToID(Time time) {
  Time::Exploded exploded;
  time.UTCExplode(&exploded);

  // We combine the month and year into a 6-digit number (200801 for
  // January, 2008). The month is 1-based.
  return exploded.year * 100 + exploded.month;
}

// static
Time TextDatabaseManager::IDToTime(TextDatabase::DBIdent id) {
  Time::Exploded exploded;
  memset(&exploded, 0, sizeof(Time::Exploded));
  exploded.year = id / 100;
  exploded.month = id % 100;
  return Time::FromUTCExploded(exploded);
}

bool TextDatabaseManager::Init(const HistoryPublisher* history_publisher) {
  history_publisher_ = history_publisher;

  // Start checking recent changes and committing them.
  ScheduleFlushOldChanges();
  return true;
}

void TextDatabaseManager::BeginTransaction() {
  transaction_nesting_++;
}

void TextDatabaseManager::CommitTransaction() {
  DCHECK(transaction_nesting_);
  transaction_nesting_--;
  if (transaction_nesting_)
    return;  // Still more nesting of transactions before committing.

  // Commit all databases with open transactions on them.
  for (DBIdentSet::const_iterator i = open_transactions_.begin();
       i != open_transactions_.end(); ++i) {
    DBCache::iterator iter = db_cache_.Get(*i);
    if (iter == db_cache_.end()) {
      NOTREACHED() << "All open transactions should be cached.";
      continue;
    }
    iter->second->CommitTransaction();
  }
  open_transactions_.clear();

  // Now that the transaction is over, we can expire old connections.
  db_cache_.ShrinkToSize(kCacheDBSize);
}

void TextDatabaseManager::InitDBList() {
  if (present_databases_loaded_)
    return;

  present_databases_loaded_ = true;

  // Find files on disk matching our pattern so we can quickly test for them.
  FilePath::StringType filepattern(TextDatabase::file_base());
  filepattern.append(FILE_PATH_LITERAL("*"));
  file_util::FileEnumerator enumerator(
      dir_, false, file_util::FileEnumerator::FILES, filepattern);
  FilePath cur_file;
  while (!(cur_file = enumerator.Next()).empty()) {
    // Convert to the number representing this file.
    TextDatabase::DBIdent id = TextDatabase::FileNameToID(cur_file);
    if (id)  // Will be 0 on error.
      present_databases_.insert(id);
  }
}

void TextDatabaseManager::AddPageURL(const GURL& url,
                                     URLID url_id,
                                     VisitID visit_id,
                                     Time time) {
  // Delete any existing page info.
  RecentChangeList::iterator found = recent_changes_.Peek(url);
  if (found != recent_changes_.end())
    recent_changes_.Erase(found);

  // Just save this info for later. We will save it when it expires or when all
  // the data is complete.
  recent_changes_.Put(url, PageInfo(url_id, visit_id, time));
}

void TextDatabaseManager::AddPageTitle(const GURL& url,
                                       const std::wstring& title) {
  RecentChangeList::iterator found = recent_changes_.Peek(url);
  if (found == recent_changes_.end()) {
    // This page is not in our cache of recent pages. This is very much an edge
    // case as normally a title will come in <20 seconds after the page commits,
    // and WebContents will avoid spamming us with >1 title per page. However,
    // it could come up if your connection is unhappy, and we don't want to
    // miss anything.
    //
    // To solve this problem, we'll just associate the most recent visit with
    // the new title and index that using the regular code path.
    URLRow url_row;
    if (!url_database_->GetRowForURL(url, &url_row))
      return;  // URL is unknown, give up.
    VisitRow visit;
    if (!visit_database_->GetMostRecentVisitForURL(url_row.id(), &visit))
      return;  // No recent visit, give up.

    if (visit.is_indexed) {
      // If this page was already indexed, we could have a body that came in
      // first and we don't want to overwrite it. We could go query for the
      // current body, or have a special setter for only the title, but this is
      // not worth it for this edge case.
      //
      // It will be almost impossible for the title to take longer than
      // kExpirationSec yet we got a body in less than that time, since the
      // title should always come in first.
      return;
    }

    AddPageData(url, url_row.id(), visit.visit_id, visit.visit_time,
                title, std::wstring());
    return;  // We don't know about this page, give up.
  }

  PageInfo& info = found->second;
  if (info.has_body()) {
    // This info is complete, write to the database.
    AddPageData(url, info.url_id(), info.visit_id(), info.visit_time(),
                title, info.body());
    recent_changes_.Erase(found);
    return;
  }

  info.set_title(title);
}

void TextDatabaseManager::AddPageContents(const GURL& url,
                                          const std::wstring& body) {
  RecentChangeList::iterator found = recent_changes_.Peek(url);
  if (found == recent_changes_.end()) {
    // This page is not in our cache of recent pages. This means that the page
    // took more than kExpirationSec to load. Often, this will be the result of
    // a very slow iframe or other resource on the page that makes us think its
    // still loading.
    //
    // As a fallback, set the most recent visit's contents using the input, and
    // use the last set title in the URL table as the title to index.
    URLRow url_row;
    if (!url_database_->GetRowForURL(url, &url_row))
      return;  // URL is unknown, give up.
    VisitRow visit;
    if (!visit_database_->GetMostRecentVisitForURL(url_row.id(), &visit))
      return;  // No recent visit, give up.

    // Use the title from the URL row as the title for the indexing.
    AddPageData(url, url_row.id(), visit.visit_id, visit.visit_time,
                url_row.title(), body);
    return;
  }

  PageInfo& info = found->second;
  if (info.has_title()) {
    // This info is complete, write to the database.
    AddPageData(url, info.url_id(), info.visit_id(), info.visit_time(),
                info.title(), body);
    recent_changes_.Erase(found);
    return;
  }

  info.set_body(body);
}

bool TextDatabaseManager::AddPageData(const GURL& url,
                                      URLID url_id,
                                      VisitID visit_id,
                                      Time visit_time,
                                      const std::wstring& title,
                                      const std::wstring& body) {
  TextDatabase* db = GetDBForTime(visit_time, true);
  if (!db)
    return false;

  TimeTicks beginning_time = TimeTicks::Now();

  // First delete any recently-indexed data for this page. This will delete
  // anything in the main database, but we don't bother looking through the
  // archived database.
  VisitVector visits;
  visit_database_->GetVisitsForURL(url_id, &visits);
  size_t our_visit_row_index = visits.size();
  for (size_t i = 0; i < visits.size(); i++) {
    // While we're going trough all the visits, also find our row so we can
    // avoid another DB query.
    if (visits[i].visit_id == visit_id) {
      our_visit_row_index = i;
    } else if (visits[i].is_indexed) {
      visits[i].is_indexed = false;
      visit_database_->UpdateVisitRow(visits[i]);
      DeletePageData(visits[i].visit_time, url, NULL);
    }
  }

  if (visit_id) {
    // We're supposed to update the visit database.
    if (our_visit_row_index >= visits.size()) {
      NOTREACHED() << "We should always have found a visit when given an ID.";
      return false;
    }

    DCHECK(visit_time == visits[our_visit_row_index].visit_time);

    // Update the visit database to reference our addition.
    visits[our_visit_row_index].is_indexed = true;
    if (!visit_database_->UpdateVisitRow(visits[our_visit_row_index]))
      return false;
  }

  // Now index the data.
  std::string url_str = URLDatabase::GURLToDatabaseURL(url);
  bool success = db->AddPageData(visit_time, url_str,
                                 ConvertStringForIndexer(title),
                                 ConvertStringForIndexer(body));

  HISTOGRAM_TIMES("History.AddFTSData",
                  TimeTicks::Now() - beginning_time);

  if (history_publisher_)
    history_publisher_->PublishPageContent(visit_time, url, title, body);

  return success;
}

void TextDatabaseManager::DeletePageData(Time time, const GURL& url,
                                         ChangeSet* change_set) {
  TextDatabase::DBIdent db_ident = TimeToID(time);

  // We want to open the database for writing, but only if it exists. To
  // achieve this, we check whether it exists by saying we're not going to
  // write to it (avoiding the autocreation code normally called when writing)
  // and then access it for writing only if it succeeds.
  TextDatabase* db = GetDB(db_ident, false);
  if (!db)
    return;
  db = GetDB(db_ident, true);

  if (change_set)
    change_set->Add(db_ident);

  db->DeletePageData(time, URLDatabase::GURLToDatabaseURL(url));
}

void TextDatabaseManager::DeleteFromUncommitted(Time begin, Time end) {
  // First find the beginning of the range to delete. Recall that the list
  // has the most recent item at the beginning. There won't normally be very
  // many items, so a brute-force search is fine.
  RecentChangeList::iterator cur = recent_changes_.begin();
  if (!end.is_null()) {
    // Walk from the beginning of the list backwards in time to find the newest
    // entry that should be deleted.
    while (cur != recent_changes_.end() && cur->second.visit_time() >= end)
      ++cur;
  }

  // Now delete all visits up to the oldest one we were supposed to delete.
  // Note that if begin is_null, it will be less than or equal to any other
  // time.
  while (cur != recent_changes_.end() && cur->second.visit_time() >= begin)
    cur = recent_changes_.Erase(cur);
}

void TextDatabaseManager::DeleteURLFromUncommitted(const GURL& url) {
  RecentChangeList::iterator found = recent_changes_.Peek(url);
  if (found == recent_changes_.end())
    return;  // We don't know about this page, give up.
  recent_changes_.Erase(found);
}

void TextDatabaseManager::DeleteAll() {
  DCHECK(transaction_nesting_ == 0) << "Calling deleteAll in a transaction.";

  InitDBList();

  // Close all open databases.
  db_cache_.ShrinkToSize(0);

  // Now go through and delete all the files.
  for (DBIdentSet::iterator i = present_databases_.begin();
       i != present_databases_.end(); ++i) {
    FilePath file_name = dir_.Append(TextDatabase::IDToFileName(*i));
    file_util::Delete(file_name, false);
  }
}

void TextDatabaseManager::OptimizeChangedDatabases(
    const ChangeSet& change_set) {
  for (ChangeSet::DBSet::const_iterator i =
           change_set.changed_databases_.begin();
       i != change_set.changed_databases_.end(); ++i) {
    // We want to open the database for writing, but only if it exists. To
    // achieve this, we check whether it exists by saying we're not going to
    // write to it (avoiding the autocreation code normally called when writing)
    // and then access it for writing only if it succeeds.
    TextDatabase* db = GetDB(*i, false);
    if (!db)
      continue;
    db = GetDB(*i, true);
    if (!db)
      continue;  // The file may have changed or something.
    db->Optimize();
  }
}

void TextDatabaseManager::GetTextMatches(
    const std::wstring& query,
    const QueryOptions& options,
    std::vector<TextDatabase::Match>* results,
    Time* first_time_searched) {
  results->clear();

  InitDBList();
  if (present_databases_.empty()) {
    // Nothing to search.
    *first_time_searched = options.begin_time;
    return;
  }

  // Get the query into the proper format for the individual DBs.
  std::wstring fts_query_wide;
  query_parser_.ParseQuery(query, &fts_query_wide);
  std::string fts_query = WideToUTF8(fts_query_wide);

  // Need a copy of the options so we can modify the max count for each call
  // to the individual databases.
  QueryOptions cur_options(options);

  // Compute the minimum and maximum values for the identifiers that could
  // encompass the input time range.
  TextDatabase::DBIdent min_ident = options.begin_time.is_null() ?
      *present_databases_.begin() :
      TimeToID(options.begin_time);
  TextDatabase::DBIdent max_ident = options.end_time.is_null() ?
      *present_databases_.rbegin() :
      TimeToID(options.end_time);

  // Iterate over the databases from the most recent backwards.
  bool checked_one = false;
  TextDatabase::URLSet found_urls;
  for (DBIdentSet::reverse_iterator i = present_databases_.rbegin();
       i != present_databases_.rend();
       ++i) {
    // TODO(brettw) allow canceling the query in the middle.
    // if (canceled_or_something)
    //   break;

    // This code is stupid, we just loop until we find the correct starting
    // time range rather than search in an intelligent way. Users will have a
    // few dozen files at most, so this should not be an issue.
    if (*i > max_ident)
      continue;  // Haven't gotten to the time range yet.
    if (*i < min_ident)
      break;  // Covered all the time range.

    TextDatabase* cur_db = GetDB(*i, false);
    if (!cur_db)
      continue;

    // Adjust the max count according to how many results we've already got.
    if (options.max_count) {
      cur_options.max_count = options.max_count -
          static_cast<int>(results->size());
    }

    // Since we are going backwards in time, it is always OK to pass the
    // current first_time_searched, since it will always be smaller than
    // any previous set.
    cur_db->GetTextMatches(fts_query, cur_options,
                           results, &found_urls, first_time_searched);
    checked_one = true;

    DCHECK(options.max_count == 0 ||
           static_cast<int>(results->size()) <= options.max_count);
    if (options.max_count &&
        static_cast<int>(results->size()) >= options.max_count)
      break;  // Got the max number of results.
  }

  // When there were no databases in the range, we need to fix up the min time.
  if (!checked_one)
    *first_time_searched = options.begin_time;
}

TextDatabase* TextDatabaseManager::GetDB(TextDatabase::DBIdent id,
                                         bool for_writing) {
  DBCache::iterator found_db = db_cache_.Get(id);
  if (found_db != db_cache_.end()) {
    if (transaction_nesting_ && for_writing &&
        open_transactions_.find(id) == open_transactions_.end()) {
      // If we currently have an open transaction, that database is not yet
      // part of the transaction, and the database will be written to, it needs
      // to be part of our transaction.
      found_db->second->BeginTransaction();
      open_transactions_.insert(id);
    }
    return found_db->second;
  }

  // Need to make the database.
  TextDatabase* new_db = new TextDatabase(dir_, id, for_writing);
  if (!new_db->Init()) {
    delete new_db;
    return NULL;
  }
  db_cache_.Put(id, new_db);
  present_databases_.insert(id);

  if (transaction_nesting_ && for_writing) {
    // If we currently have an open transaction and the new database will be
    // written to, it needs to be part of our transaction.
    new_db->BeginTransaction();
    open_transactions_.insert(id);
  }

  // When no transaction is open, allow this new one to kick out an old one.
  if (!transaction_nesting_)
    db_cache_.ShrinkToSize(kCacheDBSize);

  return new_db;
}

TextDatabase* TextDatabaseManager::GetDBForTime(Time time,
                                                bool create_if_necessary) {
  return GetDB(TimeToID(time), create_if_necessary);
}

void TextDatabaseManager::ScheduleFlushOldChanges() {
  factory_.RevokeAll();
  MessageLoop::current()->PostDelayedTask(FROM_HERE, factory_.NewRunnableMethod(
          &TextDatabaseManager::FlushOldChanges),
      kExpirationSec * Time::kMillisecondsPerSecond);
}

void TextDatabaseManager::FlushOldChanges() {
  FlushOldChangesForTime(TimeTicks::Now());
}

void TextDatabaseManager::FlushOldChangesForTime(TimeTicks now) {
  // The end of the list is the oldest, so we just start from there committing
  // things until we get something too new.
  RecentChangeList::reverse_iterator i = recent_changes_.rbegin();
  while (i != recent_changes_.rend() && i->second.Expired(now)) {
    AddPageData(i->first, i->second.url_id(), i->second.visit_id(),
                i->second.visit_time(), i->second.title(), i->second.body());
    i = recent_changes_.Erase(i);
  }

  ScheduleFlushOldChanges();
}

}  // namespace history
