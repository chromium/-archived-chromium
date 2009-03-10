// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "chrome/browser/history/history_types.h"

using base::Time;

namespace history {

// URLRow ----------------------------------------------------------------------

void URLRow::Swap(URLRow* other) {
  std::swap(id_, other->id_);
  url_.Swap(&other->url_);
  title_.swap(other->title_);
  std::swap(visit_count_, other->visit_count_);
  std::swap(typed_count_, other->typed_count_);
  std::swap(last_visit_, other->last_visit_);
  std::swap(hidden_, other->hidden_);
  std::swap(favicon_id_, other->favicon_id_);
}

void URLRow::Initialize() {
  id_ = 0;
  visit_count_ = false;
  typed_count_ = false;
  last_visit_ = Time();
  hidden_ = false;
  favicon_id_ = 0;
}

// VisitRow --------------------------------------------------------------------

VisitRow::VisitRow()
    : visit_id(0),
      url_id(0),
      referring_visit(0),
      transition(PageTransition::LINK),
      segment_id(0),
      is_indexed(false) {
}

VisitRow::VisitRow(URLID arg_url_id,
                   Time arg_visit_time,
                   VisitID arg_referring_visit,
                   PageTransition::Type arg_transition,
                   SegmentID arg_segment_id)
    : visit_id(0),
      url_id(arg_url_id),
      visit_time(arg_visit_time),
      referring_visit(arg_referring_visit),
      transition(arg_transition),
      segment_id(arg_segment_id),
      is_indexed(false) {
}

// StarredEntry ----------------------------------------------------------------

StarredEntry::StarredEntry()
    : id(0),
      parent_group_id(0),
      group_id(0),
      visual_order(0),
      type(URL),
      url_id(0) {
}

void StarredEntry::Swap(StarredEntry* other) {
  std::swap(id, other->id);
  title.swap(other->title);
  std::swap(date_added, other->date_added);
  std::swap(parent_group_id, other->parent_group_id);
  std::swap(group_id, other->group_id);
  std::swap(visual_order, other->visual_order);
  std::swap(type, other->type);
  url.Swap(&other->url);
  std::swap(url_id, other->url_id);
  std::swap(date_group_modified, other->date_group_modified);
}

// URLResult -------------------------------------------------------------------

void URLResult::Swap(URLResult* other) {
  URLRow::Swap(other);
  std::swap(visit_time_, other->visit_time_);
  snippet_.Swap(&other->snippet_);
  title_match_positions_.swap(other->title_match_positions_);
}

// QueryResults ----------------------------------------------------------------

QueryResults::QueryResults() {
}

QueryResults::~QueryResults() {
  // Free all the URL objects.
  STLDeleteContainerPointers(results_.begin(), results_.end());
}

const size_t* QueryResults::MatchesForURL(const GURL& url,
                                          size_t* num_matches) const {
  URLToResultIndices::const_iterator found = url_to_results_.find(url);
  if (found == url_to_results_.end()) {
    if (num_matches)
      *num_matches = 0;
    return NULL;
  }

  // All entries in the map should have at least one index, otherwise it
  // shouldn't be in the map.
  DCHECK(found->second->size() > 0);
  if (num_matches)
    *num_matches = found->second->size();
  return &found->second->front();
}

void QueryResults::Swap(QueryResults* other) {
  std::swap(first_time_searched_, other->first_time_searched_);
  results_.swap(other->results_);
  url_to_results_.swap(other->url_to_results_);
}

void QueryResults::AppendURLBySwapping(URLResult* result) {
  URLResult* new_result = new URLResult;
  new_result->Swap(result);

  results_.push_back(new_result);
  AddURLUsageAtIndex(new_result->url(), results_.size() - 1);
}

void QueryResults::AppendResultsBySwapping(QueryResults* other,
                                           bool remove_dupes) {
  if (remove_dupes) {
    // Delete all entries in the other array that are already in this one.
    for (size_t i = 0; i < results_.size(); i++)
      other->DeleteURL(results_[i]->url());
  }

  if (first_time_searched_ > other->first_time_searched_)
    std::swap(first_time_searched_, other->first_time_searched_);

  for (size_t i = 0; i < other->results_.size(); i++) {
    // Just transfer pointer ownership.
    results_.push_back(other->results_[i]);
    AddURLUsageAtIndex(results_.back()->url(), results_.size() - 1);
  }

  // We just took ownership of all the results in the input vector.
  other->results_.clear();
  other->url_to_results_.clear();
}

void QueryResults::DeleteURL(const GURL& url) {
  // Delete all instances of this URL. We re-query each time since each
  // mutation will cause the indices to change.
  while (const size_t* match_indices = MatchesForURL(url, NULL))
    DeleteRange(*match_indices, *match_indices);
}

void QueryResults::DeleteRange(size_t begin, size_t end) {
  DCHECK(begin <= end && begin < size() && end < size());

  // First delete the pointers in the given range and store all the URLs that
  // were modified. We will delete references to these later.
  std::set<GURL> urls_modified;
  for (size_t i = begin; i <= end; i++) {
    urls_modified.insert(results_[i]->url());
    delete results_[i];
    results_[i] = NULL;
  }

  // Now just delete that range in the vector en masse (the STL ending is
  // exclusive, while ours is inclusive, hence the +1).
  results_.erase(results_.begin() + begin, results_.begin() + end + 1);

  // Delete the indicies referencing the deleted entries.
  for (std::set<GURL>::const_iterator url = urls_modified.begin();
       url != urls_modified.end(); ++url) {
    URLToResultIndices::iterator found = url_to_results_.find(*url);
    if (found == url_to_results_.end()) {
      NOTREACHED();
      continue;
    }

    // Need a signed loop type since we do -- which may take us to -1.
    for (int match = 0; match < static_cast<int>(found->second->size());
         match++) {
      if (found->second[match] >= begin && found->second[match] <= end) {
        // Remove this referece from the list.
        found->second->erase(found->second->begin() + match);
        match--;

      }
    }

    // Clear out an empty lists if we just made one.
    if (found->second->empty())
      url_to_results_.erase(found);
  }

  // Shift all other indices over to account for the removed ones.
  AdjustResultMap(end + 1, std::numeric_limits<size_t>::max(),
                  -static_cast<ptrdiff_t>(end - begin + 1));
}

void QueryResults::AddURLUsageAtIndex(const GURL& url, size_t index) {
  URLToResultIndices::iterator found = url_to_results_.find(url);
  if (found != url_to_results_.end()) {
    // The URL is already in the list, so we can just append the new index.
    found->second->push_back(index);
    return;
  }

  // Need to add a new entry for this URL.
  StackVector<size_t, 4> new_list;
  new_list->push_back(index);
  url_to_results_[url] = new_list;
}

void QueryResults::AdjustResultMap(size_t begin, size_t end, ptrdiff_t delta) {
  for (URLToResultIndices::iterator i = url_to_results_.begin();
       i != url_to_results_.end(); ++i) {
    for (size_t match = 0; match < i->second->size(); match++) {
      size_t match_index = i->second[match];
      if (match_index >= begin && match_index <= end)
        i->second[match] += delta;
    }
  }
}

}  // namespace history
