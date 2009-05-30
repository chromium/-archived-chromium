// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_default.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"

#include "base/logging.h"
#include "base/task.h"

PasswordStoreDefault::PasswordStoreDefault(WebDataService* web_data_service)
    : web_data_service_(web_data_service) {
}

void PasswordStoreDefault::AddLoginImpl(const PasswordForm& form) {
  web_data_service_->AddLogin(form);
}

void PasswordStoreDefault::RemoveLoginImpl(const PasswordForm& form) {
  web_data_service_->RemoveLogin(form);
}

void PasswordStoreDefault::UpdateLoginImpl(const PasswordForm& form) {
  web_data_service_->UpdateLogin(form);
}

void PasswordStoreDefault::GetLoginsImpl(GetLoginsRequest* request) {
  int web_data_handle = web_data_service_->GetLogins(request->form, this);
  AddPendingWebDataServiceRequest(web_data_handle, request);
}

void PasswordStoreDefault::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult *result) {
  // Look up this handle in our request map to get the original
  // GetLoginsRequest.
  PendingRequestMap::iterator it(pending_requests_.find(h));
  DCHECK(it != pending_requests_.end());

  GetLoginsRequest* request = it->second;
  pending_requests_.erase(it);

  DCHECK(result);
  if (!result)
    return;

  const WDResult<std::vector<PasswordForm*> >* r =
      static_cast<const WDResult<std::vector<PasswordForm*> >*>(result);

  NotifyConsumer(request, r->GetValue());

  RemovePendingWebDataServiceRequest(h);
}

void PasswordStoreDefault::AddPendingWebDataServiceRequest(
    WebDataService::Handle handle, GetLoginsRequest* request) {
  pending_requests_.insert(PendingRequestMap::value_type(handle, request));

  // WebDataService callbacks are non-retaining. Since there would be a race
  // around cancelling the requests in the desctructor vs. getting a callback
  // in this worker thread, just make sure that we stick around instead.
  this->AddRef();
}

void PasswordStoreDefault::RemovePendingWebDataServiceRequest(
    WebDataService::Handle handle) {
  PendingRequestMap::iterator it(pending_requests_.find(handle));
  DCHECK(it != pending_requests_.end());
  pending_requests_.erase(it);

  // Balance the AddRef in AddPendingWebDataServiceRequest.
  this->Release();
}

PasswordStore::GetLoginsRequest*
    PasswordStoreDefault::GetLoginsRequestForWebDataServiceRequest(
        WebDataService::Handle handle) {
  PendingRequestMap::iterator it(pending_requests_.find(handle));
  if (it == pending_requests_.end())
    return NULL;
  return it->second;
}
