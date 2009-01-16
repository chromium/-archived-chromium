// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_PROMPT_H__
#define CHROME_BROWSER_LOGIN_PROMPT_H__

#include <string>

#include "base/basictypes.h"

namespace net {
class AuthChallengeInfo;
}

class GURL;
class MessageLoop;
class TabContents;
class URLRequest;

// This is the interface for the class that routes authentication info to
// the URLRequest that needs it.  Used by the automation proxy for testing.
// These functions should be (and are, in LoginHandlerImpl) implemented in
// a thread safe manner.
class LoginHandler {
 public:
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


// Get the signon_realm under which the identity should be saved.
std::string GetSignonRealm(const GURL& url,
                           const net::AuthChallengeInfo& auth_info);

#endif  // CHROME_BROWSER_LOGIN_PROMPT_H__

