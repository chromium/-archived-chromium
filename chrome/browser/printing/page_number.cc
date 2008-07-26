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

#include "chrome/browser/printing/page_number.h"

#include "base/logging.h"
#include "chrome/browser/printing/print_settings.h"

namespace printing {

PageNumber::PageNumber(const PrintSettings& settings, int document_page_count) {
  Init(settings, document_page_count);
}

PageNumber::PageNumber()
    : ranges_(NULL),
      page_number_(-1),
      page_range_index_(-1),
      document_page_count_(0) {
}

void PageNumber::operator=(const PageNumber& other) {
  ranges_ = other.ranges_;
  page_number_ = other.page_number_;
  page_range_index_ = other.page_range_index_;
  document_page_count_ = other.document_page_count_;
}

void PageNumber::Init(const PrintSettings& settings, int document_page_count) {
  DCHECK(document_page_count);
  ranges_ = settings.ranges.empty() ? NULL : &settings.ranges;
  document_page_count_ = document_page_count;
  if (ranges_) {
    page_range_index_ = 0;
    page_number_ = (*ranges_)[0].from;
  } else {
    if (document_page_count) {
      page_number_ = 0;
    } else {
      page_number_ = -1;
    }
    page_range_index_ = -1;
  }
}

int PageNumber::operator++() {
  if (!ranges_) {
    // Switch to next page.
    if (++page_number_ == document_page_count_) {
      // Finished.
      *this = npos();
    }
  } else {
    // Switch to next page.
    ++page_number_;
    // Page ranges are inclusive.
    if (page_number_ > (*ranges_)[page_range_index_].to) {
      if (++page_range_index_ == ranges_->size()) {
        // Finished.
        *this = npos();
      } else {
        page_number_ = (*ranges_)[page_range_index_].from;
      }
    }
  }
  return ToInt();
}

bool PageNumber::operator==(const PageNumber& other) const {
  return page_number_ == other.page_number_ &&
         page_range_index_ == other.page_range_index_;
}
bool PageNumber::operator!=(const PageNumber& other) const {
  return page_number_ != other.page_number_ ||
         page_range_index_ != other.page_range_index_;
}

}  // namespace printing
