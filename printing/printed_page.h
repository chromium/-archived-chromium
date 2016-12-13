// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTED_PAGE_H_
#define PRINTING_PRINTED_PAGE_H_

#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "printing/native_metafile.h"

namespace printing {

// Contains the data to reproduce a printed page, either on screen or on
// paper. Once created, this object is immutable. It has no reference to the
// PrintedDocument containing this page.
// Note: May be accessed from many threads at the same time. This is an non
// issue since this object is immutable. The reason is that a page may be
// printed and be displayed at the same time.
class PrintedPage : public base::RefCountedThreadSafe<PrintedPage> {
 public:
  PrintedPage(int page_number,
              NativeMetafile* native_metafile,
              const gfx::Size& page_size);
  ~PrintedPage();

  // Getters
  int page_number() const { return page_number_; }
  const NativeMetafile* native_metafile() const;
  const gfx::Size& page_size() const { return page_size_; }

 private:
  // Page number inside the printed document.
  const int page_number_;

  // Actual paint data.
  const scoped_ptr<NativeMetafile> native_metafile_;

  // The physical page size. To support multiple page formats inside on print
  // job.
  const gfx::Size page_size_;

  DISALLOW_EVIL_CONSTRUCTORS(PrintedPage);
};

}  // namespace printing

#endif  // PRINTING_PRINTED_PAGE_H_
