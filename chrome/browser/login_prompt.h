// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_PROMPT_H_
#define CHROME_BROWSER_LOGIN_PROMPT_H_

#include <string>

#include "base/basictypes.h"

namespace net {
class AuthChallengeInfo;
}

namespace webkit_glue {
struct PasswordForm;
}

class ConstrainedWindow;
class GURL;
class MessageLoop;
class PasswordManager;
class TabContents;
class URLRequest;

// This is the interface for the class that routes authentication info to
// the URLRequest that needs it.  Used by the automation proxy for testing.
// These functions should be (and are, in LoginHandlerImpl) implemented in
// a thread safe manner.
//
// TODO(erg): Refactor the common code from all LoginHandler subclasses into a
// common controller class. All the methods below have the same copy/pasted
// implementation. This is more difficult then it should be because all these
// subclasses are also base::RefCountedThreadSafe<> and I'm not sure how to get
// ownership correct.  http://crbug.com/14909
class LoginHandler {
 public:
  // Builds the platform specific LoginHandler. Used from within
  // CreateLoginPrompt() which creates tasks.
  static LoginHandler* Create(URLRequest* request, MessageLoop* ui_loop);

  // Initializes the underlying platform specific view.
  virtual void BuildViewForPasswordManager(PasswordManager* manager,
                                           std::wstring explanation) = 0;

  // Sets information about the authentication type (|form|) and the
  // |password_manager| for this profile.
  virtual void SetPasswordForm(const webkit_glue::PasswordForm& form) = 0;
  virtual void SetPasswordManager(PasswordManager* password_manager) = 0;

  // Returns the TabContents that needs authentication.
  virtual TabContents* GetTabContentsForLogin() = 0;

  // Resend the request with authentication credentials.
  // This function can be called from either thread.
  virtual void SetAuth(const std::wstring& username,
                       const std::wstring& password) = 0;

  // Display the error page without asking for credentials again.
  // This function can be called from either thread.
  virtual void CancelAuth() = 0;

  // Notify the handler that the request was cancelled.
  // This function can only be called from the IO thread.
  virtual void OnRequestCancelled() = 0;
};

// Details to provide the NotificationObserver.  Used by the automation proxy
// for testing.
class LoginNotificationDetails {
 public:
  LoginNotificationDetails(LoginHandler* handler) : handler_(handler) {}
  LoginHandler* handler() const { return handler_; }

 private:
  LoginNotificationDetails() {}

  LoginHandler* handler_;  // Where to send the response.

  DISALLOW_EVIL_CONSTRUCTORS(LoginNotificationDetails);
};

// Prompts the user for their username and password.  This is designed to
// be called on the background (I/O) thread, in response to
// URLRequest::Delegate::OnAuthRequired.  The prompt will be created
// on the main UI thread via a call to ui_loop's InvokeLater, and will send the
// credentials back to the URLRequest on the calling thread.
// A LoginHandler object (which lives on the calling thread) is returned,
// which can be used to set or cancel authentication programmatically.  The
// caller must invoke OnRequestCancelled() on this LoginHandler before
// destroying the URLRequest.
LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                URLRequest* request,
                                MessageLoop* ui_loop);

// Helper to remove the ref from an URLRequest to the LoginHandler.
// Should only be called from the IO thread, since it accesses an URLRequest.
void ResetLoginHandlerForRequest(URLRequest* request);

// Get the signon_realm under which the identity should be saved.
std::string GetSignonRealm(const GURL& url,
                           const net::AuthChallengeInfo& auth_info);

#endif  // CHROME_BROWSER_LOGIN_PROMPT_H_
