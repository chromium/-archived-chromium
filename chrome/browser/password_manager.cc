// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager.h"

#include "base/string_util.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/stl_util-inl.h"

#include "chromium_strings.h"
#include "generated_resources.h"

// static
void PasswordManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kPasswordManagerEnabled, true);
}

PasswordManager::PasswordManager(WebContents* web_contents)
    : ConfirmInfoBarDelegate(web_contents),
      web_contents_(web_contents),
      observer_(NULL),
      login_managers_deleter_(&pending_login_managers_) {
  password_manager_enabled_.Init(prefs::kPasswordManagerEnabled,
      web_contents->profile()->GetPrefs(), NULL);
}

PasswordManager::~PasswordManager() {
  // Remove any InfoBars we may be showing.
  web_contents_->RemoveInfoBar(this);
}

void PasswordManager::ProvisionallySavePassword(PasswordForm form) {
  if (!web_contents_->controller() || !web_contents_->profile() ||
      web_contents_->profile()->IsOffTheRecord() || !*password_manager_enabled_)
    return;

  // No password to save? Then don't.
  if (form.password_value.empty())
    return;

  LoginManagers::iterator iter;
  PasswordFormManager* manager = NULL;
  for (iter = pending_login_managers_.begin();
       iter != pending_login_managers_.end(); iter++) {
    if ((*iter)->DoesManage(form)) {
      manager = *iter;
      break;
    }
  }
  // If we didn't find a manager, this means a form was submitted without
  // first loading the page containing the form. Don't offer to save
  // passwords in this case.
  if (!manager)
    return;

  // If we found a manager but it didn't finish matching yet, the user has
  // tried to submit credentials before we had time to even find matching
  // results for the given form and autofill. If this is the case, we just
  // give up.
  if (!manager->HasCompletedMatching())
    return;

  // Also get out of here if the user told us to 'never remember' passwords for
  // this form.
  if (manager->IsBlacklisted())
    return;

  form.ssl_valid = form.origin.SchemeIsSecure() &&
      !web_contents_->controller()->ssl_manager()->
          ProcessedSSLErrorFromRequest();
  form.preferred = true;
  manager->ProvisionallySave(form);
  provisional_save_manager_.reset(manager);
  pending_login_managers_.erase(iter);
  // We don't care about the rest of the forms on the page now that one
  // was selected.
  STLDeleteElements(&pending_login_managers_);
}

void PasswordManager::DidNavigate() {
  // As long as this navigation isn't due to a currently pending
  // password form submit, we're ready to reset and move on.
  if (!provisional_save_manager_.get() && !pending_login_managers_.empty())
    STLDeleteElements(&pending_login_managers_);
}

void PasswordManager::ClearProvisionalSave() {
  provisional_save_manager_.reset();
}

void PasswordManager::DidStopLoading() {
  if (!provisional_save_manager_.get())
    return;

  DCHECK(!web_contents_->profile()->IsOffTheRecord());
  DCHECK(!provisional_save_manager_->IsBlacklisted());

  if (!web_contents_->profile() ||
      !web_contents_->profile()->GetWebDataService(Profile::IMPLICIT_ACCESS))
    return;
  if (!web_contents_->controller())
    return;

  if (provisional_save_manager_->IsNewLogin()) {
    web_contents_->AddInfoBar(this);
    pending_decision_manager_.reset(provisional_save_manager_.release());
  } else {
    // If the save is not a new username entry, then we just want to save this
    // data (since the user already has related data saved), so don't prompt.
    provisional_save_manager_->Save();
    provisional_save_manager_.reset();
  }
}

void PasswordManager::PasswordFormsSeen(const std::vector<PasswordForm>& forms) {
  if (!web_contents_->profile() ||
      !web_contents_->profile()->GetWebDataService(Profile::EXPLICIT_ACCESS))
    return;
  if (!web_contents_->controller())
    return;
  if (!*password_manager_enabled_)
    return;

  // Ask the SSLManager for current security.
  bool had_ssl_error = web_contents_->controller()->ssl_manager()->
      ProcessedSSLErrorFromRequest();

  std::vector<PasswordForm>::const_iterator iter;
  for (iter = forms.begin(); iter != forms.end(); iter++) {
    if (provisional_save_manager_.get() &&
        provisional_save_manager_->DoesManage(*iter)) {
      // The form trying to be saved has immediately re-appeared. Assume
      // login failure and abort this save.  Fallback to pending login state
      // since the user may try again.
      pending_login_managers_.push_back(provisional_save_manager_.release());
      // Don't delete the login managers since the user may try again
      // and we want to be able to save in that case.
      break;
    } else {
      bool ssl_valid = iter->origin.SchemeIsSecure() && !had_ssl_error;
      PasswordFormManager* manager =
          new PasswordFormManager(web_contents_->profile(),
                                  this, *iter, ssl_valid);
      pending_login_managers_.push_back(manager);
      manager->FetchMatchingLoginsFromWebDatabase();
    }
  }
}

void PasswordManager::Autofill(const PasswordForm& form_for_autofill,
                               const PasswordFormMap& best_matches,
                               const PasswordForm* const preferred_match) const {
  DCHECK(web_contents_);
  DCHECK(preferred_match);
  switch (form_for_autofill.scheme) {
    case PasswordForm::SCHEME_HTML: {
      // Note the check above is required because the observer_ for a non-HTML
      // schemed password form may have been freed, so we need to distinguish.
      bool action_mismatch = form_for_autofill.action.GetWithEmptyPath() !=
                             preferred_match->action.GetWithEmptyPath();
      scoped_ptr<PasswordFormDomManager::FillData> fill_data(
          PasswordFormDomManager::CreateFillData(form_for_autofill,
                                                 best_matches, preferred_match,
                                                 action_mismatch));
      web_contents_->render_view_host()->FillPasswordForm(*fill_data);
      return;
    }
    default:
      if (observer_)
        observer_->OnAutofillDataAvailable(preferred_match->username_value,
                                           preferred_match->password_value);
  }
}

// PasswordManager, ConfirmInfoBarDelegate implementation: ---------------------

void PasswordManager::InfoBarClosed() {
  pending_decision_manager_.reset(NULL);
}

std::wstring PasswordManager::GetMessageText() const {
  return l10n_util::GetString(IDS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT);
}

SkBitmap* PasswordManager::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_SAVE_PASSWORD);
}

int PasswordManager::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::wstring PasswordManager::GetButtonLabel(InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetString(IDS_PASSWORD_MANAGER_SAVE_BUTTON);
  if (button == BUTTON_CANCEL)
    return l10n_util::GetString(IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON);
  NOTREACHED();
  return std::wstring();
}

bool PasswordManager::Accept() {
  pending_decision_manager_->Save();
  return true;
}

bool PasswordManager::Cancel() {
  pending_decision_manager_->PermanentlyBlacklist();
  return true;
}

