// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/history/query_parser.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/word_iterator.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/scoped_vector.h"
#include "unicode/uscript.h"

namespace {

// Returns true if |mp1.first| is less than |mp2.first|. This is used to
// sort match positions.
int CompareMatchPosition(const Snippet::MatchPosition& mp1,
                         const Snippet::MatchPosition& mp2) {
  return mp1.first < mp2.first;
}

// Returns true if |mp2| intersects |mp1|. This is intended for use by
// CoalesceMatchesFrom and isn't meant as a general intersectpion comparison
// function.
bool SnippetIntersects(const Snippet::MatchPosition& mp1,
                       const Snippet::MatchPosition& mp2) {
  return mp2.first >= mp1.first && mp2.first <= mp1.second;
}

// Coalesces match positions in |matches| after index that intersect the match
// position at |index|.
void CoalesceMatchesFrom(size_t index,
                         Snippet::MatchPositions* matches) {
  Snippet::MatchPosition& mp = (*matches)[index];
  for (Snippet::MatchPositions::iterator i = matches->begin() + index + 1;
       i != matches->end(); ) {
    if (SnippetIntersects(mp, *i)) {
      mp.second = i->second;
      i = matches->erase(i);
    } else {
      return;
    }
  }
}

// Sorts the match positions in |matches| by their first index, then coalesces
// any match positions that intersect each other.
void CoalseAndSortMatchPositions(Snippet::MatchPositions* matches) {
  std::sort(matches->begin(), matches->end(), &CompareMatchPosition);
  // WARNING: we don't use iterator here as CoalesceMatchesFrom may remove
  // from matches.
  for (size_t i = 0; i < matches->size(); ++i)
    CoalesceMatchesFrom(i, matches);
}

// For CJK ideographs and Korean Hangul, even a single character
// can be useful in prefix matching, but that may give us too many
// false positives. Moreover, the current ICU word breaker gives us
// back every single Chinese character as a word so that there's no
// point doing anything for them and we only adjust the minimum length
// to 2 for Korean Hangul while using 3 for others. This is a temporary
// hack until we have a segmentation support.
inline bool IsWordLongEnoughForPrefixSearch(const std::wstring& word)
{
  DCHECK(word.size() > 0);
  size_t minimum_length = 3;
  // We intentionally exclude Hangul Jamos (both Conjoining and compatibility)
  // because they 'behave like' Latin letters. Moreover, we should
  // normalize the former before reaching here.
  if (0xAC00 <= word[0] && word[0] <= 0xD7A3)
    minimum_length = 2;
  return word.size() >= minimum_length;
}

} // namespace

// Inheritance structure:
// Queries are represented as trees of QueryNodes.
// QueryNodes are either a collection of subnodes (a QueryNodeList)
// or a single word (a QueryNodeWord).

// A QueryNodeWord is a single word in the query.
class QueryNodeWord : public QueryNode {
 public:
  QueryNodeWord(const std::wstring& word) : word_(word), literal_(false) {}
  virtual ~QueryNodeWord() {}
  virtual int AppendToSQLiteQuery(std::wstring* query) const;
  virtual bool IsWord() const { return true; }

  const std::wstring& word() const { return word_; }
  void set_literal(bool literal) { literal_ = literal; }

  virtual bool HasMatchIn(const std::vector<QueryWord>& words,
                          Snippet::MatchPositions* match_positions) const;

  virtual bool Matches(const std::wstring& word, bool exact) const;
  virtual void AppendWords(std::vector<std::wstring>* words) const;

 private:
  std::wstring word_;
  bool literal_;
};

bool QueryNodeWord::HasMatchIn(const std::vector<QueryWord>& words,
                               Snippet::MatchPositions* match_positions) const {
  for (size_t i = 0; i < words.size(); ++i) {
    if (Matches(words[i].word, false)) {
      size_t match_start = words[i].position;
      match_positions->push_back(
          Snippet::MatchPosition(match_start,
                                 match_start + static_cast<int>(word_.size())));
      return true;
    }
  }
  return false;
}

