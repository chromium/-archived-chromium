// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_model.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"

using base::Time;

// The max number of results to retrieve when browsing user's history.
static const int kMaxBrowseResults = 800;

// The max number of search results to retrieve.
static const int kMaxSearchResults = 100;

HistoryModel::HistoryModel(Profile* profile, const std::wstring& search_text)
    : BaseHistoryModel(profile),
      search_text_(search_text),
      search_depth_(0) {
  // Register for notifications about URL starredness changing on this profile.
  NotificationService::current()->AddObserver(
      this,
      NotificationType::URLS_STARRED,
      Source<Profile>(profile->GetOriginalProfile()));
  NotificationService::current()->AddObserver(
      this,
      NotificationType::HISTORY_URLS_DELETED,
      Source<Profile>(profile->GetOriginalProfile()));
}

HistoryModel::~HistoryModel() {
  // Unregister for notifications about URL starredness.
  NotificationService::current()->RemoveObserver(
      this,
      NotificationType::URLS_STARRED,
      Source<Profile>(profile_->GetOriginalProfile()));
  NotificationService::current()->RemoveObserver(
      this,
      NotificationType::HISTORY_URLS_DELETED,
      Source<Profile>(profile_->GetOriginalProfile()));
}

int HistoryModel::GetItemCount() {
  return static_cast<int>(results_.size());
}

Time HistoryModel::GetVisitTime(int index) {
#ifndef NDEBUG
  DCHECK(IsValidIndex(index));
#endif
  return results_[index].visit_time();
}

const std::wstring& HistoryModel::GetTitle(int index) {
  return results_[index].title();
}

const GURL& HistoryModel::GetURL(int index) {
  return results_[index].url();
}

history::URLID HistoryModel::GetURLID(int index) {
  return results_[index].id();
}

bool HistoryModel::IsStarred(int index) {
  if (star_state_[index] == UNKNOWN) {
    bool is_starred = profile_->GetBookmarkModel()->IsBookmarked(GetURL(index));
    star_state_[index] = is_starred ? STARRED : NOT_STARRED;
  }
  return (star_state_[index] == STARRED);
}

const Snippet& HistoryModel::GetSnippet(int index) {
  return results_[index].snippet();
}

void HistoryModel::RemoveFromModel(int start, int length) {
  DCHECK(start >= 0 && start + length <= GetItemCount());
  results_.DeleteRange(start, start + length);
  if (observer_)
    observer_->ModelChanged(true);
}

void HistoryModel::SetSearchText(const std::wstring& search_text) {
  if (search_text == search_text_)
    return;

  search_text_ = search_text;
  search_depth_ = 0;
  Refresh();
}

void HistoryModel::InitVisitRequest(int depth) {
  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return;

  AboutToScheduleRequest();

  history::QueryOptions options;

  // Limit our search so that it doesn't return more than the maximum required
  // number of results.
  int max_total_results = search_text_.empty() ?
                          kMaxBrowseResults : kMaxSearchResults;

  if (depth == 0) {
    // Set the end time of this first search to null (which will
    // show results from the future, should the user's clock have
    // been set incorrectly).
    options.end_time = Time();

    search_start_ = Time::Now();

    // Configure the begin point of the search to the start of the
    // current month.
    Time::Exploded start_exploded;
    search_start_.LocalMidnight().LocalExplode(&start_exploded);
    start_exploded.day_of_month = 1;
    options.begin_time = Time::FromLocalExploded(start_exploded);

    options.max_count = max_total_results;
  } else {
    Time::Exploded exploded;
    search_start_.LocalMidnight().LocalExplode(&exploded);
    exploded.day_of_month = 1;

    // Set the end-time of this search to the end of the month that is
    // |depth| months before the search end point. The end time is not
    // inclusive, so we should feel free to set it to midnight on the
    // first day of the following month.
    exploded.month -= depth - 1;
    while (exploded.month < 1) {
      exploded.month += 12;
      exploded.year--;
    }
    options.end_time = Time::FromLocalExploded(exploded);

    // Set the begin-time of the search to the start of the month
    // that is |depth| months prior to search_start_.
    if (exploded.month > 1) {
      exploded.month--;
    } else {
      exploded.month = 12;
      exploded.year--;
    }
    options.begin_time = Time::FromLocalExploded(exploded);

    // Subtract off the number of pages we already got.
    options.max_count = max_total_results - static_cast<int>(results_.size());
  }

  // This will make us get only one entry for each page. This is definitely
  // correct for "starred only" queries, but more debatable for regular
  // history queries. We might want to get all of them but then remove adjacent
  // duplicates like Mozilla.
  //
  // We'll still get duplicates across month boundaries, which is probably fine.
  options.most_recent_visit_only = true;

  HistoryService::QueryHistoryCallback* callback =
      NewCallback(this, &HistoryModel::VisitedPagesQueryComplete);
  history_service->QueryHistory(search_text_, options,
                                 &cancelable_consumer_, callback);
}

