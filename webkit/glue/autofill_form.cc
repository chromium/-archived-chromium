// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "Frame.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#undef LOG

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "webkit/api/public/WebForm.h"
#include "webkit/glue/autofill_form.h"
#include "webkit/glue/glue_util.h"

using WebKit::WebForm;

namespace webkit_glue {

AutofillForm* AutofillForm::Create(const WebForm& webform) {
  RefPtr<WebCore::HTMLFormElement> form = WebFormToHTMLFormElement(webform);
  DCHECK(form);

  WebCore::Frame* frame = form->document()->frame();
  if (!frame)
    return NULL;

  WebCore::FrameLoader* loader = frame->loader();
  if (!loader)
    return NULL;

  const WTF::Vector<WebCore::HTMLFormControlElement*>& form_elements =
      form->formElements;

  // Construct a new AutofillForm.
  AutofillForm* result = new AutofillForm();

  size_t form_element_count = form_elements.size();

  for (size_t i = 0; i < form_element_count; i++) {
    WebCore::HTMLFormControlElement* form_element = form_elements[i];

    if (!form_element->hasLocalName(WebCore::HTMLNames::inputTag))
      continue;

    WebCore::HTMLInputElement* input_element =
        static_cast<WebCore::HTMLInputElement*>(form_element);
    if (!input_element->isEnabledFormControl())
      continue;

    // Ignore all input types except TEXT.
    if (input_element->inputType() != WebCore::HTMLInputElement::TEXT)
      continue;

    // For each TEXT input field, store the name and value
    std::wstring value = StringToStdWString(input_element->value());
    TrimWhitespace(value, TRIM_LEADING, &value);
    if (value.length() == 0)
      continue;

    std::wstring name = GetNameForInputElement(input_element);
    if (name.length() == 0)
      continue;  // If we have no name, there is nothing to store.

    result->elements.push_back(AutofillForm::Element(name, value));
  }

  return result;
}

// static
std::wstring AutofillForm::GetNameForInputElement(WebCore::HTMLInputElement*
    element) {
  std::wstring name = StringToStdWString(element->name());
  std::wstring trimmed_name;
  TrimWhitespace(name, TRIM_LEADING, &trimmed_name);
  if (trimmed_name.length() > 0)
    return trimmed_name;

  name = StringToStdWString(element->id());
  TrimWhitespace(name, TRIM_LEADING, &trimmed_name);
  if (trimmed_name.length() > 0)
    return trimmed_name;

  return std::wstring();
}

}  // namespace webkit_glue
