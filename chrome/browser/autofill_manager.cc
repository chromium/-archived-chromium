// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_manager.h"

#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/web_contents.h"

AutofillManager::~AutofillManager() {
  ClearPendingQuery();
}

void AutofillManager::ClearPendingQuery() {
  pending_query_name_.clear();
  pending_query_prefix_.clear();

  if (query_is_pending_) {
    WebDataService* web_data_service =
        profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
    if (!web_data_service) {
      NOTREACHED();
      return;
    }
    web_data_service->CancelRequest(pending_query_handle_);
  }
  pending_query_handle_ = 0;
  query_is_pending_ = false;
}

void AutofillManager::AutofillFormSubmitted(const AutofillForm& form) {
  StoreFormEntriesInWebDatabase(form);
}

void AutofillManager::FetchValuesForName(const std::wstring& name,
                                         const std::wstring& prefix,
                                         int limit) {
  WebDataService* web_data_service =
      profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }

  ClearPendingQuery();

  pending_query_handle_ = web_data_service->
      GetFormValuesForElementName(name, prefix, limit, this);
  pending_query_name_ = name;
  pending_query_prefix_ = prefix;
}

void AutofillManager::OnWebDataServiceRequestDone(WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK(query_is_pending_);

  DCHECK(result);
  if (!result)
    return;

  switch (result->GetType()) {
    case AUTOFILL_VALUE_RESULT: {
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  ClearPendingQuery();
}

void AutofillManager::StoreFormEntriesInWebDatabase(
    const AutofillForm& form) {
  if (profile()->IsOffTheRecord())
    return;

  profile()->GetWebDataService(Profile::EXPLICIT_ACCESS)->
      AddAutofillFormElements(form.elements);
}