bool QueryNodeWord::Matches(const std::wstring& word, bool exact) const {
  if (exact || !IsWordLongEnoughForPrefixSearch(word_))
    return word == word_;
  return word.size() >= word_.size() &&
         (word_.compare(0, word_.size(), word, 0, word_.size()) == 0);
}

void QueryNodeWord::AppendWords(std::vector<std::wstring>* words) const {
  words->push_back(word_);
}

int QueryNodeWord::AppendToSQLiteQuery(std::wstring* query) const {
  query->append(word_);

  // Use prefix search if we're not literal and long enough.
  if (!literal_ && IsWordLongEnoughForPrefixSearch(word_))
    *query += L'*';
  return 1;
}

// A QueryNodeList has a collection of child QueryNodes
// which it cleans up after.
class QueryNodeList : public QueryNode {
 public:
  virtual ~QueryNodeList();

  virtual int AppendToSQLiteQuery(std::wstring* query) const {
    return AppendChildrenToString(query);
  }
  virtual bool IsWord() const { return false; }

  void AddChild(QueryNode* node) { children_.push_back(node); }

  typedef std::vector<QueryNode*> QueryNodeVector;
  QueryNodeVector* children() { return &children_; }

  // Remove empty subnodes left over from other parsing.
  void RemoveEmptySubnodes();

  // QueryNodeList is never used with Matches or HasMatchIn.
  virtual bool Matches(const std::wstring& word, bool exact) const {
    NOTREACHED();
    return false;
  }
  virtual bool HasMatchIn(const std::vector<QueryWord>& words,
                          Snippet::MatchPositions* match_positions) const {
    NOTREACHED();
    return false;
  }
  virtual void AppendWords(std::vector<std::wstring>* words) const;

 protected:
  int AppendChildrenToString(std::wstring* query) const;

  QueryNodeVector children_;
};

QueryNodeList::~QueryNodeList() {
  for (QueryNodeVector::iterator node = children_.begin();
       node != children_.end(); ++node)
    delete *node;
}

void QueryNodeList::RemoveEmptySubnodes() {
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i]->IsWord())
      continue;

    QueryNodeList* list_node = static_cast<QueryNodeList*>(children_[i]);
    list_node->RemoveEmptySubnodes();
    if (list_node->children()->empty()) {
      children_.erase(children_.begin() + i);
      --i;
      delete list_node;
    }
  }
}

void QueryNodeList::AppendWords(std::vector<std::wstring>* words) const {
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->AppendWords(words);
}

int QueryNodeList::AppendChildrenToString(std::wstring* query) const {
  int num_words = 0;
  for (QueryNodeVector::const_iterator node = children_.begin();
       node != children_.end(); ++node) {
    if (node != children_.begin())
      query->push_back(L' ');
    num_words += (*node)->AppendToSQLiteQuery(query);
  }
  return num_words;
}

// A QueryNodePhrase is a phrase query ("quoted").
class QueryNodePhrase : public QueryNodeList {
 public:
  virtual int AppendToSQLiteQuery(std::wstring* query) const {
    query->push_back(L'"');
    int num_words = AppendChildrenToString(query);
    query->push_back(L'"');
    return num_words;
  }

  virtual bool Matches(const std::wstring& word, bool exact) const;
  virtual bool HasMatchIn(const std::vector<QueryWord>& words,
                          Snippet::MatchPositions* match_positions) const;
};

bool QueryNodePhrase::Matches(const std::wstring& word, bool exact) const {
  NOTREACHED();
  return false;
}

bool QueryNodePhrase::HasMatchIn(
    const std::vector<QueryWord>& words,
    Snippet::MatchPositions* match_positions) const {
  if (words.size() < children_.size())
    return false;

  for (size_t i = 0, max = words.size() - children_.size() + 1; i < max; ++i) {
    bool matched_all = true;
    for (size_t j = 0; j < children_.size(); ++j) {
      if (!children_[j]->Matches(words[i + j].word, true)) {
        matched_all = false;
        break;
      }
    }
    if (matched_all) {
      const QueryWord& last_word = words[i + children_.size() - 1];
      match_positions->push_back(
          Snippet::MatchPosition(words[i].position,
                                 last_word.position + last_word.word.length()));
      return true;
    }
  }
  return false;
}

