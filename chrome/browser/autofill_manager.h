// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_MANAGER_H_

#include <string>

#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/pref_member.h"

namespace webkit_glue {
class AutofillForm;
}

class Profile;
class TabContents;

// Per-tab autofill manager. Handles receiving form data from the renderer and
// the storing and retrieving of form data through WebDataService.
class AutofillManager : public RenderViewHostDelegate::Autofill,
                        public WebDataServiceConsumer {
 public:
  explicit AutofillManager(TabContents* tab_contents);
  virtual ~AutofillManager();

  void CancelPendingQuery();

  Profile* profile();

  // RenderViewHostDelegate::Autofill implementation.
  virtual void AutofillFormSubmitted(const webkit_glue::AutofillForm& form);
  virtual void GetAutofillSuggestions(const std::wstring& name,
                                      const std::wstring& prefix,
                                      int64 node_id,
                                      int request_id);
  virtual void RemoveAutofillEntry(const std::wstring& name,
                                   const std::wstring& value);

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  static void RegisterUserPrefs(PrefService* prefs);

 private:
  void StoreFormEntriesInWebDatabase(const webkit_glue::AutofillForm& form);

  TabContents* tab_contents_;

  BooleanPrefMember form_autofill_enabled_;

  // When the manager makes a request from WebDataService, the database
  // is queried on another thread, we record the query handle until we
  // get called back.
  WebDataService::Handle pending_query_handle_;
  int64 node_id_;
  int request_id_;

  DISALLOW_COPY_AND_ASSIGN(AutofillManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_MANAGER_H_
