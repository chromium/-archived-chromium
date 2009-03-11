// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_SEARCHABLE_FORM_DATA_H__
#define WEBKIT_GLUE_SEARCHABLE_FORM_DATA_H__

#include <string>

#include "googleurl/src/gurl.h"

namespace WebCore {
class Element;
class HTMLFormElement;
}

// SearchableFormData encapsulates a URL and class name of an INPUT field
// that correspond to a searchable form request.
class SearchableFormData {
 public:
  // If form contains elements that constitutes a valid searchable form
  // request, a SearchableFormData is created and returned.
  static SearchableFormData* Create(WebCore::HTMLFormElement* form);

  // If the element is contained in a form that constitutes a valid searchable
  // form, a SearchableFormData is created and returned.
  static SearchableFormData* Create(WebCore::Element* element);

  // Returns true if the two SearchableFormData are equal, false otherwise.
  // Either argument may be NULL. If both elements are NULL, true is returned.
  static bool Equals(const SearchableFormData* a, const SearchableFormData* b);

  // URL for the searchable form request.
  const GURL& url() const { return url_; }

  // Class name of the INPUT element the user input text into.
  const std::wstring element_name() const { return element_name_; }

  // Value of the text field in the form.
  const std::wstring element_value() const { return element_value_; }

  // Encoding used to encode the form parameters; never empty.
  const std::string& encoding() const { return encoding_; }

 private:
  SearchableFormData(const GURL& url,
                     const std::wstring& element_name,
                     const std::wstring& element_value,
                     const std::string& encoding);

  const GURL url_;
  const std::wstring element_name_;
  const std::wstring element_value_;
  const std::string encoding_;

  DISALLOW_EVIL_CONSTRUCTORS(SearchableFormData);
};

#endif // WEBKIT_GLUE_SEARCHABLE_FORM_H__
