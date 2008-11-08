// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_MANAGER_H_

#include <map>
#include <string>

#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/web_contents.h"
#include "webkit/glue/autofill_form.h"

class Profile;

// Per-tab autofill manager. Handles receiving form data from the renderer and
// the storing and retrieving of form data through WebDataService.
class AutofillManager : public WebDataServiceConsumer {
 public:
  explicit AutofillManager(WebContents* web_contents)
      : query_is_pending_(false), web_contents_(web_contents) {}
  virtual ~AutofillManager();

  void ClearPendingQuery();

  Profile* profile() { return web_contents_->profile(); }

  // Called when a form is submitted (i.e. when the user hits the submit button)
  // to store the form entries in the profile's sql database.
  void AutofillFormSubmitted(const AutofillForm& form);

  // Starts a query into the database for the values corresponding to name.
  // OnWebDataServiceRequestDone gets called when the query completes.
  void FetchValuesForName(const std::wstring& name, const std::wstring& prefix,
      int limit);

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

 private:
  void StoreFormEntriesInWebDatabase(const AutofillForm& form);

  WebContents* web_contents_;
  
  // When the manager makes a request from WebDataService, the database
  // is queried on another thread, we record the query in this map until we
  // get called back.
  bool query_is_pending_;
  WebDataService::Handle pending_query_handle_;
  std::wstring pending_query_name_;
  std::wstring pending_query_prefix_;

  DISALLOW_COPY_AND_ASSIGN(AutofillManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_MANAGER_H_
