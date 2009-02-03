// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_AUTOFILL_FORM_H_
#define WEBKIT_GLUE_AUTOFILL_FORM_H_

#include <string>
#include <vector>

namespace WebCore {
  class HTMLFormElement;
}

// The AutofillForm struct represents a single HTML form together with the
// values entered in the fields.

class AutofillForm {
 public:
  // Struct for storing name/value pairs.
  struct Element {
    Element() {}
    Element(const std::wstring& in_name, const std::wstring& in_value) {
      name = in_name;
      value = in_value;
    }
    std::wstring name;
    std::wstring value;
  };

  static AutofillForm* CreateAutofillForm(WebCore::HTMLFormElement* form);

  // A vector of all the input fields in the form.
  std::vector<Element> elements;
};

#endif  // WEBKIT_GLUE_AUTOFILL_FORM_H_
