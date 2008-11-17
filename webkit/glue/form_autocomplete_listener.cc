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
    WebViewDelegate* webview_delegate)
    : webview_delegate_(webview_delegate) {
}

void FormAutocompleteListener::OnInlineAutocompleteNeeded(
    WebCore::HTMLInputElement* input_element,
    const std::wstring& user_input) {
  std::wstring name = webkit_glue::StringToStdWString(input_element->
      name().string());
  webview_delegate_->QueryFormFieldAutofill(name, user_input,
      reinterpret_cast<int64>(input_element));
}

}  // namespace
