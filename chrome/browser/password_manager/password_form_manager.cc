// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_form_manager.h"

#include <algorithm>

#if defined(OS_POSIX)
// TODO(port): remove these when supporting classes are ported
#include "chrome/common/temp_scaffolding_stubs.h"
#elif defined(OS_WIN)
#include "chrome/browser/password_manager/password_manager.h"
#endif

#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "webkit/glue/password_form_dom_manager.h"

using base::Time;

PasswordFormManager::PasswordFormManager(Profile* profile,
                                         PasswordManager* password_manager,
                                         const PasswordForm& observed_form,
                                         bool ssl_valid)
    : best_matches_deleter_(&best_matches_),
      observed_form_(observed_form),
      is_new_login_(true),
      password_manager_(password_manager),
      pending_login_query_(NULL),
      preferred_match_(NULL),
      state_(PRE_MATCHING_PHASE),
      profile_(profile) {
  DCHECK(profile_);
  if (observed_form_.origin.is_valid())
    SplitString(observed_form_.origin.path(), '/', &form_path_tokens_);
  observed_form_.ssl_valid = ssl_valid;
}

PasswordFormManager::~PasswordFormManager() {
  CancelLoginsQuery();
}

// TODO(timsteele): use a hash of some sort in the future?
bool PasswordFormManager::DoesManage(const PasswordForm& form) const {
  if (form.scheme != PasswordForm::SCHEME_HTML)
      return observed_form_.signon_realm == form.signon_realm;

  // HTML form case.
  // At a minimum, username and password element must match.
  if (!((form.username_element == observed_form_.username_element) &&
        (form.password_element == observed_form_.password_element))) {
    return false;
  }

  // The action URL must also match, but the form is allowed to have an empty
  // action URL (See bug 1107719).
  if (form.action.is_valid() && (form.action != observed_form_.action))
    return false;

  // If this is a replay of the same form in the case a user entered an invalid
  // password, the origin of the new form may equal the action of the "first"
  // form.
  if (!((form.origin == observed_form_.origin) ||
        (form.origin == observed_form_.action))) {
    if (form.origin.SchemeIsSecure() &&
        !observed_form_.origin.SchemeIsSecure()) {
      // Compare origins, ignoring scheme. There is no easy way to do this
      // with GURL because clearing the scheme would result in an invalid url.
      // This is for some sites (such as Hotmail) that begin on an http page and
      // head to https for the retry when password was invalid.
      std::string::const_iterator after_scheme1 = form.origin.spec().begin() +
                                                  form.origin.scheme().length();
      std::string::const_iterator after_scheme2 =
          observed_form_.origin.spec().begin() +
          observed_form_.origin.scheme().length();
      return std::search(after_scheme1,
                         form.origin.spec().end(),
                         after_scheme2,
                         observed_form_.origin.spec().end())
                         != form.origin.spec().end();
    }
    return false;
  }
  return true;
}

bool PasswordFormManager::IsBlacklisted() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  if (preferred_match_ && preferred_match_->blacklisted_by_user)
    return true;
  return false;
}

void PasswordFormManager::PermanentlyBlacklist() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);

  // Configure the form about to be saved for blacklist status.
  pending_credentials_.preferred = true;
  pending_credentials_.blacklisted_by_user = true;
  pending_credentials_.username_value.clear();
  pending_credentials_.password_value.clear();

  // Retroactively forget existing matches for this form, so we NEVER prompt or
  // autofill it again.
  if (!best_matches_.empty()) {
    PasswordFormMap::const_iterator iter;
    WebDataService* web_data_service =
        profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
    if (!web_data_service) {
      NOTREACHED();
      return;
    }
    for (iter = best_matches_.begin(); iter != best_matches_.end(); ++iter) {
      // We want to remove existing matches for this form so that the exact
      // origin match with |blackisted_by_user == true| is the only result that
      // shows up in the future for this origin URL. However, we don't want to
      // delete logins that were actually saved on a different page (hence with
      // different origin URL) and just happened to match this form because of
      // the scoring algorithm. See bug 1204493.
      if (iter->second->origin == observed_form_.origin)
        web_data_service->RemoveLogin(*iter->second);
    }
  }

  // Save the pending_credentials_ entry marked as blacklisted.
  SaveAsNewLogin();
}

