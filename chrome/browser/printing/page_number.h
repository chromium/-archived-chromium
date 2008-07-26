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

#ifndef CHROME_BROWSER_PRINTING_PAGE_NUMBER_H__
#define CHROME_BROWSER_PRINTING_PAGE_NUMBER_H__

#ifdef _DEBUG
#include <ostream>
#endif

#include "chrome/browser/printing/page_range.h"

namespace printing {

class PrintSettings;

// Represents a page series following the array of page ranges defined in a
// PrintSettings.
class PageNumber {
 public:
  // Initializes the page to the first page in the settings's range or 0.
  PageNumber(const PrintSettings& settings, int document_page_count);

  PageNumber();

  void operator=(const PageNumber& other);

  // Initializes the page to the first page in the setting's range or 0. It
  // initialize to npos if the range is empty and document_page_count is 0.
  void Init(const PrintSettings& settings, int document_page_count);

  // Converts to a page numbers.
  int ToInt() const {
    return page_number_;
  }

  // Calculates the next page in the serie.
  int operator++();

  // Returns an instance that represents the end of a serie.
  static const PageNumber npos() {
    return PageNumber();
  }

  // Equality operator. Only the current page number is verified so that
  // "page != PageNumber::npos()" works.
  bool operator==(const PageNumber& other) const;
  bool operator!=(const PageNumber& other) const;

 private:
  // The page range to follow.
  const PageRanges* ranges_;

  // The next page to be printed. -1 when not printing.
  int page_number_;

  // The next page to be printed. -1 when not used. Valid only if
  // document()->settings().range.empty() is false.
  int page_range_index_;

  // Number of expected pages in the document. Used when ranges_ is NULL.
  int document_page_count_;
};

// Debug output support.
template<class E, class T>
inline std::basic_ostream<E,T>& operator<<(std::basic_ostream<E,T>& ss,
                                           const PageNumber& page) {
  return ss << page.ToInt();
}

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PAGE_NUMBER_H__
