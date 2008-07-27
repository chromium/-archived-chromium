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
//
// This file provides the implementaiton of the password manager's autocomplete 
// component.

#include "webkit/glue/password_autocomplete_listener.h"
#include "base/logging.h"

namespace webkit_glue {

PasswordAutocompleteListener::PasswordAutocompleteListener(
    AutocompleteEditDelegate* username_delegate,
    HTMLInputDelegate* password_delegate,
    const PasswordFormDomManager::FillData& data)
    : AutocompleteInputListener(username_delegate),
      password_delegate_(password_delegate),
      data_(data) {
}

void PasswordAutocompleteListener::OnBlur(const std::wstring& user_input) {
  // If this listener exists, its because the password manager had more than
  // one match for the password form, which implies it had at least one
  // [preferred] username/password pair.
  DCHECK(data_.basic_data.values.size() == 2);

  // Set the password field to match the current username.
  if (data_.basic_data.values[0] == user_input) {
    // Preferred username/login is selected.
    password_delegate_->SetValue(data_.basic_data.values[1]);
  } else if (data_.additional_logins.find(user_input) !=
             data_.additional_logins.end()) {
    // One of the extra username/logins is selected.
    password_delegate_->SetValue(data_.additional_logins[user_input]);
  }
  password_delegate_->OnFinishedAutocompleting();
}

void PasswordAutocompleteListener::OnInlineAutocompleteNeeded(
    const std::wstring& user_input) {
  // If wait_for_username is true, we only autofill the password when
  // the username field is blurred (i.e not inline) with a matching
  // username string entered.
  if (data_.wait_for_username)
    return;
  // Look for any suitable matches to current field text.
  // TODO(timsteele): The preferred login (in basic_data.values) and
  // additional logins could be bundled into the same data structure
  // (possibly even as WebCore strings) upon construction of the
  // PasswordAutocompleteListener to simplify lookup and save string
  // conversions (see SetValue) on each successful call to
  // OnInlineAutocompleteNeeded.
  if (TryToMatch(user_input,
                 data_.basic_data.values[0],
                 data_.basic_data.values[1])) {
    return;
  }

  // Scan additional logins for a match.
  for (PasswordFormDomManager::LoginCollection::iterator it =
           data_.additional_logins.begin();
       it != data_.additional_logins.end();
       ++it) {
    if (TryToMatch(user_input, it->first, it->second))
      return;
  }
}

bool PasswordAutocompleteListener::TryToMatch(const std::wstring& input,
                                              const std::wstring& username,
                                              const std::wstring& password) {
  if (input.compare(0, input.length(), username, 0, input.length()) != 0)
    return false;

  // Input matches the username, fill in required values.
  edit_delegate()->SetValue(username);
  edit_delegate()->SetSelectionRange(input.length(), username.length());
  edit_delegate()->OnFinishedAutocompleting();
  password_delegate_->SetValue(password);
  password_delegate_->OnFinishedAutocompleting();
  return true;
}

}  // webkit_glue
