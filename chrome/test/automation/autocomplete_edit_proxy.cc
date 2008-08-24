// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/autocomplete_edit_proxy.h"

#include <vector>

#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"

bool AutocompleteEditProxy::GetText(std::wstring* text) const {
  if (!is_valid())
    return false;
  if (!text) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  if (!sender_->SendAndWaitForResponse(
      new AutomationMsg_AutocompleteEditGetTextRequest(0, handle_), &response,
      AutomationMsg_AutocompleteEditGetTextResponse::ID))
    return false;
  scoped_ptr<IPC::Message> response_deleter(response);

  Tuple2<bool, std::wstring> returned_result;
  if (!AutomationMsg_AutocompleteEditGetTextResponse::Read(response,
      &returned_result) || !returned_result.a)
    return false;

  text->swap(returned_result.b);
  return true;
}

bool AutocompleteEditProxy::SetText(const std::wstring& text) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  if (!sender_->SendAndWaitForResponse(
      new AutomationMsg_AutocompleteEditSetTextRequest(0, handle_, text),
      &response, AutomationMsg_AutocompleteEditSetTextResponse::ID))
    return false;

  delete response;
  return true;
}

bool AutocompleteEditProxy::IsQueryInProgress(bool* query_in_progress) const {
  if (!is_valid())
    return false;
  if (!query_in_progress) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  if (!sender_->SendAndWaitForResponse(
      new AutomationMsg_AutocompleteEditIsQueryInProgressRequest(0, handle_),
      &response, AutomationMsg_AutocompleteEditIsQueryInProgressResponse::ID))
    return false;
  scoped_ptr<IPC::Message> response_deleter(response);

  Tuple2<bool, bool> returned_result;
  if (!AutomationMsg_AutocompleteEditIsQueryInProgressResponse::Read(
      response, &returned_result) || !returned_result.a)
    return false;

  *query_in_progress = returned_result.b;
  return true;
}


bool AutocompleteEditProxy::WaitForQuery(int wait_timeout_ms) const {
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout_ms);
  bool query_in_progress;
  while (TimeTicks::Now() - start < timeout) {
    if (IsQueryInProgress(&query_in_progress) && !query_in_progress)
      return true;
    Sleep(automation::kSleepTime);
  }
  // If we get here the query is still in progress.
  return false;
}

bool AutocompleteEditProxy::GetAutocompleteMatches(Matches* matches) const {
  if (!is_valid())
    return false;
  if (!matches) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  if (!sender_->SendAndWaitForResponse(
      new AutomationMsg_AutocompleteEditGetMatchesRequest(0, handle_),
      &response, AutomationMsg_AutocompleteEditGetMatchesResponse::ID))
    return false;
  scoped_ptr<IPC::Message> response_deleter(response);

  Tuple2<bool, Matches> returned_result;
  if (!AutomationMsg_AutocompleteEditGetMatchesResponse::Read(
      response, &returned_result) || !returned_result.a)
    return false;

  *matches = returned_result.b;
  return true;
}

