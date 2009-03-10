// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/expire_history_backend.h"

#include <algorithm>
#include <limits>

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "chrome/browser/bookmarks/bookmark_service.h"
#include "chrome/browser/history/archived_database.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/text_database_manager.h"
#include "chrome/browser/history/thumbnail_database.h"
#include "chrome/common/notification_type.h"

using base::Time;
using base::TimeDelta;

namespace history {

namespace {

// Returns true if this visit is worth archiving. Otherwise, this visit is not
// worth saving (for example, subframe navigations and redirects) and we can
// just delete it when it gets old.
bool ShouldArchiveVisit(const VisitRow& visit) {
  int no_qualifier = PageTransition::StripQualifier(visit.transition);

  // These types of transitions are always "important" and the user will want
  // to see them.
  if (no_qualifier == PageTransition::TYPED ||
      no_qualifier == PageTransition::AUTO_BOOKMARK ||
      no_qualifier == PageTransition::START_PAGE)
    return true;

  // Only archive these "less important" transitions when they were the final
  // navigation and not part of a redirect chain.
  if ((no_qualifier == PageTransition::LINK ||
       no_qualifier == PageTransition::FORM_SUBMIT ||
       no_qualifier == PageTransition::GENERATED) &&
      visit.transition & PageTransition::CHAIN_END)
    return true;

  // The transition types we ignore are AUTO_SUBFRAME and MANUAL_SUBFRAME.
  return false;
}

// The number of visits we will expire very time we check for old items. This
// Prevents us from doing too much work any given time.
const int kNumExpirePerIteration = 10;

// The number of seconds between checking for items that should be expired when
// we think there might be more items to expire. This timeout is used when the
// last expiration found at least kNumExpirePerIteration and we want to check
// again "soon."
const int kExpirationDelaySec = 60;

// The number of minutes between checking, as with kExpirationDelaySec, but
// when we didn't find enough things to expire last time. If there was no
// history to expire last iteration, it's likely there is nothing next
// iteration, so we want to wait longer before checking to avoid wasting CPU.
const int kExpirationEmptyDelayMin = 5;

}  // namespace

ExpireHistoryBackend::ExpireHistoryBackend(
    BroadcastNotificationDelegate* delegate,
    BookmarkService* bookmark_service)
    : delegate_(delegate),
      main_db_(NULL),
      archived_db_(NULL),
      thumb_db_(NULL),
      text_db_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)),
      bookmark_service_(bookmark_service) {
}

ExpireHistoryBackend::~ExpireHistoryBackend() {
}

void ExpireHistoryBackend::SetDatabases(HistoryDatabase* main_db,
                                        ArchivedDatabase* archived_db,
                                        ThumbnailDatabase* thumb_db,
                                        TextDatabaseManager* text_db) {
  main_db_ = main_db;
  archived_db_ = archived_db;
  thumb_db_ = thumb_db;
  text_db_ = text_db;
}

void ExpireHistoryBackend::DeleteURL(const GURL& url) {
  if (!main_db_)
    return;

  URLRow url_row;
  if (!main_db_->GetRowForURL(url, &url_row))
    return;  // Nothing to delete.

  // Collect all the visits and delete them. Note that we don't give up if
  // there are no visits, since the URL could still have an entry that we should
  // delete.
  // TODO(brettw): bug 1171148: We should also delete from the archived DB.
  VisitVector visits;
  main_db_->GetVisitsForURL(url_row.id(), &visits);

  DeleteDependencies dependencies;
  DeleteVisitRelatedInfo(visits, &dependencies);

  // We skip ExpireURLsForVisits (since we are deleting from the URL, and not
  // starting with visits in a given time range). We therefore need to call the
  // deletion and favicon update functions manually.

  BookmarkService* bookmark_service = GetBookmarkService();
  bool is_bookmarked =
      (bookmark_service && bookmark_service->IsBookmarked(url));

  DeleteOneURL(url_row, is_bookmarked, &dependencies);
  if (!is_bookmarked)
    DeleteFaviconsIfPossible(dependencies.affected_favicons);

  if (text_db_)
    text_db_->OptimizeChangedDatabases(dependencies.text_db_changes);

  BroadcastDeleteNotifications(&dependencies);
}

void ExpireHistoryBackend::ExpireHistoryBetween(Time begin_time,
                                                Time end_time) {
  if (!main_db_)
    return;

  // There may be stuff in the text database manager's temporary cache.
  if (text_db_)
    text_db_->DeleteFromUncommitted(begin_time, end_time);

  // Find the affected visits and delete them.
  // TODO(brettw): bug 1171164: We should query the archived database here, too.
  VisitVector visits;
  main_db_->GetAllVisitsInRange(begin_time, end_time, 0, &visits);
  if (visits.empty())
    return;

  DeleteDependencies dependencies;
  DeleteVisitRelatedInfo(visits, &dependencies);

  // Delete or update the URLs affected. We want to update the visit counts
  // since this is called by the user who wants to delete their recent history,
  // and we don't want to leave any evidence.
  ExpireURLsForVisits(visits, &dependencies);
  DeleteFaviconsIfPossible(dependencies.affected_favicons);

  // An is_null begin time means that all history should be deleted.
  BroadcastDeleteNotifications(&dependencies);

  // Pick up any bits possibly left over.
  ParanoidExpireHistory();
}