bool PasswordFormManager::IsNewLogin() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  return is_new_login_;
}

void PasswordFormManager::ProvisionallySave(const PasswordForm& credentials) {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(DoesManage(credentials));

  // Make sure the important fields stay the same as the initially observed or
  // autofilled ones, as they may have changed if the user experienced a login
  // failure.
  // Look for these credentials in the list containing auto-fill entries.
  PasswordFormMap::const_iterator it =
      best_matches_.find(credentials.username_value);
  if (it != best_matches_.end()) {
    // The user signed in with a login we autofilled.
    pending_credentials_ = *it->second;
    is_new_login_ = false;
    // If the user selected credentials we autofilled from a PasswordForm
    // that contained no action URL (IE6/7 imported passwords, for example),
    // bless it with the action URL from the observed form. See bug 1107719.
    if (pending_credentials_.action.is_empty())
      pending_credentials_.action = observed_form_.action;
  } else {
    pending_credentials_ = observed_form_;
    pending_credentials_.username_value = credentials.username_value;
  }

  pending_credentials_.password_value = credentials.password_value;
  pending_credentials_.preferred = credentials.preferred;
}

void PasswordFormManager::Save() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(!profile_->IsOffTheRecord());

  if (IsNewLogin())
    SaveAsNewLogin();
  else
    UpdateLogin();
}

void PasswordFormManager::FetchMatchingLoginsFromWebDatabase() {
  DCHECK_EQ(state_, PRE_MATCHING_PHASE);
  DCHECK(!pending_login_query_);
  state_ = MATCHING_PHASE;
  WebDataService* web_data_service =
      profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }
  pending_login_query_ = web_data_service->GetLogins(observed_form_, this);
}

bool PasswordFormManager::HasCompletedMatching() {
  return state_ == POST_MATCHING_PHASE;
}

