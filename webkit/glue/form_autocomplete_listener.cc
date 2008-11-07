// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/form_autocomplete_listener.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "HTMLInputElement.h"
MSVC_POP_WARNING();

#undef LOG

#include "webkit/glue/autocomplete_input_listener.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webview_delegate.h"

namespace webkit_glue {

FormAutocompleteListener::FormAutocompleteListener(
    WebViewDelegate* webview_delegate,
    WebCore::HTMLInputElement* input_element)
    : AutocompleteInputListener(new HTMLInputDelegate(input_element)),
      webview_delegate_(webview_delegate),
      name_(webkit_glue::StringToStdWString(input_element->name().string())),
      node_id_(reinterpret_cast<int64>(input_element)) {
  DCHECK(input_element->isTextField() && !input_element->isPasswordField() &&
         input_element->autoComplete());
}

void FormAutocompleteListener::OnInlineAutocompleteNeeded(
    const std::wstring& user_input) {
  webview_delegate_->QueryFormFieldAutofill(name_, user_input, node_id_);
}

}  // namespace
