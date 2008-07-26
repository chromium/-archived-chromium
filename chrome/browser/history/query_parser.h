// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
