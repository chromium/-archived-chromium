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

#ifndef CHROME_BROWSER_PRINTING_PRINTED_PAGE_H__
#define CHROME_BROWSER_PRINTING_PRINTED_PAGE_H__

#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

namespace gfx {
class Emf;
}

namespace printing {

// Contains the data to reproduce a printed page, either on screen or on
// paper. Once created, this object is immuable. It has no reference to the
// PrintedDocument containing this page.
// Note: May be accessed from many threads at the same time. This is an non
// issue since this object is immuable. The reason is that a page may be printed
// and be displayed at the same time.
class PrintedPage : public base::RefCountedThreadSafe<PrintedPage> {
 public:
  PrintedPage(int page_number,
              gfx::Emf* emf,
              const gfx::Size& page_size);
  ~PrintedPage();

  // Getters
  int page_number() const { return page_number_; }
  const gfx::Emf* emf() const;
  const gfx::Size& page_size() const { return page_size_; }

 private:
  // Page number inside the printed document.
  const int page_number_;

  // Actual paint data.
  const scoped_ptr<gfx::Emf> emf_;

  // The physical page size. To support multiple page formats inside on print
  // job.
  const gfx::Size page_size_;

  DISALLOW_EVIL_CONSTRUCTORS(PrintedPage);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTED_PAGE_H__
