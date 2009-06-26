// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_index.h"

#include <algorithm>
#include <iterator>

#include "app/l10n_util.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/query_parser.h"

BookmarkIndex::NodeSet::const_iterator
    BookmarkIndex::Match::nodes_begin() const {
  return nodes.empty() ? terms.front()->second.begin() : nodes.begin();
}

BookmarkIndex::NodeSet::const_iterator BookmarkIndex::Match::nodes_end() const {
  return nodes.empty() ? terms.front()->second.end() : nodes.end();
}

void BookmarkIndex::Add(const BookmarkNode* node) {
  if (!node->is_url())
    return;
  std::vector<std::wstring> terms = ExtractQueryWords(node->GetTitle());
  for (size_t i = 0; i < terms.size(); ++i)
    RegisterNode(terms[i], node);
}

void BookmarkIndex::Remove(const BookmarkNode* node) {
  if (!node->is_url())
    return;

  std::vector<std::wstring> terms = ExtractQueryWords(node->GetTitle());
  for (size_t i = 0; i < terms.size(); ++i)
    UnregisterNode(terms[i], node);
}

void BookmarkIndex::GetBookmarksWithTitlesMatching(
    const std::wstring& query,
    size_t max_count,
    std::vector<bookmark_utils::TitleMatch>* results) {
  std::vector<std::wstring> terms = ExtractQueryWords(query);
  if (terms.empty())
    return;

  Matches matches;
  for (size_t i = 0; i < terms.size(); ++i) {
    if (!GetBookmarksWithTitleMatchingTerm(terms[i], i == 0, &matches))
      return;
  }

  // We use a QueryParser to fill in match positions for us. It's not the most
  // efficient way to go about this, but by the time we get here we know what
  // matches and so this shouldn't be performance critical.
  QueryParser parser;
  ScopedVector<QueryNode> query_nodes;
  parser.ParseQuery(query, &query_nodes.get());
  for (size_t i = 0; i < matches.size() && results->size() < max_count; ++i) {
    AddMatchToResults(matches[i], max_count, &parser, query_nodes.get(),
                      results);
  }
}

void BookmarkIndex::AddMatchToResults(
    const Match& match,
    size_t max_count,
    QueryParser* parser,
    const std::vector<QueryNode*>& query_nodes,
    std::vector<bookmark_utils::TitleMatch>* results) {
  for (NodeSet::const_iterator i = match.nodes_begin();
       i != match.nodes_end() && results->size() < max_count; ++i) {
    results->push_back(bookmark_utils::TitleMatch());
    bookmark_utils::TitleMatch& title_match = results->back();
    title_match.node = *i;
    if (!parser->DoesQueryMatch((*i)->GetTitle(), query_nodes,
                                &(title_match.match_positions))) {
      // If we get here it implies the QueryParser didn't match something we
      // thought should match. We should always match the same thing as the
      // query parser.
      NOTREACHED();
    }
  }
}

bool BookmarkIndex::GetBookmarksWithTitleMatchingTerm(const std::wstring& term,
                                                      bool first_term,
                                                      Matches* matches) {
  Index::const_iterator i = index_.lower_bound(term);
  if (i == index_.end())
    return false;

  if (!QueryParser::IsWordLongEnoughForPrefixSearch(term)) {
    // Term is too short for prefix match, compare using exact match.
    if (i->first != term)
      return false;  // No bookmarks with this term.

    if (first_term) {
      Match match;
      match.terms.push_back(i);
      matches->push_back(match);
      return true;
    }
    CombineMatchesInPlace(i, matches);
  } else if (first_term) {
    // This is the first term and we're doing a prefix match. Loop through
    // index adding all entries that start with term to matches.
    while (i != index_.end() &&
           i->first.size() >= term.size() &&
           term.compare(0, term.size(), i->first, 0, term.size()) == 0) {
      Match match;
      match.terms.push_back(i);
      matches->push_back(match);
      ++i;
    }
  } else {
    // Prefix match and not the first term. Loop through index combining
    // current matches in matches with term, placing result in result.
    Matches result;
    while (i != index_.end() &&
           i->first.size() >= term.size() &&
           term.compare(0, term.size(), i->first, 0, term.size()) == 0) {
      CombineMatches(i, *matches, &result);
      ++i;
    }
    matches->swap(result);
  }
  return !matches->empty();
}

void BookmarkIndex::CombineMatchesInPlace(const Index::const_iterator& index_i,
                                          Matches* matches) {
  for (size_t i = 0; i < matches->size(); ) {
    Match* match = &((*matches)[i]);
    NodeSet intersection;
    std::set_intersection(match->nodes_begin(), match->nodes_end(),
                          index_i->second.begin(), index_i->second.end(),
                          std::inserter(intersection, intersection.begin()));
    if (intersection.empty()) {
      matches->erase(matches->begin() + i);
    } else {
      match->terms.push_back(index_i);
      match->nodes.swap(intersection);
      ++i;
    }
  }
}

void BookmarkIndex::CombineMatches(const Index::const_iterator& index_i,
                                   const Matches& current_matches,
                                   Matches* result) {
  for (size_t i = 0; i < current_matches.size(); ++i) {
    const Match& match = current_matches[i];
    NodeSet intersection;
    std::set_intersection(match.nodes_begin(), match.nodes_end(),
                          index_i->second.begin(), index_i->second.end(),
                          std::inserter(intersection, intersection.begin()));
    if (!intersection.empty()) {
      result->push_back(Match());
      Match& combined_match = result->back();
      combined_match.terms = match.terms;
      combined_match.terms.push_back(index_i);
      combined_match.nodes.swap(intersection);
    }
  }
}

std::vector<std::wstring> BookmarkIndex::ExtractQueryWords(
    const std::wstring& query) {
  std::vector<std::wstring> terms;
  if (query.empty())
    return terms;
  QueryParser parser;
  // TODO: use ICU normalization.
  parser.ExtractQueryWords(l10n_util::ToLower(query), &terms);
  return terms;
}

void BookmarkIndex::RegisterNode(const std::wstring& term,
                                 const BookmarkNode* node) {
  if (std::find(index_[term].begin(), index_[term].end(), node) !=
      index_[term].end()) {
    // We've already added node for term.
    return;
  }
  index_[term].insert(node);
}

void BookmarkIndex::UnregisterNode(const std::wstring& term,
                                   const BookmarkNode* node) {
  Index::iterator i = index_.find(term);
  if (i == index_.end()) {
    // We can get here if the node has the same term more than once. For
    // example, a bookmark with the title 'foo foo' would end up here.
    return;
  }
  i->second.erase(node);
  if (i->second.empty())
    index_.erase(i);
}
