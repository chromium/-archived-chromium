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

#ifndef CHROME_BROWSER_PRINTING_PAGE_OVERLAYS_H__
#define CHROME_BROWSER_PRINTING_PAGE_OVERLAYS_H__

#include <string>

namespace printing {

class PrintedDocument;
class PrintedPage;

// Page's overlays, i.e. headers and footers. Contains the strings that will be
// printed in the overlays, with actual values as variables. The variables are
// replaced by their actual values with ReplaceVariables().
class PageOverlays {
 public:
  // Position of the header/footer.
  enum HorizontalPosition {
    LEFT,
    CENTER,
    RIGHT,
  };

  // Position of the header/footer.
  enum VerticalPosition {
    TOP,
    BOTTOM,
  };

  PageOverlays();

  // Equality operator.
  bool Equals(const PageOverlays& rhs) const;

  // Returns the string of an overlay according to its x,y position.
  const std::wstring& GetOverlay(HorizontalPosition x,
                                 VerticalPosition y) const;

  // Replaces the variables in |input| with their actual values according to the
  // properties of the current printed document and the current printed page.
  static std::wstring ReplaceVariables(const std::wstring& input,
                                       const PrintedDocument& document,
                                       const PrintedPage& page);

  // Keys that are dynamically replaced in the header and footer by their actual
  // values.
  // Web page's title.
  static const wchar_t* const kTitle;
  // Print job's start time.
  static const wchar_t* const kTime;
  // Print job's start date.
  static const wchar_t* const kDate;
  // Printed page's number.
  static const wchar_t* const kPage;
  // Print job's total page count.
  static const wchar_t* const kPageCount;
  // Printed page's number on total page count.
  static const wchar_t* const kPageOnTotal;
  // Web page's displayed url.
  static const wchar_t* const kUrl;

  // Actual headers and footers.
  std::wstring top_left;
  std::wstring top_center;
  std::wstring top_right;
  std::wstring bottom_left;
  std::wstring bottom_center;
  std::wstring bottom_right;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PAGE_OVERLAYS_H__
