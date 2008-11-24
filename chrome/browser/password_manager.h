// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_H__
#define CHROME_BROWSER_PASSWORD_MANAGER_H__

#include "base/scoped_ptr.h"
#include "chrome/browser/views/info_bar_confirm_view.h"
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
class PasswordManager : public views::LoginModel {
 public:
  static void RegisterUserPrefs(PrefService* prefs);

  explicit PasswordManager(WebContents* web_contents);
  ~PasswordManager();

  // Called by a PasswordFormManager when it decides a form can be autofilled
  // on the page.
  void Autofill(const PasswordForm& form_for_autofill,
                const PasswordFormMap& best_matches,
                const PasswordForm* const preferred_match) const;

  // Closes any visible password manager UI
  void CloseBars();

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
  // The Info Bar UI that prompts the user to save a password.
  friend class SavePasswordBar;
  class SavePasswordBar : public InfoBarConfirmView {
   public:
    SavePasswordBar(PasswordFormManager* form_manager,
                    PasswordManager* password_manager);
    virtual ~SavePasswordBar();

    // InfoBarConfirmView overrides.
    virtual void OKButtonPressed();
    virtual void CancelButtonPressed();

   private:
    PasswordFormManager* form_manager_;
    PasswordManager* password_manager_;

    DISALLOW_EVIL_CONSTRUCTORS(SavePasswordBar);
  };

  // Replace the current InfoBar with a new one. Just adds if there is no
  // visible InfoBars.
  void ReplaceInfoBar(InfoBarItemView* view);

  // The currently visible InfoBar (NULL if none)
  InfoBarItemView* current_bar_;

  // When a form is "seen" on a page, a PasswordFormManager is created
  // and stored in this collection until user navigates away from page.
  typedef std::vector<PasswordFormManager*> LoginManagers;
  LoginManagers pending_login_managers_;

  // Deleter for pending_login_managers_ when PasswordManager is deleted (e.g
  // tab closes) on a page with a password form, thus containing login managers.
  STLElementDeleter<LoginManagers> login_managers_deleter_;

  // When the user opts to save a password, this contains the
  // PasswordFormManager for the form in question.
  // Scoped incase PasswordManager gets deleted (e.g tab closes) between the
  // time a user submits a login form and gets to the next page.
  scoped_ptr<PasswordFormManager> pending_save_manager_;

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

