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

#include "base/string_util.h"
#include "chrome/browser/printing/page_overlays.h"
#include "chrome/browser/printing/print_settings.h"
#include "chrome/browser/printing/printed_document.h"
#include "chrome/browser/printing/printed_page.h"
#include "chrome/browser/printing/printed_pages_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct Keys {
  const wchar_t* key;
  const wchar_t* expected;
};

const Keys kOverlayKeys[] = {
  printing::PageOverlays::kTitle, L"Foobar Document",
  printing::PageOverlays::kTime, L"",
  printing::PageOverlays::kDate, L"",
  printing::PageOverlays::kPage, L"1",
  printing::PageOverlays::kPageCount, L"2",
  printing::PageOverlays::kPageOnTotal, L"1/2",
  printing::PageOverlays::kUrl, L"http://www.perdu.com/",
};

class PagesSource : public printing::PrintedPagesSource {
 public:
  virtual void RenderOnePrintedPage(printing::PrintedDocument* document,
                                    int page_number) {
    NOTREACHED();
  }

  virtual std::wstring RenderSourceName() {
    return L"Foobar Document";
  }

  virtual GURL RenderSourceUrl() {
    return GURL(L"http://www.perdu.com");
  }
};

}  // namespace


TEST(PageOverlaysTest, StringConversion) {
  printing::PageOverlays overlays;
  overlays.GetOverlay(printing::PageOverlays::LEFT,
                      printing::PageOverlays::BOTTOM);
  printing::PrintSettings settings;
  PagesSource source;
  int cookie = 1;
  scoped_refptr<printing::PrintedDocument> doc(
      new printing::PrintedDocument(settings, &source, cookie));
  doc->set_page_count(2);
  gfx::Size page_size(100, 100);
  scoped_refptr<printing::PrintedPage> page(
      new printing::PrintedPage(1, NULL, page_size));

  std::wstring input;
  std::wstring out;
  for (int i = 0; i < arraysize(kOverlayKeys); ++i) {
    input = StringPrintf(L"foo%sbar", kOverlayKeys[i].key);
    out = printing::PageOverlays::ReplaceVariables(input, *doc.get(),
                                                   *page.get());
    EXPECT_FALSE(out.empty());
    if (wcslen(kOverlayKeys[i].expected) == 0)
      continue;
    EXPECT_EQ(StringPrintf(L"foo%lsbar", kOverlayKeys[i].expected), out) <<
        kOverlayKeys[i].key;
  }
}
