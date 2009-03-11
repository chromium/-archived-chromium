// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A concrete definition of the DOM autocomplete framework defined by
// autocomplete_input_listener.h, for the password manager.

#ifndef WEBKIT_GLUE_PASSWORD_AUTOCOMPLETE_LISTENER_H_
#define WEBKIT_GLUE_PASSWORD_AUTOCOMPLETE_LISTENER_H_

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "HTMLInputElement.h"
MSVC_POP_WARNING();

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/password_form_dom_manager.h"

namespace webkit_glue {

// A proxy interface to a WebCore::HTMLInputElement for inline autocomplete.
// The delegate does not own the WebCore element; it only interfaces it.
class HTMLInputDelegate {
 public:
  explicit HTMLInputDelegate(WebCore::HTMLInputElement* element);
  virtual ~HTMLInputDelegate();

  virtual void SetValue(const std::wstring& value);
  virtual void SetSelectionRange(size_t start, size_t end);
  virtual void OnFinishedAutocompleting();

 private:
  // The underlying DOM element we're wrapping. We reference the underlying
  // HTMLInputElement for its lifetime to ensure it does not get freed by
  // WebCore while in use by the delegate instance.
  WebCore::HTMLInputElement* element_;

  DISALLOW_COPY_AND_ASSIGN(HTMLInputDelegate);
};


class PasswordAutocompleteListener {
 public:
  PasswordAutocompleteListener(HTMLInputDelegate* username_delegate,
                               HTMLInputDelegate* password_delegate,
                               const PasswordFormDomManager::FillData& data);
  virtual ~PasswordAutocompleteListener() {
  }

  virtual void OnBlur(WebCore::HTMLInputElement* element,
                      const std::wstring& user_input);
  virtual void OnInlineAutocompleteNeeded(WebCore::HTMLInputElement* element,
                                          const std::wstring& user_input);

 private:
  // Check if the input string resembles a potential matching login
  // (username/password) and if so, match them up by autocompleting the edit
  // delegates.
  bool TryToMatch(const std::wstring& input,
                  const std::wstring& username,
                  const std::wstring& password);

  // Access to password field to autocomplete on blur/username updates.
  scoped_ptr<HTMLInputDelegate> password_delegate_;
  scoped_ptr<HTMLInputDelegate> username_delegate_;

  // Contains the extra logins for matching on delta/blur.
  PasswordFormDomManager::FillData data_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAutocompleteListener);
};

}  // webkit_glue

#endif  // WEBKIT_GLUE_PASSWORD_AUTOCOMPLETE_LISTENER_H_
