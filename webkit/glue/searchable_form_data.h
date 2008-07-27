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
  SearchableFormData(const std::wstring& url,
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
