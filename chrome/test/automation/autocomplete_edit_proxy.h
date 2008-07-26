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

#ifndef CHROME_TEST_AUTOMATION_AUTOCOMPLETE_EDIT_PROXY_H__
#define CHROME_TEST_AUTOMATION_AUTOCOMPLETE_EDIT_PROXY_H__

#include <string>
#include <vector>

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/test/automation/automation_handle_tracker.h"

// The purpose of this class is to act as a searializable version of
// AutocompleteMatch. The reason for this class is because we don't want to
// searialize all elements of AutocompleteMatch and we want some data from the
// autocomplete provider without the hassle of serializing it.
struct AutocompleteMatchData {
 public:
  AutocompleteMatchData() {}
  explicit AutocompleteMatchData(const AutocompleteMatch& match)
    : contents(match.contents),
      deletable(match.deletable),
      description(match.description),
      destination_url(match.destination_url),
      fill_into_edit(match.fill_into_edit),
      inline_autocomplete_offset(match.inline_autocomplete_offset),
      is_history_what_you_typed_match(match.is_history_what_you_typed_match),
      provider_name(match.provider->name()),
      relevance(match.relevance),
      starred(match.starred) {
    switch (match.type) {
      case AutocompleteMatch::URL:
        str_type = L"URL";
        break;
      case AutocompleteMatch::KEYWORD:
        str_type = L"KEYWORD";
        break;
      case AutocompleteMatch::SEARCH:
        str_type = L"SEARCH";
        break;
      case AutocompleteMatch::HISTORY_SEARCH:
        str_type = L"HISTORY";
        break;
      default:
        NOTREACHED();
    }
  }
  std::wstring contents;
  bool deletable;
  std::wstring description;
  std::wstring destination_url;
  std::wstring fill_into_edit;
  size_t inline_autocomplete_offset;
  bool is_history_what_you_typed_match;
  std::string provider_name;
  int relevance;
  bool starred;
  std::wstring str_type;
};
typedef std::vector<AutocompleteMatchData> Matches;

namespace IPC {

template <>
struct ParamTraits<AutocompleteMatchData> {
  typedef AutocompleteMatchData param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteWString(p.contents);
    m->WriteBool(p.deletable);
    m->WriteWString(p.description);
    m->WriteWString(p.destination_url);
    m->WriteWString(p.fill_into_edit);
    m->WriteSize(p.inline_autocomplete_offset);
    m->WriteBool(p.is_history_what_you_typed_match);
    m->WriteString(p.provider_name);
    m->WriteInt(p.relevance);
    m->WriteBool(p.starred);
    m->WriteWString(p.str_type);
  }

  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadWString(iter, &r->contents) &&
           m->ReadBool(iter, &r->deletable) &&
           m->ReadWString(iter, &r->description) &&
           m->ReadWString(iter, &r->destination_url) &&
           m->ReadWString(iter, &r->fill_into_edit) &&
           m->ReadSize(iter, &r->inline_autocomplete_offset) &&
           m->ReadBool(iter, &r->is_history_what_you_typed_match) &&
           m->ReadString(iter, &r->provider_name) &&
           m->ReadInt(iter, &r->relevance) &&
           m->ReadBool(iter, &r->starred) &&
           m->ReadWString(iter, &r->str_type);
  }

  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"[%S %s %S %S %S %d %s %s %d %s %S]",
      p.contents, p.deletable ? "true" : "false", p.description,
      p.destination_url, p.fill_into_edit, p.inline_autocomplete_offset,
      p.is_history_what_you_typed_match ? "true" : "false", p.provider_name,
      p.relevance, p.starred ? "true" : "false", p.str_type));
  }
};
}  // namespace IPC

class AutocompleteEditProxy : public AutomationResourceProxy {
 public:
  AutocompleteEditProxy(AutomationMessageSender* sender,
                        AutomationHandleTracker* tracker,
                        int handle)
      : AutomationResourceProxy(tracker, sender, handle) {}
  virtual ~AutocompleteEditProxy() {}

  // All these functions return true if the autocomplete edit is valid and
  // there are no IPC errors.

  // Gets the text visible in the omnibox.
  bool GetText(std::wstring* text) const;

  // Sets the text visible in the omnibox.
  bool SetText(const std::wstring& text);

  // Determines if a query to an autocomplete provider is still in progress.
  // NOTE: No autocomplete queries will be made if the omnibox doesn't have
  // focus.  This can be achieved by sending a IDC_FOCUS_LOCATION accelerator
  // to the browser.
  bool IsQueryInProgress(bool* query_in_progress) const;

  // Gets a list of autocomplete matches that have been gathered so far.
  bool GetAutocompleteMatches(Matches* matches) const;

  // Waits for all queries to autocomplete providers to complete.
  // |wait_timeout_ms| gives the number of milliseconds to wait for the query
  // to finish. Returns false if IPC call failed or if the function times out.
  bool WaitForQuery(int wait_timeout_ms) const;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(AutocompleteEditProxy);
};

#endif  // #define CHROME_TEST_AUTOMATION_AUTOCOMPLETE_EDIT_PROXY_H__