void ExpireHistoryBackend::ArchiveHistoryBefore(Time end_time) {
  if (!main_db_)
    return;

  // Archive as much history as possible before the given date.
  ArchiveSomeOldHistory(end_time, std::numeric_limits<size_t>::max());
  ParanoidExpireHistory();
}

void ExpireHistoryBackend::StartArchivingOldStuff(
    TimeDelta expiration_threshold) {
  expiration_threshold_ = expiration_threshold;
  ScheduleArchive(TimeDelta::FromSeconds(kExpirationDelaySec));
}

void ExpireHistoryBackend::DeleteFaviconsIfPossible(
    const std::set<FavIconID>& favicon_set) {
  if (!main_db_ || !thumb_db_)
    return;

  for (std::set<FavIconID>::const_iterator i = favicon_set.begin();
       i != favicon_set.end(); ++i) {
    if (!main_db_->IsFavIconUsed(*i))
      thumb_db_->DeleteFavIcon(*i);
  }
}

void ExpireHistoryBackend::BroadcastDeleteNotifications(
    DeleteDependencies* dependencies) {
  if (!dependencies->deleted_urls.empty()) {
    // Broadcast the URL deleted notification. Note that we also broadcast when
    // we were requested to delete everything even if that was a NOP, since
    // some components care to know when history is deleted (it's up to them to
    // determine if they care whether anything was deleted).
    URLsDeletedDetails* deleted_details = new URLsDeletedDetails;
    deleted_details->all_history = false;
    std::vector<URLRow> typed_urls_changed;  // Collect this for later.
    for (size_t i = 0; i < dependencies->deleted_urls.size(); i++) {
      deleted_details->urls.insert(dependencies->deleted_urls[i].url());
      if (dependencies->deleted_urls[i].typed_count() > 0)
        typed_urls_changed.push_back(dependencies->deleted_urls[i]);
    }
    delegate_->BroadcastNotifications(NotificationType::HISTORY_URLS_DELETED,
                                      deleted_details);

    // Broadcast the typed URL changed modification (this updates the inline
    // autocomplete database).
    //
    // Note: if we ever need to broadcast changes to more than just typed URLs,
    // this notification should be changed rather than a new "non-typed"
    // notification added. The in-memory database can always do the filtering
    // itself in that case.
    if (!typed_urls_changed.empty()) {
      URLsModifiedDetails* modified_details = new URLsModifiedDetails;
      modified_details->changed_urls.swap(typed_urls_changed);
      delegate_->BroadcastNotifications(
          NotificationType::HISTORY_TYPED_URLS_MODIFIED,
          modified_details);
    }
  }
}

void ExpireHistoryBackend::DeleteVisitRelatedInfo(
    const VisitVector& visits,
    DeleteDependencies* dependencies) {
  for (size_t i = 0; i < visits.size(); i++) {
    // Delete the visit itself.
    main_db_->DeleteVisit(visits[i]);

    // Add the URL row to the affected URL list.
    std::map<URLID, URLRow>::const_iterator found =
        dependencies->affected_urls.find(visits[i].url_id);
    const URLRow* cur_row;
    if (found == dependencies->affected_urls.end()) {
      URLRow row;
      if (!main_db_->GetURLRow(visits[i].url_id, &row))
        continue;
      dependencies->affected_urls[visits[i].url_id] = row;
      cur_row = &dependencies->affected_urls[visits[i].url_id];
    } else {
      cur_row = &found->second;
    }

    // Delete any associated full-text indexed data.
    if (visits[i].is_indexed && text_db_) {
      text_db_->DeletePageData(visits[i].visit_time, cur_row->url(),
                               &dependencies->text_db_changes);
    }
  }
}

void ExpireHistoryBackend::DeleteOneURL(
    const URLRow& url_row,
    bool is_bookmarked,
    DeleteDependencies* dependencies) {
  main_db_->DeleteSegmentForURL(url_row.id());

  // The URL may be in the text database manager's temporary cache.
  if (text_db_)
    text_db_->DeleteURLFromUncommitted(url_row.url());

  if (!is_bookmarked) {
    dependencies->deleted_urls.push_back(url_row);

    // Delete stuff that references this URL.
    if (thumb_db_)
      thumb_db_->DeleteThumbnail(url_row.id());

    // Collect shared information.
    if (url_row.favicon_id())
      dependencies->affected_favicons.insert(url_row.favicon_id());

    // Last, delete the URL entry.
    main_db_->DeleteURLRow(url_row.id());
  }
}

