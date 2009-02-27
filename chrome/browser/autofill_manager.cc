// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_manager.h"

#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "webkit/glue/autofill_form.h"

// static
void AutofillManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kFormAutofillEnabled, true);
}

AutofillManager::AutofillManager(WebContents* web_contents)
    : web_contents_(web_contents),
      pending_query_handle_(0),
      node_id_(0),
      request_id_(0) {
  form_autofill_enabled_.Init(prefs::kFormAutofillEnabled,
      profile()->GetPrefs(), NULL);
}

AutofillManager::~AutofillManager() {
  CancelPendingQuery();
}

void AutofillManager::CancelPendingQuery() {
  if (pending_query_handle_) {
    WebDataService* web_data_service =
        profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
    if (!web_data_service) {
      NOTREACHED();
      return;
    }
    web_data_service->CancelRequest(pending_query_handle_);
  }
  pending_query_handle_ = 0;
}

Profile* AutofillManager::profile() {
  return web_contents_->profile();
}

void AutofillManager::AutofillFormSubmitted(const AutofillForm& form) {
  StoreFormEntriesInWebDatabase(form);
}

void AutofillManager::FetchValuesForName(const std::wstring& name,
                                         const std::wstring& prefix,
                                         int limit,
                                         int64 node_id,
                                         int request_id) {
  if (!*form_autofill_enabled_)
    return;

  WebDataService* web_data_service =
      profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery();

  node_id_ = node_id;
  request_id_ = request_id;

  pending_query_handle_ = web_data_service->
      GetFormValuesForElementName(name, prefix, limit, this);
}

void AutofillManager::OnWebDataServiceRequestDone(WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK(pending_query_handle_);
  pending_query_handle_ = 0;

  if (!*form_autofill_enabled_)
    return;

  DCHECK(result);
  if (!result)
    return;

  switch (result->GetType()) {
    case AUTOFILL_VALUE_RESULT: {
      RenderViewHost* host = web_contents_->render_view_host();
      if (!host)
        return;
      const WDResult<std::vector<std::wstring> >* r =
          static_cast<const WDResult<std::vector<std::wstring> >*>(result);
      std::vector<std::wstring> suggestions = r->GetValue();
      host->AutofillSuggestionsReturned(suggestions, node_id_, request_id_, -1);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void AutofillManager::StoreFormEntriesInWebDatabase(
    const AutofillForm& form) {
  if (!*form_autofill_enabled_)
    return;

  if (profile()->IsOffTheRecord())
    return;

  profile()->GetWebDataService(Profile::EXPLICIT_ACCESS)->
      AddAutofillFormElements(form.elements);
}
