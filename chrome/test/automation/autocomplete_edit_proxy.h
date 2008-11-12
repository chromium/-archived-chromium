// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOCOMPLETE_EDIT_PROXY_H__
#define CHROME_TEST_AUTOMATION_AUTOCOMPLETE_EDIT_PROXY_H__

#include <string>
#include <vector>

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/test/automation/automation_handle_tracker.h"

// The purpose of this class is to act as a serializable version of
// AutocompleteMatch. The reason for this class is because we don't want to
// serialize all elements of AutocompleteMatch and we want some data from the
// autocomplete provider without the hassle of serializing it.
struct AutocompleteMatchData {
 public:
  AutocompleteMatchData() {}
  explicit AutocompleteMatchData(const AutocompleteMatch& match)
    : provider_name(match.provider->name()),
      relevance(match.relevance),
      deletable(match.deletable),
      fill_into_edit(match.fill_into_edit),
      inline_autocomplete_offset(match.inline_autocomplete_offset),
      destination_url(match.destination_url),
      contents(match.contents),
      description(match.description),
      is_history_what_you_typed_match(match.is_history_what_you_typed_match),
      type(AutocompleteMatch::TypeToString(match.type)),
      starred(match.starred) {
  }

  std::string provider_name;
  int relevance;
  bool deletable;
  std::wstring fill_into_edit;
  size_t inline_autocomplete_offset;
  std::wstring destination_url;
  std::wstring contents;
  std::wstring description;
  bool is_history_what_you_typed_match;
  std::string type;
  bool starred;
};
typedef std::vector<AutocompleteMatchData> Matches;

namespace IPC {

template <>
struct ParamTraits<AutocompleteMatchData> {
  typedef AutocompleteMatchData param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteString(p.provider_name);
    m->WriteInt(p.relevance);
    m->WriteBool(p.deletable);
    m->WriteWString(p.fill_into_edit);
    m->WriteSize(p.inline_autocomplete_offset);
    m->WriteWString(p.destination_url);
    m->WriteWString(p.contents);
    m->WriteWString(p.description);
    m->WriteBool(p.is_history_what_you_typed_match);
    m->WriteString(p.type);
    m->WriteBool(p.starred);
  }

  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadString(iter, &r->provider_name) &&
           m->ReadInt(iter, &r->relevance) &&
           m->ReadBool(iter, &r->deletable) &&
           m->ReadWString(iter, &r->fill_into_edit) &&
           m->ReadSize(iter, &r->inline_autocomplete_offset) &&
           m->ReadWString(iter, &r->destination_url) &&
           m->ReadWString(iter, &r->contents) &&
           m->ReadWString(iter, &r->description) &&
           m->ReadBool(iter, &r->is_history_what_you_typed_match) &&
           m->ReadString(iter, &r->type) &&
           m->ReadBool(iter, &r->starred);
  }

  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"[%ls %d %ls %ls %d %ls %ls %ls %ls %ls %ls]",
        UTF8ToWide(p.provider_name),
        p.relevance,
        p.deletable ? L"true" : L"false",
        p.fill_into_edit,
        p.inline_autocomplete_offset,
        p.destination_url,
        p.contents,
        p.description,
        p.is_history_what_you_typed_match ? L"true" : L"false",
        UTF8ToWide(p.type),
        p.starred ? L"true" : L"false"));
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


