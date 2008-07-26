// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
class PasswordManager : public ChromeViews::LoginModel {
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

  // Notification that the user initiated a navigation away from the current
  // page. Unless this is a password form submission, for our purposes this
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

  // LoginModel implementation.
  virtual void SetObserver(ChromeViews::LoginModelObserver* observer) {
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
  ChromeViews::LoginModelObserver* observer_;

  // Set to false to disable the password manager (will no longer fill
  // passwords or ask you if you want to save passwords).
  BooleanPrefMember password_manager_enabled_;

  DISALLOW_EVIL_CONSTRUCTORS(PasswordManager);
};

#endif
