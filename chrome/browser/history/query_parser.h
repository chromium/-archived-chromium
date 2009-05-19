// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The query parser is used to parse queries entered into the history
// search into more normalized queries can be passed to the SQLite backend.

#ifndef CHROME_BROWSER_HISTORY_QUERY_PARSER_H__
#define CHROME_BROWSER_HISTORY_QUERY_PARSER_H__

#include <set>
#include <vector>

#include "chrome/browser/history/snippet.h"

class QueryNodeList;

// Used by HasMatchIn.
struct QueryWord {
  // The work to match against.
  std::wstring word;

  // The starting position of the word in the original text.
  size_t position;
};

// QueryNode is used by QueryNodeParser to represent the elements that
// constitute a query. While QueryNode is exposed by way of ParseQuery, it
// really isn't meant for external usage.
class QueryNode {
 public:
  virtual ~QueryNode() {}

  // Serialize ourselves out to a string that can be passed to SQLite. Returns
  // the number of words in this node.
  virtual int AppendToSQLiteQuery(std::wstring* query) const = 0;

  // Return true if this is a word node, false if it's a QueryNodeList.
  virtual bool IsWord() const = 0;

  // Returns true if this node matches the specified text. If exact is true,
  // the string must exactly match. Otherwise, this uses a starts with
  // comparison.
  virtual bool Matches(const std::wstring& word, bool exact) const = 0;

  // Returns true if this node matches at least one of the words in words. If
  // the node matches at least one word, an entry is added to match_positions
  // giving the matching region.
  virtual bool HasMatchIn(const std::vector<QueryWord>& words,
                          Snippet::MatchPositions* match_positions) const = 0;

  // Appends the words that make up this node in |words|.
  virtual void AppendWords(std::vector<std::wstring>* words) const = 0;
};


class QueryParser {
 public:
  QueryParser();

  // For CJK ideographs and Korean Hangul, even a single character
  // can be useful in prefix matching, but that may give us too many
  // false positives. Moreover, the current ICU word breaker gives us
  // back every single Chinese character as a word so that there's no
  // point doing anything for them and we only adjust the minimum length
  // to 2 for Korean Hangul while using 3 for others. This is a temporary
  // hack until we have a segmentation support.
  static bool IsWordLongEnoughForPrefixSearch(const std::wstring& word);

  // Parse a query into a SQLite query. The resulting query is placed in
  // sqlite_query and the number of words is returned.
  int ParseQuery(const std::wstring& query,
                 std::wstring* sqlite_query);

  // Parses the query words in query, returning the nodes that constitute the
  // valid words in the query. This is intended for later usage with
  // DoesQueryMatch.
  // Ownership of the nodes passes to the caller.
  void ParseQuery(const std::wstring& query,
                  std::vector<QueryNode*>* nodes);

  // Parses a query returning the words that make up the query. Any words in
  // quotes are put in |words| without the quotes. For example, the query text
  // "foo bar" results in two entries being added to words, one for foo and one
  // for bar.
  void ExtractQueryWords(const std::wstring& query,
                         std::vector<std::wstring>* words);

  // Returns true if the string text matches the query nodes created by a call
  // to ParseQuery. If the query does match each of the matching positions in
  // the text is added to |match_positions|.
  bool DoesQueryMatch(const std::wstring& text,
                      const std::vector<QueryNode*>& nodes,
                      Snippet::MatchPositions* match_positions);

 private:
  // Does the work of parsing a query; creates nodes in QueryNodeList as
  // appropriate. This is invoked from both of the ParseQuery methods.
  bool ParseQueryImpl(const std::wstring& query,
                      QueryNodeList* root);

  // Extracts the words from text, placing each word into words.
  void ExtractQueryWords(const std::wstring& text,
                         std::vector<QueryWord>* words);
};

#endif  // CHROME_BROWSER_HISTORY_QUERY_PARSER_H__
