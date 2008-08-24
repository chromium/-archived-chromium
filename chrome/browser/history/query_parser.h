// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The query parser is used to parse queries entered into the history
// search into more normalized queries can be passed to the SQLite backend.

#ifndef CHROME_BROWSER_HISTORY_QUERY_PARSER_H__
#define CHROME_BROWSER_HISTORY_QUERY_PARSER_H__

#include <set>
#include <vector>

class QueryNodeList;

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

  // Returns true if this node matches at least one of the words in words.
  virtual bool HasMatchIn(const std::vector<std::wstring>& words) const = 0;
};


class QueryParser {
 public:
  QueryParser();

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

  // Returns true if the string text matches the query nodes created by a call
  // to ParseQuery.
  bool DoesQueryMatch(const std::wstring& text,
                      const std::vector<QueryNode*>& nodes);

 private:
  // Does the work of parsing a query; creates nodes in QueryNodeList as
  // appropriate. This is invoked from both of the ParseQuery methods.
  bool ParseQueryImpl(const std::wstring& query,
                      QueryNodeList* root);

  // Extracts the words from text, placing each word into words.
  void ExtractWords(const std::wstring& text,
                    std::vector<std::wstring>* words);
};

#endif  // CHROME_BROWSER_HISTORY_QUERY_PARSER_H__

