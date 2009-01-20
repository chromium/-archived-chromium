// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service.h"

#include "chrome/browser/password_manager/ie7_password.h"

void WebDataService::AddIE7Login(const IE7PasswordInfo& info) {
  GenericRequest<IE7PasswordInfo>* request =
      new GenericRequest<IE7PasswordInfo>(this, GetNextRequestHandle(), NULL,
                                          info);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this, &WebDataService::AddIE7LoginImpl,
                                 request));
}

void WebDataService::RemoveIE7Login(const IE7PasswordInfo& info) {
  GenericRequest<IE7PasswordInfo>* request =
      new GenericRequest<IE7PasswordInfo>(this, GetNextRequestHandle(), NULL,
                                          info);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this, &WebDataService::RemoveIE7LoginImpl,
                                 request));
}

WebDataService::Handle WebDataService::GetIE7Login(
    const IE7PasswordInfo& info,
    WebDataServiceConsumer* consumer) {
  GenericRequest<IE7PasswordInfo>* request =
      new GenericRequest<IE7PasswordInfo>(this, GetNextRequestHandle(),
                                          consumer, info);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this, &WebDataService::GetIE7LoginImpl,
                                 request));
  return request->GetHandle();
}

void WebDataService::AddIE7LoginImpl(GenericRequest<IE7PasswordInfo>* request) {
  if (db_ && !request->IsCancelled()) {
    if (db_->AddIE7Login(request->GetArgument()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveIE7LoginImpl(
    GenericRequest<IE7PasswordInfo>* request) {
  if (db_ && !request->IsCancelled()) {
    if (db_->RemoveIE7Login(request->GetArgument()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::GetIE7LoginImpl(
    GenericRequest<IE7PasswordInfo>* request) {
  if (db_ && !request->IsCancelled()) {
    IE7PasswordInfo result;
    db_->GetIE7Login(request->GetArgument(), &result);
    request->SetResult(
        new WDResult<IE7PasswordInfo>(PASSWORD_IE7_RESULT, result));
  }
  request->RequestComplete();
}
