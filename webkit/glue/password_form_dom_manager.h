// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PASSWORD_FORM_DOM_MANAGER_H__
#define WEBKIT_GLUE_PASSWORD_FORM_DOM_MANAGER_H__

#include "webkit/glue/form_data.h"
#include "webkit/glue/password_form.h"

namespace WebCore {
class HTMLFormElement;
class HTMLInputElement;
class HTMLFormControlElement;
}

class GURL;

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
  static PasswordForm* CreatePasswordForm(WebCore::HTMLFormElement* form);

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
  // Helper structure to locate username, passwords and submit fields.
  struct PasswordFormFields {
    WebCore::HTMLInputElement* username;
    std::vector<WebCore::HTMLInputElement*> passwords;
    WebCore::HTMLFormControlElement* submit;
    PasswordFormFields() : username(NULL), submit(NULL) {
    }
  };

  // Helper to CreatePasswordForm to do the locating of username/password
  // fields.
  // This method based on Firefox2 code in
  //   toolkit/components/passwordmgr/base/nsPasswordManager.cpp
  // Its license block is

  /* ***** BEGIN LICENSE BLOCK *****
  * Version: MPL 1.1/GPL 2.0/LGPL 2.1
  *
  * The contents of this file are subject to the Mozilla Public License Version
  * 1.1 (the "License"); you may not use this file except in compliance with
  * the License. You may obtain a copy of the License at
  * http://www.mozilla.org/MPL/
  *
  * Software distributed under the License is distributed on an "AS IS" basis,
  * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
  * for the specific language governing rights and limitations under the
  * License.
  *
  * The Original Code is Mozilla Password Manager.
  *
  * The Initial Developer of the Original Code is
  * Brian Ryner.
  * Portions created by the Initial Developer are Copyright (C) 2003
  * the Initial Developer. All Rights Reserved.
  *
  * Contributor(s):
  *  Brian Ryner <bryner@brianryner.com>
  *
  * Alternatively, the contents of this file may be used under the terms of
  * either the GNU General Public License Version 2 or later (the "GPL"), or
  * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
  * in which case the provisions of the GPL or the LGPL are applicable instead
  * of those above. If you wish to allow use of your version of this file only
  * under the terms of either the GPL or the LGPL, and not to allow others to
  * use your version of this file under the terms of the MPL, indicate your
  * decision by deleting the provisions above and replace them with the notice
  * and other provisions required by the GPL or the LGPL. If you do not delete
  * the provisions above, a recipient may use your version of this file under
  * the terms of any one of the MPL, the GPL or the LGPL.
  *
  * ***** END LICENSE BLOCK ***** */
  static void FindPasswordFormFields(WebCore::HTMLFormElement* form,
                                     PasswordFormFields* fields);
  // Helper to determine which password is the main one, and which is
  // an old password (e.g on a "make new password" form), if any.
  static bool LocateSpecificPasswords(
      PasswordFormFields* fields,
      WebCore::HTMLInputElement** password,
      WebCore::HTMLInputElement** old_password_index);

  // Helper to gather up the final form data and create a PasswordForm.
  static PasswordForm* AssemblePasswordFormResult(
      const GURL& full_origin,
      const GURL& full_action,
      WebCore::HTMLFormControlElement* submit,
      WebCore::HTMLInputElement* username,
      WebCore::HTMLInputElement* old_password,
      WebCore::HTMLInputElement* password);

  // This class can't be instantiated.
  DISALLOW_IMPLICIT_CONSTRUCTORS(PasswordFormDomManager);
};

#endif  // WEBKIT_GLUE_PASSWORD_FORM_DOM_MANAGER_H__

