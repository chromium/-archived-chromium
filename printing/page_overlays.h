// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PAGE_OVERLAYS_H_
#define PRINTING_PAGE_OVERLAYS_H_

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

  // Sets the string of an overlay according to its x,y position.
  void SetOverlay(HorizontalPosition x, VerticalPosition y,
                  std::wstring& input);

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

#endif  // PRINTING_PAGE_OVERLAYS_H_
