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

#ifndef CHROME_BROWSER_PRINTING_PAGE_SETUP_H__
#define CHROME_BROWSER_PRINTING_PAGE_SETUP_H__

#include "base/gfx/rect.h"

namespace printing {

// Margins for a page setup.
class PageMargins {
 public:
  PageMargins();

  void Clear();

  // Equality operator.
  bool Equals(const PageMargins& rhs) const;

  // Vertical space for the overlay from the top of the sheet.
  int header;
  // Vertical space for the overlay from the bottom of the sheet.
  int footer;
  // Margin on each side of the sheet.
  int left;
  int top;
  int right;
  int bottom;
};

// Settings that define the size and printable areas of a page. Unit is
// unspecified.
class PageSetup {
 public:
  PageSetup();

  void Clear();

  // Equality operator.
  bool Equals(const PageSetup& rhs) const;

  void Init(const gfx::Size& physical_size, const gfx::Rect& printable_area,
            int text_height);

  void SetRequestedMargins(const PageMargins& requested_margins);

  const gfx::Size& physical_size() const { return physical_size_; }
  const gfx::Rect& overlay_area() const { return overlay_area_; }
  const gfx::Rect& content_area() const { return content_area_; }
  const PageMargins& effective_margins() const {
    return effective_margins_;
  }

 private:
  // Physical size of the page, including non-printable margins.
  gfx::Size physical_size_;

  // The printable area as specified by the printer driver. We can't get
  // larger than this.
  gfx::Rect printable_area_;

  // The printable area for headers and footers.
  gfx::Rect overlay_area_;

  // The printable area as selected by the user's margins.
  gfx::Rect content_area_;

  // Effective margins.
  PageMargins effective_margins_;

  // Requested margins.
  PageMargins requested_margins_;

  // Space that must be kept free for the overlays.
  int text_height_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PAGE_SETUP_H__
