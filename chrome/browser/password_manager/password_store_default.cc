// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_default.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"

#include "base/logging.h"
#include "base/task.h"

using webkit_glue::PasswordForm;

PasswordStoreDefault::PasswordStoreDefault(WebDataService* web_data_service)
    : web_data_service_(web_data_service) {
}

PasswordStoreDefault::~PasswordStoreDefault() {
  for (PendingRequestMap::const_iterator it = pending_requests_.begin();
       it != pending_requests_.end(); ++it) {
    web_data_service_->CancelRequest(it->first);
  }
}

// Override all the public methods to do avoid passthroughs to the Impl
// versions. Since we are calling through to WebDataService, which is
// asynchronous, we'll still behave as the caller expects.
void PasswordStoreDefault::AddLogin(const PasswordForm& form) {
  web_data_service_->AddLogin(form);
}

void PasswordStoreDefault::UpdateLogin(const PasswordForm& form) {
  web_data_service_->UpdateLogin(form);
}

void PasswordStoreDefault::RemoveLogin(const PasswordForm& form) {
  web_data_service_->RemoveLogin(form);
}

int PasswordStoreDefault::GetLogins(const PasswordForm& form,
                                    PasswordStoreConsumer* consumer) {
  int handle = handle_++;
  GetLoginsRequest* request = new GetLoginsRequest(form, consumer, handle);

  int web_data_handle = web_data_service_->GetLogins(form, this);
  pending_requests_.insert(PendingRequestMap::value_type(web_data_handle,
                                                         request));
  return handle;
}

void PasswordStoreDefault::CancelLoginsQuery(int handle) {
  for (PendingRequestMap::iterator it = pending_requests_.begin();
       it != pending_requests_.end(); ++it) {
    GetLoginsRequest* request = it->second;
    if (request->handle == handle) {
      web_data_service_->CancelRequest(it->first);
      delete request;
      pending_requests_.erase(it);
      return;
    }
  }
}

void PasswordStoreDefault::AddLoginImpl(const PasswordForm& form) {
  NOTREACHED();
}

void PasswordStoreDefault::RemoveLoginImpl(const PasswordForm& form) {
  NOTREACHED();
}

void PasswordStoreDefault::UpdateLoginImpl(const PasswordForm& form) {
  NOTREACHED();
}

void PasswordStoreDefault::GetLoginsImpl(GetLoginsRequest* request) {
  NOTREACHED();
}

void PasswordStoreDefault::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult *result) {
  // Look up this handle in our request map to get the original
  // GetLoginsRequest.
  PendingRequestMap::iterator it(pending_requests_.find(h));
  // If the request was cancelled, we are done.
  if (it == pending_requests_.end())
    return;

  scoped_ptr<GetLoginsRequest> request(it->second);
  pending_requests_.erase(it);

  DCHECK(result);
  if (!result)
    return;

  const WDResult<std::vector<PasswordForm*> >* r =
      static_cast<const WDResult<std::vector<PasswordForm*> >*>(result);

  request->consumer->OnPasswordStoreRequestDone(request->handle,
                                                r->GetValue());
}
