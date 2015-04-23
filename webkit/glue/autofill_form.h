// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_AUTOFILL_FORM_H_
#define WEBKIT_GLUE_AUTOFILL_FORM_H_

#include <string>
#include <vector>

namespace WebCore {
class HTMLInputElement;
}

namespace WebKit {
class WebForm;
}

namespace webkit_glue {

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

  static AutofillForm* Create(const WebKit::WebForm& form);

  // Returns the name that should be used for the specified |element| when
  // storing autofill data.  This is either the field name or its id, an empty
  // string if it has no name and no id.
  static std::wstring GetNameForInputElement(WebCore::HTMLInputElement*
      element);

  // A vector of all the input fields in the form.
  std::vector<Element> elements;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_AUTOFILL_FORM_H_
