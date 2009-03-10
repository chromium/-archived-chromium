// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_FORM_MANAGER_H__
#define CHROME_BROWSER_PASSWORD_FORM_MANAGER_H__

#include "build/build_config.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "webkit/glue/password_form.h"

class PasswordManager;
class Profile;

// Per-password-form-{on-page, dialog} class responsible for interactions
// between a given form, the per-tab PasswordManager, and the web database.
class PasswordFormManager : public WebDataServiceConsumer {
 public:
  // web_data_service allows access to current profile's Web Data
  // password_manager owns this object
  // form_on_page is the form that may be submitted and could need login data.
  // ssl_valid represents the security of the page containing observed_form,
  //           used to filter login results from database.
  PasswordFormManager(Profile* profile,
                      PasswordManager* password_manager,
                      const PasswordForm& observed_form,
                      bool ssl_valid);
  virtual ~PasswordFormManager();

  // Compare basic data of observed_form_ with argument.
  bool DoesManage(const PasswordForm& form) const;

  // Retrieves potential matching logins from the database.
  void FetchMatchingLoginsFromWebDatabase();
#if defined(OS_WIN)
  void FetchMatchingIE7LoginFromWebDatabase();
#endif

  // Simple state-check to verify whether this object as received a callback
  // from the web database and completed its matching phase. Note that the
  // callback in question occurs on the same (and only) main thread from which
  // instances of this class are ever used, but it is required since it is
  // conceivable that a user (or ui test) could attempt to submit a login
  // prompt before the callback has occured, which would InvokeLater a call to
  // PasswordManager::ProvisionallySave, which would interact with this object
  // before the db has had time to answer with matching password entries.
  // This is intended to be a one-time check; if the return value is false the
  // expectation is caller will give up. This clearly won't work if you put it
  // in a loop and wait for matching to complete; you're (supposed to be) on
  // the same thread!
  bool HasCompletedMatching();

  // Determines if the user opted to 'never remember' passwords for this form.
  bool IsBlacklisted();

  // Used by PasswordManager to determine whether or not to display
  // a SavePasswordBar when given the green light to save the PasswordForm
  // managed by this.
  bool IsNewLogin();

  // WebDataServiceConsumer implementation. If matches were found
  // (in *result), this is where we determine we need to autofill.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  // Determines if we need to autofill given the results of the query.
  void OnRequestDone(WebDataService::Handle h, const WDTypedResult* result);

#if defined(OS_WIN)
  // Determines if we need to autofill given the results of the query in the
  // ie7_password table.
  void OnIE7RequestDone(WebDataService::Handle h, const WDTypedResult* result);
#endif

  // A user opted to 'never remember' passwords for this form.
  // Blacklist it so that from now on when it is seen we ignore it.
  void PermanentlyBlacklist();

  // If the user has submitted observed_form_, provisionally hold on to
  // the submitted credentials until we are told by PasswordManager whether
  // or not the login was successful.
  void ProvisionallySave(const PasswordForm& credentials);

  // Handles save-as-new or update of the form managed by this manager.
  // Note the basic data of updated_credentials must match that of
  // observed_form_ (e.g DoesManage(pending_credentials_) == true).
  void Save();

 private:
  friend class PasswordFormManagerTest;
  // Called by destructor to ensure if this object is deleted, no potential
  // outstanding callbacks can call OnWebDataServiceRequestDone.
  void CancelLoginsQuery();

  // Helper for OnWebDataServiceRequestDone to determine whether or not
  // the given result form is worth scoring.
  bool IgnoreResult(const PasswordForm& form) const;

  // Helper for Save in the case that best_matches.size() == 0, meaning
  // we have no prior record of this form/username/password and the user
  // has opted to 'Save Password'.
  void SaveAsNewLogin();

  // Helper for OnWebDataServiceRequestDone to score an individual result
  // against the observed_form_.
  int ScoreResult(const PasswordForm& form) const;

  // Helper for Save in the case that best_matches.size() > 0, meaning
  // we have at least one match for this form/username/password. This
  // Updates the form managed by this object, as well as any matching forms
  // that now need to have preferred bit changed, since updated_credentials
  // is now implicitly 'preferred'.
  void UpdateLogin();

  // Set of PasswordForms from the DB that best match the form
  // being managed by this. Use a map instead of vector, because we most
  // frequently require lookups by username value in IsNewLogin.
  PasswordFormMap best_matches_;

  // Cleans up when best_matches_ goes out of scope.
  STLValueDeleter<PasswordFormMap> best_matches_deleter_;

  // The PasswordForm from the page or dialog managed by this.
  PasswordForm observed_form_;

  // The origin url path of observed_form_ tokenized, for convenience when
  // scoring.
  std::vector<std::string> form_path_tokens_;

  // Stores updated credentials when the form was submitted but success is
  // still unknown.
  PasswordForm pending_credentials_;

  // Whether pending_credentials_ stores a new login or is an update
  // to an existing one.
  bool is_new_login_;

  // PasswordManager owning this.
  const PasswordManager* const password_manager_;

  // Handle to any pending WebDataService::GetLogins query.
  WebDataService::Handle pending_login_query_;

  // Convenience pointer to entry in best_matches_ that is marked
  // as preferred. This is only allowed to be null if there are no best matches
  // at all, since there will always be one preferred login when there are
  // multiple matches (when first saved, a login is marked preferred).
  const PasswordForm* preferred_match_;

  typedef enum {
    PRE_MATCHING_PHASE,      // Have not yet invoked a GetLogins query to find
                             // matching login information from DB.
    MATCHING_PHASE,          // We've made a GetLogins request, but
                             // haven't received or finished processing result.
    POST_MATCHING_PHASE      // We've queried the DB and processed matching
                             // login results.
  } PasswordFormManagerState;

  // State of matching process, used to verify that we don't call methods
  // assuming we've already processed the web data request for matching logins,
  // when we actually haven't.
  PasswordFormManagerState state_;

  // The profile from which we get the WebDataService.
  Profile* profile_;

  DISALLOW_EVIL_CONSTRUCTORS(PasswordFormManager);
};
#endif  // CHROME_BROWSER_PASSWORD_FORM_MANAGER_H__
