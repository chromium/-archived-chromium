// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_H__
#define CHROME_BROWSER_PASSWORD_MANAGER_H__

#include "base/scoped_ptr.h"
#include "chrome/browser/infobar_delegate.h"
#include "chrome/browser/password_form_manager.h"
#include "chrome/browser/views/login_view.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/stl_util-inl.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/password_form_dom_manager.h"

class PrefService;
class WebContents;

// Per-tab password manager. Handles creation and management of UI elements,
// receiving password form data from the renderer and managing the password
// database through the WebDataService. The PasswordManager is a LoginModel
// for purposes of supporting HTTP authentication dialogs.
class PasswordManager : public views::LoginModel,
                        public ConfirmInfoBarDelegate {
 public:
  static void RegisterUserPrefs(PrefService* prefs);

  explicit PasswordManager(WebContents* web_contents);
  ~PasswordManager();

  // Called by a PasswordFormManager when it decides a form can be autofilled
  // on the page.
  void Autofill(const PasswordForm& form_for_autofill,
                const PasswordFormMap& best_matches,
                const PasswordForm* const preferred_match) const;

  // Notification that the user navigated away from the current page. 
  // Unless this is a password form submission, for our purposes this
  // means we're done with the current page, so we can clean-up.
  void DidNavigate();

  // Show a prompt to save submitted password if it is a new username for
  // the form, or else just update the stored value.
  void DidStopLoading();

  // Notifies the password manager that password forms were parsed on the page.
  void PasswordFormsSeen(const std::vector<PasswordForm>& forms);

  // When a form is submitted, we prepare to save the password but wait
  // until we decide the user has successfully logged in. This is step 1
  // of 2 (see SavePassword).
  void ProvisionallySavePassword(PasswordForm form);

  // Clear any pending saves
  void ClearProvisionalSave();

  // LoginModel implementation.
  virtual void SetObserver(views::LoginModelObserver* observer) {
    observer_ = observer;
  }

 private:
  // Overridden from ConfirmInfoBarDelegate:
  virtual void InfoBarClosed();
  virtual std::wstring GetMessageText() const;
  virtual SkBitmap* GetIcon() const;
  virtual int GetButtons() const;
  virtual std::wstring GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();
  
  // Note about how a PasswordFormManager can transition from
  // pending_login_managers_ to {provisional_save, pending_decision_}manager_.
  // 
  // 1. form "seen"
  //       |                                             new
  //       |                                               ___ pending_decision
  // pending_login -- form submit --> provisional_save ___/
  //             ^                            |           \___ (update DB)
  //             |                           fail
  //             |-----------<------<---------|          !new
  //
  // When a form is "seen" on a page, a PasswordFormManager is created
  // and stored in this collection until user navigates away from page.
  typedef std::vector<PasswordFormManager*> LoginManagers;
  LoginManagers pending_login_managers_;

  // Deleter for pending_login_managers_ when PasswordManager is deleted (e.g
  // tab closes) on a page with a password form, thus containing login managers.
  STLElementDeleter<LoginManagers> login_managers_deleter_;

  // When the user submits a password/credential, this contains the
  // PasswordFormManager for the form in question until we deem the login
  // attempt to have succeeded (as in valid credentials). If it fails, we
  // send the PasswordFormManager back to the pending_login_managers_ set.
  // Scoped in case PasswordManager gets deleted (e.g tab closes) between the
  // time a user submits a login form and gets to the next page.
  scoped_ptr<PasswordFormManager> provisional_save_manager_;

  // After a successful *new* login attempt, we take the PasswordFormManager in
  // provisional_save_manager_ and move it here while the user makes up their
  // mind with the "save password" infobar. Note if the login is one we already
  // know about, the end of the line is provisional_save_manager_ because we
  // just update it on success and so such forms never end up in
  // pending_decision_manager_.
  scoped_ptr<PasswordFormManager> pending_decision_manager_;

  // The containing WebContents
  WebContents* web_contents_;

  // The LoginModelObserver (i.e LoginView) requiring autofill.
  views::LoginModelObserver* observer_;

  // Set to false to disable the password manager (will no longer fill
  // passwords or ask you if you want to save passwords).
  BooleanPrefMember password_manager_enabled_;

  DISALLOW_EVIL_CONSTRUCTORS(PasswordManager);
};

#endif