URLID ExpireHistoryBackend::ArchiveOneURL(const URLRow& url_row) {
  if (!archived_db_)
    return 0;

  // See if this URL is present in the archived database already. Note that
  // we must look up by ID since the URL ID will be different.
  URLRow archived_row;
  if (archived_db_->GetRowForURL(url_row.url(), &archived_row)) {
    // TODO(sky): bug 1168470, need to archive past search terms.
    // FIXME(brettw) should be copy the visit counts over? This will mean that
    // the main DB's visit counts are only for the last 3 months rather than
    // accumulative.
    archived_row.set_last_visit(url_row.last_visit());
    archived_db_->UpdateURLRow(archived_row.id(), archived_row);
    return archived_row.id();
  }

  // This row is not in the archived DB, add it.
  return archived_db_->AddURL(url_row);
}

namespace {

struct ChangedURL {
  ChangedURL() : visit_count(0), typed_count(0) {}
  int visit_count;
  int typed_count;
};

}  // namespace

void ExpireHistoryBackend::ExpireURLsForVisits(
    const VisitVector& visits,
    DeleteDependencies* dependencies) {
  // First find all unique URLs and the number of visits we're deleting for
  // each one.
  std::map<URLID, ChangedURL> changed_urls;
  for (size_t i = 0; i < visits.size(); i++) {
    ChangedURL& cur = changed_urls[visits[i].url_id];
    cur.visit_count++;
    // NOTE: This code must stay in sync with HistoryBackend::AddPageVisit().
    // TODO(pkasting): http://b/1148304 We shouldn't be marking so many URLs as
    // typed, which would eliminate the need for this code.
    const PageTransition::Type transition = visits[i].transition;
    if (PageTransition::StripQualifier(transition) == PageTransition::TYPED &&
        !PageTransition::IsRedirect(transition))
      cur.typed_count++;
  }

  // Check each unique URL with deleted visits.
  BookmarkService* bookmark_service = GetBookmarkService();
  for (std::map<URLID, ChangedURL>::const_iterator i = changed_urls.begin();
       i != changed_urls.end(); ++i) {
    // The unique URL rows should already be filled into the dependencies.
    URLRow& url_row = dependencies->affected_urls[i->first];
    if (!url_row.id())
      continue;  // URL row doesn't exist in the database.

    // Check if there are any other visits for this URL and update the time
    // (the time change may not actually be synced to disk below when we're
    // archiving).
    VisitRow last_visit;
    if (main_db_->GetMostRecentVisitForURL(url_row.id(), &last_visit))
      url_row.set_last_visit(last_visit.visit_time);
    else
      url_row.set_last_visit(Time());

    // Don't delete URLs with visits still in the DB, or bookmarked.
    bool is_bookmarked =
        (bookmark_service && bookmark_service->IsBookmarked(url_row.url()));
    if (!is_bookmarked && url_row.last_visit().is_null()) {
      // Not bookmarked and no more visits. Nuke the url.
      DeleteOneURL(url_row, is_bookmarked, dependencies);
    } else {
      // NOTE: The calls to std::max() below are a backstop, but they should
      // never actually be needed unless the database is corrupt (I think).
      url_row.set_visit_count(
          std::max(0, url_row.visit_count() - i->second.visit_count));
      url_row.set_typed_count(
          std::max(0, url_row.typed_count() - i->second.typed_count));

      // Update the db with the new details.
      main_db_->UpdateURLRow(url_row.id(), url_row);
    }
  }
}

void ExpireHistoryBackend::ArchiveURLsAndVisits(
    const VisitVector& visits,
    DeleteDependencies* dependencies) {
  // Make sure all unique URL rows are added to the dependency list and the
  // archived database. We will also keep the mapping between the main DB URLID
  // and the archived one.
  std::map<URLID, URLID> main_id_to_archived_id;
  for (size_t i = 0; i < visits.size(); i++) {
    std::map<URLID, URLRow>::const_iterator found =
      dependencies->affected_urls.find(visits[i].url_id);
    if (found == dependencies->affected_urls.end()) {
      // Unique URL encountered, archive it.
      URLRow row;  // Row in the main DB.
      URLID archived_id;  // ID in the archived DB.
      if (!main_db_->GetURLRow(visits[i].url_id, &row) ||
          !(archived_id = ArchiveOneURL(row))) {
        // Failure archiving, skip this one.
        continue;
      }

      // Only add URL to the dependency list once we know we successfully
      // archived it.
      main_id_to_archived_id[row.id()] = archived_id;
      dependencies->affected_urls[row.id()] = row;
    }
  }

  // Now archive the visits since we know the URL ID to make them reference.
  // The source visit list should still reference the visits in the main DB, but
  // we will update it to reflect only the visits that were successfully
  // archived.
  for (size_t i = 0; i < visits.size(); i++) {
    // Construct the visit that we will add to the archived database. We do
    // not store referring visits since we delete many of the visits when
    // archiving.
    VisitRow cur_visit(visits[i]);
    cur_visit.url_id = main_id_to_archived_id[cur_visit.url_id];
    cur_visit.referring_visit = 0;
    archived_db_->AddVisit(&cur_visit);
    // Ignore failures, we will delete it from the main DB no matter what.
  }
}

