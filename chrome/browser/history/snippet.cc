// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/snippet.h"

#include <algorithm>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "unicode/brkiter.h"
#include "unicode/utext.h"
#include "unicode/utf8.h"

namespace {

bool PairFirstLessThan(const std::pair<int,int>& a,
                       const std::pair<int,int>& b) {
  return a.first < b.first;
}

// Combines all pairs after offset in match_positions that are contained
// or touch the pair at offset.
void CoalescePositionsFrom(size_t offset,
                           Snippet::MatchPositions* match_positions) {
  DCHECK(offset < match_positions->size());
  std::pair<int,int>& pair((*match_positions)[offset]);
  ++offset;
  while (offset < match_positions->size() &&
         pair.second >= (*match_positions)[offset].first) {
    pair.second = std::max(pair.second, (*match_positions)[offset].second);
    match_positions->erase(match_positions->begin() + offset);
  }
}

// Makes sure there is a pair in match_positions that contains the specified
// range. This keeps the pairs ordered in match_positions by first, and makes
// sure none of the pairs in match_positions touch each other.
void AddMatch(int start, int end, Snippet::MatchPositions* match_positions) {
  DCHECK(start < end && match_positions);
  std::pair<int,int> pair(start, end);
  if (match_positions->empty()) {
    match_positions->push_back(pair);
    return;
  }
  // There's at least one match. Find the position of the new match,
  // potentially extending pairs around it.
  Snippet::MatchPositions::iterator i =
      std::lower_bound(match_positions->begin(), match_positions->end(),
                       pair, &PairFirstLessThan);
  if (i != match_positions->end() && i->first == start) {
    // Match not at the end and there is already a pair with the same
    // start.
    if (end > i->second) {
      // New pair extends beyond existing pair. Extend existing pair and
      // coalesce matches after it.
      i->second = end;
      CoalescePositionsFrom(i - match_positions->begin(), match_positions);
    } // else case, new pair completely contained in existing pair, nothing
      // to do.
  } else if (i == match_positions->begin()) {
    // Match at the beginning and the first pair doesn't have the same
    // start. Insert new pair and coalesce matches after it.
    match_positions->insert(i, pair);
    CoalescePositionsFrom(0, match_positions);
  } else {
    // Not at the beginning (but may be at the end).
    --i;
    if (start <= i->second && end > i->second) {
      // Previous element contains match. Extend it and coalesce.
      i->second = end;
      CoalescePositionsFrom(i - match_positions->begin(), match_positions);
    } else if (end > i->second) {
      // Region doesn't touch previous element. See if region touches current
      // element.
      ++i;
      if (i == match_positions->end() || end < i->first) {
        match_positions->insert(i, pair);
      } else {
        i->first = start;
        i->second = end;
        CoalescePositionsFrom(i - match_positions->begin(), match_positions);
      }
    }
  }
}

// Converts an index in a utf8 string into the index in the corresponding wide
// string and returns the wide index. This is intended to be called in a loop
// iterating through a utf8 string.
//
// utf8_string: the utf8 string.
// utf8_length: length of the utf8 string.
// offset: the utf8 offset to convert.
// utf8_pos: current offset in the utf8 string. This is modified and on return
//           matches offset.
// wide_pos: current index in the wide string. This is the same as the return
//           value.
int AdvanceAndReturnWidePos(const char* utf8_string,
                            int utf8_length,
                            int offset,
                            int* utf8_pos,
                            int* wide_pos) {
  DCHECK(offset >= *utf8_pos && offset <= utf8_length);

  UChar32 wide_char;
  while (*utf8_pos < offset) {
    U8_NEXT(utf8_string, *utf8_pos, utf8_length, wide_char);
    *wide_pos += (wide_char <= 0xFFFF) ? 1 : 2;
  }
  return *wide_pos;
}

// Given a character break iterator over a UTF-8 string, set the iterator
// position to |*utf8_pos| and move by |count| characters. |count| can
// be either positive or negative.
void MoveByNGraphemes(BreakIterator* bi, int count, int* utf8_pos) {
  // Ignore the return value. A side effect of the current position
  // being set at or following |*utf8_pos| is exploited here.
  // It's simpler than calling following(n) and then previous().
  // isBoundary() is not very fast, but should be good enough for the
  // snippet generation. If not, revisit the way we scan in ComputeSnippet.
  bi->isBoundary(*utf8_pos);
  bi->next(count);
  *utf8_pos = static_cast<int>(bi->current());
}

// The amount of context to include for a given hit. Note that it's counted
// in terms of graphemes rather than bytes.
const int kSnippetContext = 50;

// Returns true if next match falls within a snippet window
// from the previous match. The window size is counted in terms
// of graphemes rather than bytes in UTF-8.
bool IsNextMatchWithinSnippetWindow(BreakIterator* bi,
                                    int previous_match_end,
                                    int next_match_start) {
  // If it's within a window in terms of bytes, it's certain
  // that it's within a window in terms of graphemes as well.
  if (next_match_start < previous_match_end + kSnippetContext)
    return true;
  bi->isBoundary(previous_match_end);
  // An alternative to this is to call |bi->next()| at most
  // kSnippetContext times, compare |bi->current()| with |next_match_start|
  // after each call and return early if possible. There are other
  // heuristics to speed things up if necessary, but it's not likely that
  // we need to bother.
  bi->next(kSnippetContext);
  int64_t current = bi->current();
  return (next_match_start < current || current == BreakIterator::DONE);
}

}  // namespace

