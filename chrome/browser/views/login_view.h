// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_LOGIN_VIEW_H__
#define CHROME_BROWSER_VIEWS_LOGIN_VIEW_H__

#include "base/task.h"
#include "views/view.h"

namespace views {
class Label;
class Textfield;
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
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

 private:
  void FocusFirstField();

  // Non-owning refs to the input text fields.
  Textfield* username_field_;
  Textfield* password_field_;

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