void ExpireHistoryBackend::ScheduleArchive(TimeDelta delay) {
  factory_.RevokeAll();
  MessageLoop::current()->PostDelayedTask(FROM_HERE, factory_.NewRunnableMethod(
          &ExpireHistoryBackend::DoArchiveIteration),
      static_cast<int>(delay.InMilliseconds()));
}

void ExpireHistoryBackend::DoArchiveIteration() {
  DCHECK(expiration_threshold_ != TimeDelta()) << "threshold should be set";
  Time threshold = Time::Now() - expiration_threshold_;

  if (ArchiveSomeOldHistory(threshold, kNumExpirePerIteration)) {
    // Possibly more items to delete now, schedule it sooner to happen again.
    ScheduleArchive(TimeDelta::FromSeconds(kExpirationDelaySec));
  } else {
    // If we didn't find the maximum number of items to delete, wait longer
    // before trying to delete more later.
    ScheduleArchive(TimeDelta::FromMinutes(kExpirationEmptyDelayMin));
  }
}

bool ExpireHistoryBackend::ArchiveSomeOldHistory(Time time_threshold,
                                                 int max_visits) {
  if (!main_db_)
    return false;

  // Get all visits up to and including the threshold. This is a little tricky
  // because GetAllVisitsInRange's end value is non-inclusive, so we have to
  // increment the time by one unit to get the input value to be inclusive.
  DCHECK(!time_threshold.is_null());
  Time effective_threshold =
      Time::FromInternalValue(time_threshold.ToInternalValue() + 1);
  VisitVector affected_visits;
  main_db_->GetAllVisitsInRange(Time(), effective_threshold, max_visits,
                                &affected_visits);

  // Some visits we'll delete while others we'll archive.
  VisitVector deleted_visits, archived_visits;
  for (size_t i = 0; i < affected_visits.size(); i++) {
    if (ShouldArchiveVisit(affected_visits[i]))
      archived_visits.push_back(affected_visits[i]);
    else
      deleted_visits.push_back(affected_visits[i]);
  }

  // Do the actual archiving.
  DeleteDependencies archived_dependencies;
  ArchiveURLsAndVisits(archived_visits, &archived_dependencies);
  DeleteVisitRelatedInfo(archived_visits, &archived_dependencies);

  DeleteDependencies deleted_dependencies;
  DeleteVisitRelatedInfo(deleted_visits, &deleted_dependencies);

  // This will remove or archive all the affected URLs. Must do the deleting
  // cleanup before archiving so the delete dependencies structure references
  // only those URLs that were actually deleted instead of having some visits
  // archived and then the rest deleted.
  ExpireURLsForVisits(deleted_visits, &deleted_dependencies);
  ExpireURLsForVisits(archived_visits, &archived_dependencies);

  // Create a union of all affected favicons (we don't store favicons for
  // archived URLs) and delete them.
  std::set<FavIconID> affected_favicons(
      archived_dependencies.affected_favicons);
  for (std::set<FavIconID>::const_iterator i =
           deleted_dependencies.affected_favicons.begin();
       i != deleted_dependencies.affected_favicons.end(); ++i) {
    affected_favicons.insert(*i);
  }
  DeleteFaviconsIfPossible(affected_favicons);

  // Send notifications for the stuff that was deleted. These won't normally be
  // in history views since they were subframes, but they will be in the visited
  // link system, which needs to be updated now. This function is smart enough
  // to not do anything if nothing was deleted.
  BroadcastDeleteNotifications(&deleted_dependencies);

  // When we got the maximum number of visits we asked for, we say there could
  // be additional things to expire now.
  return static_cast<int>(affected_visits.size()) == max_visits;
}

void ExpireHistoryBackend::ParanoidExpireHistory() {
  // FIXME(brettw): Bug 1067331: write this to clean up any errors.
}

BookmarkService* ExpireHistoryBackend::GetBookmarkService() {
  // We use the bookmark service to determine if a URL is bookmarked. The
  // bookmark service is loaded on a separate thread and may not be done by the
  // time we get here. We therefor block until the bookmarks have finished
  // loading.
  if (bookmark_service_)
    bookmark_service_->BlockTillLoaded();
  return bookmark_service_;
}

}  // namespace history
