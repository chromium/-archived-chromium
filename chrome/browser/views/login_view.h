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

#ifndef CHROME_BROWSER_VIEWS_LOGIN_VIEW_H__
#define CHROME_BROWSER_VIEWS_LOGIN_VIEW_H__

#include "base/task.h"
#include "chrome/views/view.h"

namespace ChromeViews {
class Label;
class TextField;
class LoginModel;

// Simple Model & Observer interfaces for a LoginView to facilitate exchanging
// information.
class LoginModelObserver {
 public:
  // Called by the model when a username,password pair has been identified
  // as a match for the pending login prompt.
  virtual void OnAutofillDataAvailable(const std::wstring& username,
                                       const std::wstring& password) = 0;
};

class LoginModel {
 public:
  // Set the observer interested in the data from the model.
  // observer can be null, signifying there is no longer any observer
  // interested in the data.
  virtual void SetObserver(LoginModelObserver* observer) = 0;
};

// This class is responsible for displaying the contents of a login window
// for HTTP/FTP authentication.
class LoginView : public View, public LoginModelObserver {
 public:
  explicit LoginView(const std::wstring& explanation);
  virtual ~LoginView();

  // Access the data in the username/password text fields.
  std::wstring GetUsername();
  std::wstring GetPassword();

  // LoginModelObserver implementation.
  virtual void OnAutofillDataAvailable(const std::wstring& username,
                                       const std::wstring& password);

  // Sets the model. This lets the observer notify the model
  // when it has been closed / freed, so the model should no longer try and
  // contact it. The view does not own the model, and it is the responsibility
  // of the caller to inform this view if the model is deleted.
  void SetModel(LoginModel* model);

 protected:
  // ChromeViews::View overrides:
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

 private:
  void FocusFirstField();

  // Non-owning refs to the input text fields.
  TextField* username_field_;
  TextField* password_field_;

  // Button labels
  Label* username_label_;
  Label* password_label_;

  // Authentication message.
  Label* message_label_;

  // If not null, points to a model we need to notify of our own destruction
  // so it doesn't try and access this when its too late.
  LoginModel* login_model_;

  ScopedRunnableMethodFactory<LoginView> focus_grabber_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(LoginView);
};

} // namespace
#endif // CHROME_BROWSER_VIEWS_LOGIN_VIEW_H__
