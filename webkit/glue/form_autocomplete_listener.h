// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_AUTOCOMPLETE_LISTENER_
#define WEBKIT_GLUE_FORM_AUTOCOMPLETE_LISTENER_

#include <string>

#include "webkit/glue/autocomplete_input_listener.h"

class WebViewDelegate;

namespace webkit_glue {

// This class listens for the user typing in a text input in a form and queries
// the browser for autofill information.

class FormAutocompleteListener : public AutocompleteInputListener {
 public:
  explicit FormAutocompleteListener(WebViewDelegate* webview_delegate);
  virtual ~FormAutocompleteListener() { }

  // AutocompleteInputListener implementation.
  virtual void OnBlur(WebCore::HTMLInputElement* input_element,
                      const std::wstring& user_input) { }
  virtual void OnInlineAutocompleteNeeded(WebCore::HTMLInputElement* element,
                                          const std::wstring& user_input);
  
 private:
  // The delegate associated with the WebView that contains thhe input we are
  // listening to.
  WebViewDelegate* webview_delegate_;

  DISALLOW_COPY_AND_ASSIGN(FormAutocompleteListener);
};

}  // webkit_glue

#endif  // WEBKIT_GLUE_FORM_AUTOCOMPLETE_LISTENER_
