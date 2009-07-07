// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "printing/page_overlays.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "printing/printed_page.h"
#include "printing/printed_pages_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::AtExitManager global_at_exit_manager;

class PageOverlaysTest : public testing::Test {
 private:
  MessageLoop message_loop_;
};

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
  virtual std::wstring RenderSourceName() {
    return L"Foobar Document";
  }

  virtual GURL RenderSourceUrl() {
    return GURL(L"http://www.perdu.com");
  }
};

}  // namespace


TEST_F(PageOverlaysTest, StringConversion) {
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
    input = StringPrintf(L"foo%lsbar", kOverlayKeys[i].key);
    out = printing::PageOverlays::ReplaceVariables(input, *doc.get(),
                                                   *page.get());
    EXPECT_FALSE(out.empty());
    if (wcslen(kOverlayKeys[i].expected) == 0)
      continue;
    EXPECT_EQ(StringPrintf(L"foo%lsbar", kOverlayKeys[i].expected), out) <<
        kOverlayKeys[i].key;
  }

  // Check if SetOverlay really sets the page overlay.
  overlays.SetOverlay(printing::PageOverlays::LEFT,
                      printing::PageOverlays::TOP,
                      UTF16ToWide(L"Page {page}"));
  input = overlays.GetOverlay(printing::PageOverlays::LEFT,
                              printing::PageOverlays::TOP);
  EXPECT_EQ(input, L"Page {page}");

  // Replace the variables to see if the page number is correct.
  out = printing::PageOverlays::ReplaceVariables(input, *doc.get(),
                                                 *page.get());
  EXPECT_EQ(out, L"Page 1");
}