QueryParser::QueryParser() {
}

// Returns true if the character is considered a quote.
static bool IsQueryQuote(wchar_t ch) {
  return ch == '"' ||
         ch == 0xab ||    // left pointing double angle bracket
         ch == 0xbb ||    // right pointing double angle bracket
         ch == 0x201c ||  // left double quotation mark
         ch == 0x201d ||  // right double quotation mark
         ch == 0x201e;    // double low-9 quotation mark
}

int QueryParser::ParseQuery(const std::wstring& query,
                            std::wstring* sqlite_query) {
  QueryNodeList root;
  if (!ParseQueryImpl(query, &root))
    return 0;
  return root.AppendToSQLiteQuery(sqlite_query);
}

void QueryParser::ParseQuery(const std::wstring& query,
                             std::vector<QueryNode*>* nodes) {
  QueryNodeList root;
  if (ParseQueryImpl(l10n_util::ToLower(query), &root))
    nodes->swap(*root.children());
}


void QueryParser::ExtractQueryWords(const std::wstring& query,
                                    std::vector<std::wstring>* words) {
  QueryNodeList root;
  if (!ParseQueryImpl(query, &root))
    return;
  root.AppendWords(words);
}

bool QueryParser::DoesQueryMatch(const std::wstring& text,
                                 const std::vector<QueryNode*>& query_nodes,
                                 Snippet::MatchPositions* match_positions) {
  if (query_nodes.empty())
    return false;

  std::vector<QueryWord> query_words;
  ExtractQueryWords(l10n_util::ToLower(text), &query_words);

  if (query_words.empty())
    return false;

  Snippet::MatchPositions matches;
  for (size_t i = 0; i < query_nodes.size(); ++i) {
    if (!query_nodes[i]->HasMatchIn(query_words, &matches))
      return false;
  }
  CoalseAndSortMatchPositions(&matches);
  match_positions->swap(matches);
  return true;
}

bool QueryParser::ParseQueryImpl(const std::wstring& query,
                                QueryNodeList* root) {
  WordIterator iter(query, WordIterator::BREAK_WORD);
  // TODO(evanm): support a locale here
  if (!iter.Init())
    return false;

  // To handle nesting, we maintain a stack of QueryNodeLists.
  // The last element (back) of the stack contains the current, deepest node.
  std::vector<QueryNodeList*> query_stack;
  query_stack.push_back(root);

  bool in_quotes = false;  // whether we're currently in a quoted phrase
  while (iter.Advance()) {
    // Just found a span between 'prev' (inclusive) and 'pos' (exclusive). It
    // is not necessarily a word, but could also be a sequence of punctuation
    // or whitespace.
    if (iter.IsWord()) {
      std::wstring word = iter.GetWord();

      QueryNodeWord* word_node = new QueryNodeWord(word);
      if (in_quotes)
        word_node->set_literal(true);
      query_stack.back()->AddChild(word_node);
    } else {  // Punctuation.
      if (IsQueryQuote(query[iter.prev()])) {
        if (!in_quotes) {
          QueryNodeList* quotes_node = new QueryNodePhrase;
          query_stack.back()->AddChild(quotes_node);
          query_stack.push_back(quotes_node);
          in_quotes = true;
        } else {
          query_stack.pop_back();  // stop adding to the quoted phrase
          in_quotes = false;
        }
      }
    }
  }

  root->RemoveEmptySubnodes();
  return true;
}

void QueryParser::ExtractQueryWords(const std::wstring& text,
                                    std::vector<QueryWord>* words) {
  WordIterator iter(text, WordIterator::BREAK_WORD);
  // TODO(evanm): support a locale here
  if (!iter.Init())
    return;

  while (iter.Advance()) {
    // Just found a span between 'prev' (inclusive) and 'pos' (exclusive). It
    // is not necessarily a word, but could also be a sequence of punctuation
    // or whitespace.
    if (iter.IsWord()) {
      std::wstring word = iter.GetWord();
      if (!word.empty()) {
        words->push_back(QueryWord());
        words->back().word = word;
        words->back().position = iter.prev();
      }
    }
  }
}