void PasswordFormManager::OnRequestDone(WebDataService::Handle h,
    const WDTypedResult* result) {
  // Get the result from the database into a usable form.
  const WDResult<std::vector<PasswordForm*> >* r =
      static_cast<const WDResult<std::vector<PasswordForm*> >*>(result);
  std::vector<PasswordForm*> logins_result = r->GetValue();
  // Note that the result gets deleted after this call completes, but we own
  // the PasswordForm objects pointed to by the result vector, thus we keep
  // copies to a minimum here.

  int best_score = 0;
  std::vector<PasswordForm> empties; // Empty-path matches in result set.
  for (size_t i = 0; i < logins_result.size(); i++) {
    if (IgnoreResult(*logins_result[i])) {
      delete logins_result[i];
      continue;
    }
    // Score and update best matches.
    int current_score = ScoreResult(*logins_result[i]);
    // This check is here so we can append empty path matches in the event
    // they don't score as high as others and aren't added to best_matches_.
    // This is most commonly imported firefox logins. We skip blacklisted
    // ones because clearly we don't want to autofill them, and secondly
    // because they only mean something when we have no other matches already
    // saved in Chrome - in which case they'll make it through the regular
    // scoring flow below by design. Note signon_realm == origin implies empty
    // path logins_result, since signon_realm is a prefix of origin for HTML
    // password forms.
    // TODO(timsteele): Bug 1269400. We probably should do something more
    // elegant for any shorter-path match instead of explicitly handling empty
    // path matches.
    if ((observed_form_.scheme == PasswordForm::SCHEME_HTML) &&
        (observed_form_.signon_realm == logins_result[i]->origin.spec()) &&
        (current_score > 0) && (!logins_result[i]->blacklisted_by_user)){
      empties.push_back(*logins_result[i]);
    }

    if (current_score < best_score) {
      delete logins_result[i];
      continue;
    }
    if (current_score == best_score) {
      best_matches_[logins_result[i]->username_value] = logins_result[i];
    } else if (current_score > best_score) {
      best_score = current_score;
      // This new login has a better score than all those up to this point
      // Note 'this' owns all the PasswordForms in best_matches_.
      STLDeleteValues(&best_matches_);
      best_matches_.clear();
      preferred_match_ = NULL;  // Don't delete, its owned by best_matches_.
      best_matches_[logins_result[i]->username_value] = logins_result[i];
    }
    preferred_match_ = logins_result[i]->preferred ? logins_result[i]
                                                   : preferred_match_;
  }
  // We're done matching now.
  state_ = POST_MATCHING_PHASE;

  if (best_score <= 0) {
#if defined(OS_WIN)
    state_ = PRE_MATCHING_PHASE;
    FetchMatchingIE7LoginFromWebDatabase();
#endif
    return;
  }

  for (std::vector<PasswordForm>::const_iterator it = empties.begin();
       it != empties.end(); ++it) {
    // If we don't already have a result with the same username, add the
    // lower-scored empty-path match (if it had equal score it would already be
    // in best_matches_).
    if (best_matches_.find(it->username_value) == best_matches_.end())
      best_matches_[it->username_value] = new PasswordForm(*it);
  }

  // Its possible we have at least one match but have no preferred_match_,
  // because a user may have chosen to 'Forget' the preferred match. So we
  // just pick the first one and whichever the user selects for submit will
  // be saved as preferred.
  DCHECK(!best_matches_.empty());
  if (!preferred_match_)
    preferred_match_ = best_matches_.begin()->second;

  // Now we determine if the user told us to ignore this site in the past.
  // If they haven't, we proceed to auto-fill.
  if (!preferred_match_->blacklisted_by_user) {
    password_manager_->Autofill(observed_form_, best_matches_,
                                preferred_match_);
  }
}

void PasswordFormManager::OnWebDataServiceRequestDone(WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK_EQ(state_, MATCHING_PHASE);
  DCHECK_EQ(pending_login_query_, h);
  DCHECK(result);
  pending_login_query_ = NULL;

  if (!result)
    return;

  switch (result->GetType()) {
    case PASSWORD_RESULT: {
      OnRequestDone(h, result);
      break;
    }
#if defined(OS_WIN)
    case PASSWORD_IE7_RESULT: {
      OnIE7RequestDone(h, result);
      break;
    }
#endif
    default:
      NOTREACHED();
  }
}

bool PasswordFormManager::IgnoreResult(const PasswordForm& form) const {
  // Ignore change password forms until we have some change password
  // functionality
  if (observed_form_.old_password_element.length() != 0) {
    return true;
  }
  // Don't match an invalid SSL form with one saved under secure
  // circumstances.
  if (form.ssl_valid && !observed_form_.ssl_valid) {
    return true;
  }
  return false;
}

void PasswordFormManager::SaveAsNewLogin() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(IsNewLogin());
  // The new_form is being used to sign in, so it is preferred.
  DCHECK(pending_credentials_.preferred);
  // new_form contains the same basic data as observed_form_ (because its the
  // same form), but with the newly added credentials.

  DCHECK(!profile_->IsOffTheRecord());

  WebDataService* web_data_service =
      profile_->GetWebDataService(Profile::IMPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }
  pending_credentials_.date_created = Time::Now();
  web_data_service->AddLogin(pending_credentials_);
}

