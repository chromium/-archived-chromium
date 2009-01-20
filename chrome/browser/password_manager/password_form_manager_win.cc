// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_form_manager.h"

#include "base/string_util.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profile.h"

void PasswordFormManager::FetchMatchingIE7LoginFromWebDatabase() {
  DCHECK_EQ(state_, PRE_MATCHING_PHASE);
  DCHECK(!pending_login_query_);
  state_ = MATCHING_PHASE;
  WebDataService* web_data_service =
      profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
   return;
  }

  IE7PasswordInfo info;
  std::wstring url = ASCIIToWide(observed_form_.origin.spec());
  info.url_hash = ie7_password::GetUrlHash(url);
  pending_login_query_ = web_data_service->GetIE7Login(info, this);
}

void PasswordFormManager::OnIE7RequestDone(WebDataService::Handle h,
    const WDTypedResult* result) {
  // Get the result from the database into a usable form.
  const WDResult<IE7PasswordInfo>* r =
      static_cast<const WDResult<IE7PasswordInfo>*>(result);
  IE7PasswordInfo result_value = r->GetValue();

  state_ = POST_MATCHING_PHASE;

  if (!result_value.encrypted_data.empty()) {
    // We got a result.
    // Delete the entry. If it's good we will add it to the real saved password
    // table.
    WebDataService* web_data_service =
        profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
    if (!web_data_service) {
      NOTREACHED();
     return;
    }
    web_data_service->RemoveIE7Login(result_value);

    std::wstring username;
    std::wstring password;
    std::wstring url = ASCIIToWide(observed_form_.origin.spec());
    if (!ie7_password::DecryptPassword(url, result_value.encrypted_data,
                                       &username, &password)) {
      return;
    }

    PasswordForm* auto_fill = new PasswordForm(observed_form_);
    auto_fill->username_value = username;
    auto_fill->password_value = password;
    auto_fill->preferred = true;
    auto_fill->ssl_valid = observed_form_.origin.SchemeIsSecure();
    auto_fill->date_created = result_value.date_created;
    // Add this PasswordForm to the saved password table.
    web_data_service->AddLogin(*auto_fill);

    if (IgnoreResult(*auto_fill)) {
      delete auto_fill;
      return;
    }

    best_matches_[auto_fill->username_value] = auto_fill;
    preferred_match_ = auto_fill;
    password_manager_->Autofill(observed_form_, best_matches_,
                                preferred_match_);
  }
}
