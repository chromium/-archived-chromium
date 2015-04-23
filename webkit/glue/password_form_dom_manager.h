// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PASSWORD_FORM_DOM_MANAGER_H_
#define WEBKIT_GLUE_PASSWORD_FORM_DOM_MANAGER_H_

#include "webkit/glue/form_data.h"
#include "webkit/glue/password_form.h"

namespace WebKit {
class WebForm;
}

class GURL;

namespace webkit_glue {

class PasswordFormDomManager {
 public:
  typedef std::map<std::wstring, std::wstring> LoginCollection;

  // Structure used for autofilling password forms.
  // basic_data identifies the HTML form on the page and preferred username/
  //            password for login, while
  // additional_logins is a list of other matching user/pass pairs for the form.
  // wait_for_username tells us whether we need to wait for the user to enter
  // a valid username before we autofill the password. By default, this is off
  // unless the PasswordManager determined there is an additional risk
  // associated with this form. This can happen, for example, if action URI's
  // of the observed form and our saved representation don't match up.
  struct FillData {
    FormData basic_data;
    LoginCollection additional_logins;
    bool wait_for_username;
    FillData() : wait_for_username(false) {
    }
  };

  // Create a PasswordForm from DOM form. Webkit doesn't allow storing
  // custom metadata to DOM nodes, so we have to do this every time an event
  // happens with a given form and compare against previously Create'd forms
  // to identify..which sucks.
  static PasswordForm* CreatePasswordForm(const WebKit::WebForm& form);

  // Create a FillData structure in preparation for autofilling a form,
  // from basic_data identifying which form to fill, and a collection of
  // matching stored logins to use as username/password values.
  // preferred_match should equal (address) one of matches.
  // wait_for_username_before_autofill is true if we should not autofill
  // anything until the user typed in a valid username and blurred the field.
  static void InitFillData(const PasswordForm& form_on_page,
                           const PasswordFormMap& matches,
                           const PasswordForm* const preferred_match,
                           bool wait_for_username_before_autofill,
                           FillData* result);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PasswordFormDomManager);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_PASSWORD_FORM_DOM_MANAGER_H__
