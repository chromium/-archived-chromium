// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#undef LOG

#include "base/logging.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/password_form_dom_manager.h"

using WebKit::WebForm;

namespace webkit_glue {

namespace {

// Maximum number of password fields we will observe before throwing our
// hands in the air and giving up with a given form.
const size_t kMaxPasswords = 3;

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
void FindPasswordFormFields(
    WebCore::HTMLFormElement* form,
    PasswordFormFields* fields) {
  DCHECK(form && fields);
  int first_password_index = 0;
  // First, find the password fields and activated submit button
  const WTF::Vector<WebCore::HTMLFormControlElement*>& form_elements =
      form->formElements;
  for (size_t i = 0; i < form_elements.size(); i++) {
    WebCore::HTMLFormControlElement* form_element = form_elements[i];
    if (form_element->isActivatedSubmit())
      fields->submit = form_element;

    if (!form_element->hasLocalName(WebCore::HTMLNames::inputTag))
      continue;

    WebCore::HTMLInputElement* input_element =
        static_cast<WebCore::HTMLInputElement*>(form_element);
    if (!input_element->isEnabledFormControl())
      continue;

    if ((fields->passwords.size() < kMaxPasswords) &&
        (input_element->inputType() == WebCore::HTMLInputElement::PASSWORD) &&
        (input_element->autoComplete())) {
      if (fields->passwords.empty())
        first_password_index = i;
      fields->passwords.push_back(input_element);
    }
  }

  if (!fields->passwords.empty()) {
    // Then, search backwards for the username field
    for (int i = first_password_index - 1; i >= 0; i--) {
      WebCore::HTMLFormControlElement* form_element = form_elements[i];
      if (!form_element->hasLocalName(WebCore::HTMLNames::inputTag))
        continue;

      WebCore::HTMLInputElement* input_element =
        static_cast<WebCore::HTMLInputElement*>(form_element);
      if (!input_element->isEnabledFormControl())
        continue;

      if ((input_element->inputType() == WebCore::HTMLInputElement::TEXT) &&
          (input_element->autoComplete())) {
        fields->username = input_element;
        break;
      }
    }
  }
}

// Helper to determine which password is the main one, and which is
// an old password (e.g on a "make new password" form), if any.
bool LocateSpecificPasswords(
    PasswordFormFields* fields,
    WebCore::HTMLInputElement** password,
    WebCore::HTMLInputElement** old_password) {
  DCHECK(fields && password && old_password);
  switch (fields->passwords.size()) {
    case 1:
      // Single password, easy.
      *password = fields->passwords[0];
      break;
    case 2:
      if (fields->passwords[0]->value() == fields->passwords[1]->value())
        // Treat two identical passwords as a single password.
        *password = fields->passwords[0];
      else {
        // Assume first is old password, second is new (no choice but to guess).
        *old_password = fields->passwords[0];
        *password = fields->passwords[1];
      }
      break;
    case 3:
      if (fields->passwords[0]->value() == fields->passwords[1]->value() &&
          fields->passwords[0]->value() == fields->passwords[2]->value()) {
          // All three passwords the same? Just treat as one and hope.
          *password = fields->passwords[0];
      } else if (fields->passwords[0]->value() ==
                 fields->passwords[1]->value()) {
          // Two the same and one different -> old password is duplicated one.
          *old_password = fields->passwords[0];
          *password = fields->passwords[2];
      } else if (fields->passwords[1]->value() ==
                 fields->passwords[2]->value()) {
        *old_password = fields->passwords[0];
        *password = fields->passwords[1];
      } else {
        // Three different passwords, or first and last match with middle
        // different. No idea which is which, so no luck.
        return false;
      }
      break;
    default:
      return false;
  }
  return true;
}

// Helper to gather up the final form data and create a PasswordForm.
PasswordForm* AssemblePasswordFormResult(
    const GURL& full_origin,
    const GURL& full_action,
    WebCore::HTMLFormControlElement* submit,
    WebCore::HTMLInputElement* username,
    WebCore::HTMLInputElement* old_password,
    WebCore::HTMLInputElement* password) {
  std::wstring empty;
  PasswordForm* result = new PasswordForm();
  // Ignore the query and ref components
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  // We want to keep the path but strip any authentication data, as well as
  // query and ref portions of URL, for the form action and form origin.
  result->action = full_action.ReplaceComponents(rep);
  result->origin = full_origin.ReplaceComponents(rep);

  // Naming is confusing here because we have both the HTML form origin URL
  // the page where the form was seen), and the "origin" components of the url
  // (scheme, host, and port).
  result->signon_realm = full_origin.GetOrigin().spec();
  // Note PasswordManager sets ssl_valid by asking the WebContents' SSLManager.
  result->submit_element =
      submit == NULL ? empty : StringToStdWString(submit->name());
  result->username_element =
      username == NULL ? empty : StringToStdWString(username->name());
  result->username_value =
      username == NULL ? empty : StringToStdWString(username->value());
  result->password_element =
      password == NULL ? empty : StringToStdWString(password->name());
  result->password_value =
      password == NULL ? empty : StringToStdWString(password->value());
  result->old_password_element =
      old_password == NULL ? empty : StringToStdWString(old_password->name());
  result->old_password_value =
      old_password == NULL ? empty : StringToStdWString(old_password->value());
  return result;
}

}  // namespace

PasswordForm* PasswordFormDomManager::CreatePasswordForm(
    const WebForm& webform) {
  RefPtr<WebCore::HTMLFormElement> form = WebFormToHTMLFormElement(webform);

  WebCore::Frame* frame = form->document()->frame();
  if (!frame)
    return NULL;

  PasswordFormFields fields;
  FindPasswordFormFields(form.get(), &fields);

  // Get the document URL
  WebCore::String origin_string = form->document()->documentURI();
  GURL full_origin(StringToStdString(origin_string));

  // Calculate the canonical action URL
  GURL full_action(KURLToGURL(frame->loader()->completeURL(form->action())));
  if (!full_action.is_valid())
    return NULL;

  // Determine the types of the password fields
  WebCore::HTMLInputElement* password = NULL;
  WebCore::HTMLInputElement* old_password = NULL;
  if (!LocateSpecificPasswords(&fields, &password, &old_password))
    return NULL;

  return AssemblePasswordFormResult(full_origin, full_action,
                                    fields.submit, fields.username,
                                    old_password, password);
}

// static
void PasswordFormDomManager::InitFillData(
    const PasswordForm& form_on_page,
    const PasswordFormMap& matches,
    const PasswordForm* const preferred_match,
    bool wait_for_username_before_autofill,
    PasswordFormDomManager::FillData* result) {
  DCHECK(preferred_match);
  // Fill basic form data.
  result->basic_data.origin = form_on_page.origin;
  result->basic_data.action = form_on_page.action;
  result->basic_data.elements.push_back(form_on_page.username_element);
  result->basic_data.values.push_back(preferred_match->username_value);
  result->basic_data.elements.push_back(form_on_page.password_element);
  result->basic_data.values.push_back(preferred_match->password_value);
  result->basic_data.submit = form_on_page.submit_element;
  result->wait_for_username = wait_for_username_before_autofill;

  // Copy additional username/value pairs.
  PasswordFormMap::const_iterator iter;
  for (iter = matches.begin(); iter != matches.end(); iter++) {
    if (iter->second != preferred_match)
      result->additional_logins[iter->first] = iter->second->password_value;
  }
}

}  // namespace webkit_glue
