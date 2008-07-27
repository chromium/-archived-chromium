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
// A concrete definition of the DOM autocomplete framework defined by
// autocomplete_input_listener.h, for the password manager.

#ifndef WEBKIT_GLUE_PASSWORD_AUTOCOMPLETE_LISTENER_H__
#define WEBKIT_GLUE_PASSWORD_AUTOCOMPLETE_LISTENER_H__

#include "base/basictypes.h"
#include "webkit/glue/autocomplete_input_listener.h"
#include "webkit/glue/password_form_dom_manager.h"

namespace webkit_glue {

class PasswordAutocompleteListener : public AutocompleteInputListener {
 public:
  PasswordAutocompleteListener(AutocompleteEditDelegate* username_delegate,
                               HTMLInputDelegate* password_delegate,
                               const PasswordFormDomManager::FillData& data);
  virtual ~PasswordAutocompleteListener() {
  }

  // AutocompleteInputListener implementation.
  virtual void OnBlur(const std::wstring& user_input);
  virtual void OnInlineAutocompleteNeeded(const std::wstring& user_input);

 private:
  // Check if the input string resembles a potential matching login
  // (username/password) and if so, match them up by autocompleting
  // the edit delegates.
  bool TryToMatch(const std::wstring& input,
                  const std::wstring& username,
                  const std::wstring& password);

  // Access to password field to autocomplete on blur/username updates.
  scoped_ptr<HTMLInputDelegate> password_delegate_;

  // Contains the extra logins for matching on delta/blur.
  PasswordFormDomManager::FillData data_;

  DISALLOW_EVIL_CONSTRUCTORS(PasswordAutocompleteListener);
};

}  // webkit_glue

#endif  // WEBKIT_GLUE_PASSWORD_AUTOCOMPLETE_LISTENER_H__