void PasswordFormManager::UpdateLogin() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(preferred_match_);
  // If we're doing an Update, its because we autofilled a form and the user
  // submitted it with a possibly new password value, page security, or selected
  // one of the non-preferred matches, thus requiring a swap of preferred bits.
  DCHECK(!IsNewLogin() && pending_credentials_.preferred);
  DCHECK(!profile_->IsOffTheRecord());

  WebDataService* web_data_service =
      profile_->GetWebDataService(Profile::IMPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }

  // Update all matches to reflect new preferred status.
  PasswordFormMap::iterator iter;
  for (iter = best_matches_.begin(); iter != best_matches_.end(); iter++) {
    if ((iter->second->username_value != pending_credentials_.username_value) &&
         iter->second->preferred) {
      // This wasn't the selected login but it used to be preferred.
      iter->second->preferred = false;
      web_data_service->UpdateLogin(*iter->second);
    }
  }
  // Update the new preferred login.
  // Note origin.spec().length > signon_realm.length implies the origin has a
  // path, since signon_realm is a prefix of origin for HTML password forms.
  if ((observed_form_.scheme == PasswordForm::SCHEME_HTML) &&
      (observed_form_.origin.spec().length() >
       observed_form_.signon_realm.length()) &&
      (observed_form_.signon_realm == pending_credentials_.origin.spec())) {
    // The user logged in successfully with one of our autofilled logins on a
    // page with non-empty path, but the autofilled entry was initially saved/
    // imported with an empty path. Rather than just mark this entry preferred,
    // we create a more specific copy for this exact page and leave the "master"
    // unchanged. This is to prevent the case where that master login is used
    // on several sites (e.g site.com/a and site.com/b) but the user actually
    // has a different preference on each site. For example, on /a, he wants the
    // general empty-path login so it is flagged as preferred, but on /b he logs
    // in with a different saved entry - we don't want to remove the preferred
    // status of the former because upon return to /a it won't be the default-
    // fill match.
    // TODO(timsteele): Bug 1188626 - expire the master copies.
    PasswordForm copy(pending_credentials_);
    copy.origin = observed_form_.origin;
    copy.action = observed_form_.action;
    web_data_service->AddLogin(copy);
  } else {
    web_data_service->UpdateLogin(pending_credentials_);
  }
}

void PasswordFormManager::CancelLoginsQuery() {
  if (!pending_login_query_)
    return;
  WebDataService* web_data_service =
      profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }
  web_data_service->CancelRequest(pending_login_query_);
  pending_login_query_ = NULL;
}

int PasswordFormManager::ScoreResult(const PasswordForm& candidate) const {
  DCHECK_EQ(state_, MATCHING_PHASE);
  // For scoring of candidate login data:
  // The most important element that should match is the origin, followed by
  // the action, the password name, the submit button name, and finally the
  // username input field name.
  // Exact origin match gives an addition of 32 (1 << 5) + # of matching url
  // dirs.
  // Partial match gives an addition of 16 (1 << 4) + # matching url dirs
  // That way, a partial match cannot trump an exact match even if
  // the partial one matches all other attributes (action, elements) (and
  // regardless of the matching depth in the URL path).
  int score = 0;
  if (candidate.origin == observed_form_.origin) {
    // This check is here for the most common case which
    // is we have a single match in the db for the given host,
    // so we don't generally need to walk the entire URL path (the else
    // clause).
    score += (1 << 5) + static_cast<int>(form_path_tokens_.size());
  } else {
    // Walk the origin URL paths one directory at a time to see how
    // deep the two match.
    std::vector<std::string> candidate_path_tokens;
    SplitString(candidate.origin.path(), '/', &candidate_path_tokens);
    size_t depth = 0;
    size_t max_dirs = std::min(form_path_tokens_.size(),
                               candidate_path_tokens.size());
    while ((depth < max_dirs) && (form_path_tokens_[depth] ==
                                  candidate_path_tokens[depth])) {
      depth++;
      score++;
    }
    // do we have a partial match?
    score += (depth > 0) ? 1 << 4 : 0;
  }
  if (observed_form_.scheme == PasswordForm::SCHEME_HTML) {
    if (candidate.action == observed_form_.action)
      score += 1 << 3;
    if (candidate.password_element == observed_form_.password_element)
      score += 1 << 2;
    if (candidate.submit_element == observed_form_.submit_element)
      score += 1 << 1;
    if (candidate.username_element == observed_form_.username_element)
      score += 1 << 0;
  }

  return score;
}

