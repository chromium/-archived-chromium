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
  FormAutocompleteListener(WebViewDelegate* webview_delegate,
                           WebCore::HTMLInputElement* input_element);
  virtual ~FormAutocompleteListener() { }

  // AutocompleteInputListener implementation.
  virtual void OnBlur(const std::wstring& user_input) { }
  virtual void OnInlineAutocompleteNeeded(const std::wstring& user_input);
  
 private:
  // The delegate associated with the WebView that contains thhe input we are
  // listening to.
  WebViewDelegate* webview_delegate_;

  // The name of the input node we are listening to.
  std::wstring name_;

  // An id to represent the input element.  That ID is passed to the call that
  // queries for suggestions.
  int64 node_id_;

  DISALLOW_COPY_AND_ASSIGN(FormAutocompleteListener);
};

}  // webkit_glue

#endif  // WEBKIT_GLUE_FORM_AUTOCOMPLETE_LISTENER_
