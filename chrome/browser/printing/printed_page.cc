// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printed_page.h"

namespace printing {

PrintedPage::PrintedPage(int page_number,
                         NativeMetafile* native_metafile,
                         const gfx::Size& page_size)
    : page_number_(page_number),
      native_metafile_(native_metafile),
      page_size_(page_size) {
}

PrintedPage::~PrintedPage() {
}

const NativeMetafile* PrintedPage::native_metafile() const {
  return native_metafile_.get();
}

}  // namespace printing