// static
void Snippet::ExtractMatchPositions(const std::string& offsets_str,
                                    const std::string& column_num,
                                    MatchPositions* match_positions) {
  DCHECK(match_positions);
  if (offsets_str.empty())
    return;
  std::vector<std::string> offsets;
  SplitString(offsets_str, ' ', &offsets);
  // SQLite offsets are sets of four integers:
  //   column, query term, match offset, match length
  // Matches within a string are marked by (start, end) pairs.
  for (size_t i = 0; i < offsets.size() - 3; i += 4) {
    if (offsets[i] != column_num)
      continue;
    const int start = atoi(offsets[i+2].c_str());
    const int end = start + atoi(offsets[i+3].c_str());
    AddMatch(start, end, match_positions);
  }
}

// static
void Snippet::ConvertMatchPositionsToWide(
    const std::string& utf8_string,
    Snippet::MatchPositions* match_positions) {
  DCHECK(match_positions);
  int utf8_pos = 0;
  int wide_pos = 0;
  const char* utf8_cstring = utf8_string.c_str();
  const int utf8_length = static_cast<int>(utf8_string.size());
  for (Snippet::MatchPositions::iterator i = match_positions->begin();
       i != match_positions->end(); ++i) {
    i->first = AdvanceAndReturnWidePos(utf8_cstring, utf8_length,
                                       i->first, &utf8_pos, &wide_pos);
    i->second =
        AdvanceAndReturnWidePos(utf8_cstring, utf8_length, i->second, &utf8_pos,
                                &wide_pos);
  }
}

void Snippet::ComputeSnippet(const MatchPositions& match_positions,
                             const std::string& document) {
  // The length of snippets we try to produce.
  // We can generate longer snippets but stop once we cross kSnippetMaxLength.
  const size_t kSnippetMaxLength = 200;


  const std::wstring kEllipsis = L" ... ";

  // Grab the size as an int to cut down on casts later.
  const int document_size = static_cast<int>(document.size());

  UText* document_utext = NULL;
  UErrorCode status = U_ZERO_ERROR;
  document_utext = utext_openUTF8(document_utext, document.data(),
                                  document_size, &status);
  // Locale does not matter because there's no per-locale customization
  // for character iterator.
  scoped_ptr<BreakIterator> bi(
    BreakIterator::createCharacterInstance(Locale::getDefault(), status));
  bi->setText(document_utext, status);
  DCHECK(U_SUCCESS(status));

  // We build the snippet by iterating through the matches and then grabbing
  // context around each match.  If matches are near enough each other (within
  // kSnippetContext), we skip the "..." between them.
  std::wstring snippet;
  int start = 0;
  for (size_t i = 0; i < match_positions.size(); ++i) {
    // Some shorter names for the current match.
    const int match_start = match_positions[i].first;
    const int match_end = match_positions[i].second;

    // Add the context, if any, to show before the match.
    int context_start = match_start;
    MoveByNGraphemes(bi.get(), -kSnippetContext, &context_start);
    start = std::max(start, context_start);
    if (start < match_start) {
      if (start > 0)
        snippet += kEllipsis;
      snippet += UTF8ToWide(document.substr(start, match_start - start));
    }

    // Add the match.
    matches_.push_back(std::make_pair(static_cast<int>(snippet.size()), 0));
    snippet += UTF8ToWide(document.substr(match_start,
                                          match_end - match_start));
    matches_.back().second = static_cast<int>(snippet.size());

    // Compute the context, if any, to show after the match.
    int end;
    // Check if the next match falls within our snippet window.
    if (i + 1 < match_positions.size() &&
        IsNextMatchWithinSnippetWindow(bi.get(), match_end,
          match_positions[i + 1].first)) {
      // Yes, it's within the window.  Make the end context extend just up
      // to the next match.
      end = match_positions[i + 1].first;
      snippet += UTF8ToWide(document.substr(match_end, end - match_end));
    } else {
      // No, there's either no next match or the next match is too far away.
      end = match_end;
      MoveByNGraphemes(bi.get(), kSnippetContext, &end);
      snippet += UTF8ToWide(document.substr(match_end, end - match_end));
      if (end < document_size)
        snippet += kEllipsis;
    }
    start = end;

    // Stop here if we have enough snippet computed.
    if (snippet.size() >= kSnippetMaxLength)
      break;
  }

  utext_close(document_utext);
  swap(text_, snippet);
}