void HistoryModel::SetPageStarred(int index, bool state) {
  const history::URLResult& result = results_[index];
  if (!UpdateStarredStateOfURL(result.url(), state))
    return;  // Nothing was changed.

  if (observer_)
    observer_->ModelChanged(false);

  BookmarkModel* bb_model = profile_->GetBookmarkModel();
  if (bb_model)
    bb_model->SetURLStarred(result.url(), result.title(), state);
}

void HistoryModel::Refresh() {
  cancelable_consumer_.CancelAllRequests();
  if (observer_)
    observer_->ModelEndWork();
  search_depth_ = 0;
  InitVisitRequest(search_depth_);

  if (results_.size() > 0) {
    // There are results and we've been asked to reload. If we don't swap out
    // the results now, the view is left holding indices that are going to
    // change as soon as the load completes, which poses problems for deletion.
    // In particular, if the user deletes a range, then clicks on delete again
    // a modal dialog is shown. If during the time the modal dialog is shown
    // and the user clicks ok the load completes, the index passed to delete is
    // no longer valid. To avoid this we empty out the results immediately.
    history::QueryResults empty_results;
    results_.Swap(&empty_results);
    if (observer_)
      observer_->ModelChanged(true);
  }
}

void HistoryModel::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::URLS_STARRED: {  // Somewhere a URL has been starred.
      Details<history::URLsStarredDetails> starred_state(details);

      // In the degenerate case when there are a lot of pages starred, this may
      // be unacceptably slow.
      std::set<GURL>::const_iterator i;
      bool changed = false;
      for (i = starred_state->changed_urls.begin();
           i != starred_state->changed_urls.end(); ++i) {
        changed |= UpdateStarredStateOfURL(*i, starred_state->starred);
      }
      if (changed && observer_)
        observer_->ModelChanged(false);
      break;
    }

    case NotificationType::HISTORY_URLS_DELETED:
      // TODO(brettw) bug 1140015: This should actually update the current query
      // rather than re-querying. This should be much more efficient and
      // user-friendly.
      //
      // Note that we can special case when the "all_history" flag is set to just
      // clear the view.
      Refresh();
      break;

    // TODO(brettw) bug 1140015, 1140017, 1140020: Add a more observers to catch
    // title changes, new additions, etc.. Also, URLS_ADDED when that
    // notification exists.

    default:
      NOTREACHED();
      break;
  }
}

void HistoryModel::VisitedPagesQueryComplete(
    HistoryService::Handle request_handle,
    history::QueryResults* results) {
  bool changed = (results->size() > 0);
  if (search_depth_ == 0) {
    if (results_.size() > 0)
      changed = true;
    results_.Swap(results);
  } else {
    results_.AppendResultsBySwapping(results, true);
  }

  is_search_results_ = !search_text_.empty();

  if (changed) {
    star_state_.reset(new StarState[results_.size()]);
    memset(star_state_.get(), 0, sizeof(StarState) * results_.size());
    if (observer_)
      observer_->ModelChanged(true);
  }

  search_depth_++;

  int max_results = search_text_.empty() ?
                    kMaxBrowseResults : kMaxSearchResults;

  // TODO(glen/brettw): bug 1203052 - Need to detect if we've reached the
  // end of the user's history.
  if (search_depth_ < kHistoryScopeMonths &&
      static_cast<int>(results_.size()) < max_results) {
    InitVisitRequest(search_depth_);
  } else {
    RequestCompleted();
  }
}

bool HistoryModel::UpdateStarredStateOfURL(const GURL& url, bool is_starred) {
  bool changed = false;

  // See if we've got any of the changed URLs in our results. There may be
  // more than once instance of the URL, and we have to update them all.
  size_t num_matches;
  const size_t* match_indices = results_.MatchesForURL(url, &num_matches);
  for (size_t i = 0; i < num_matches; i++) {
    if (IsStarred(static_cast<int>(match_indices[i])) != is_starred) {
      star_state_[match_indices[i]] = is_starred ? STARRED : NOT_STARRED;
      changed = true;
    }
  }
  return changed;
}
