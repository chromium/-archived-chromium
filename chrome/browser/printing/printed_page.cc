// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printed_page.h"

#include "chrome/common/gfx/emf.h"

namespace printing {

PrintedPage::PrintedPage(int page_number,
                         gfx::Emf* emf,
                         const gfx::Size& page_size)
    : page_number_(page_number),
      emf_(emf),
      page_size_(page_size) {
}

PrintedPage::~PrintedPage() {
}

const gfx::Emf* PrintedPage::emf() const {
  return emf_.get();
}

}  // namespace printing
