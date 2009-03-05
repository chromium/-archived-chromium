// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager.h"

#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/stl_util-inl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

// After a successful *new* login attempt, we take the PasswordFormManager in
// provisional_save_manager_ and move it to a SavePasswordInfoBarDelegate while
// the user makes up their mind with the "save password" infobar. Note if the
// login is one we already know about, the end of the line is
// provisional_save_manager_ because we just update it on success and so such
// forms never end up in an infobar.
class SavePasswordInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SavePasswordInfoBarDelegate(TabContents* tab_contents,
                              PasswordFormManager* form_to_save) :
      ConfirmInfoBarDelegate(tab_contents),
      form_to_save_(form_to_save) {
  }

   virtual ~SavePasswordInfoBarDelegate() { }

  // Overridden from ConfirmInfoBarDelegate:
  virtual void InfoBarClosed() {
    delete this;
  }

  virtual std::wstring GetMessageText() const {
    return l10n_util::GetString(IDS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT);
  }

  virtual SkBitmap* GetIcon() const {
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_INFOBAR_SAVE_PASSWORD);
  }

  virtual int GetButtons() const {
    return BUTTON_OK | BUTTON_CANCEL;
  }

  virtual std::wstring GetButtonLabel(InfoBarButton button) const {
    if (button == BUTTON_OK)
      return l10n_util::GetString(IDS_PASSWORD_MANAGER_SAVE_BUTTON);
    if (button == BUTTON_CANCEL)
      return l10n_util::GetString(IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON);
    NOTREACHED();
    return std::wstring();
  }

  virtual bool Accept() {
    DCHECK(form_to_save_.get());
    form_to_save_->Save();
    return true;
  }

  virtual bool Cancel() {
    DCHECK(form_to_save_.get());
    form_to_save_->PermanentlyBlacklist();
    return true;
  }

 private:
  // The PasswordFormManager managing the form we're asking the user about,
  // and should update as per her decision.
  scoped_ptr<PasswordFormManager> form_to_save_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordInfoBarDelegate);
};

// static
void PasswordManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kPasswordManagerEnabled, true);
}

PasswordManager::PasswordManager(WebContents* web_contents)
    : login_managers_deleter_(&pending_login_managers_),
      web_contents_(web_contents),
      observer_(NULL) {
  password_manager_enabled_.Init(prefs::kPasswordManagerEnabled,
      web_contents->profile()->GetPrefs(), NULL);
}

PasswordManager::~PasswordManager() {
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
    web_contents_->AddInfoBar(
        new SavePasswordInfoBarDelegate(web_contents_,
                                        provisional_save_manager_.release()));
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
      PasswordFormDomManager::FillData fill_data;
      PasswordFormDomManager::InitFillData(form_for_autofill,
                                           best_matches, preferred_match,
                                           action_mismatch,
                                           &fill_data);
      web_contents_->render_view_host()->FillPasswordForm(fill_data);
      return;
    }
    default:
      if (observer_)
        observer_->OnAutofillDataAvailable(preferred_match->username_value,
                                           preferred_match->password_value);
  }
}

