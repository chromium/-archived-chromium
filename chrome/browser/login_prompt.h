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

#ifndef CHROME_BROWSER_LOGIN_PROMPT_H__
#define CHROME_BROWSER_LOGIN_PROMPT_H__

#include <string>

#include "base/basictypes.h"

namespace net {
class AuthChallengeInfo;
}

class URLRequest;
class MessageLoop;
class TabContents;

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

#endif // CHROME_BROWSER_LOGIN_PROMPT_H__
