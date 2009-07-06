// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printed_document.h"

#include <set>

#include "app/gfx/font.h"
#include "app/gfx/text_elider.h"
#include "app/win_util.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/printing/page_number.h"
#include "chrome/browser/printing/page_overlays.h"
#include "chrome/browser/printing/printed_pages_source.h"
#include "chrome/browser/printing/printed_page.h"
#include "printing/units.h"
#include "skia/ext/platform_device.h"

using base::Time;

namespace {

struct PrintDebugDumpPath {
  PrintDebugDumpPath()
    : enabled(false) {
  }

  bool enabled;
  std::wstring debug_dump_path;
};

Singleton<PrintDebugDumpPath> g_debug_dump_info;

}  // namespace

namespace printing {

PrintedDocument::PrintedDocument(const PrintSettings& settings,
                                 PrintedPagesSource* source,
                                 int cookie)
    : mutable_(source),
      immutable_(settings, source, cookie) {

  // Records the expected page count if a range is setup.
  if (!settings.ranges.empty()) {
    // If there is a range, set the number of page
    for (unsigned i = 0; i < settings.ranges.size(); ++i) {
      const PageRange& range = settings.ranges[i];
      mutable_.expected_page_count_ += range.to - range.from + 1;
    }
  }
}

PrintedDocument::~PrintedDocument() {
}

void PrintedDocument::SetPage(int page_number,
                              NativeMetafile* metafile,
                              double shrink) {
  // Notice the page_number + 1, the reason is that this is the value that will
  // be shown. Users dislike 0-based counting.
  scoped_refptr<PrintedPage> page(
      new PrintedPage(page_number + 1,
          metafile, immutable_.settings_.page_setup_pixels().physical_size()));
  {
    AutoLock lock(lock_);
    mutable_.pages_[page_number] = page;
    if (mutable_.shrink_factor == 0) {
      mutable_.shrink_factor = shrink;
    } else {
      DCHECK_EQ(mutable_.shrink_factor, shrink);
    }
  }
  DebugDump(*page);
}

bool PrintedDocument::GetPage(int page_number,
                              scoped_refptr<PrintedPage>* page) {
  AutoLock lock(lock_);
  PrintedPages::const_iterator itr = mutable_.pages_.find(page_number);
  if (itr != mutable_.pages_.end()) {
    if (itr->second.get()) {
      *page = itr->second;
      return true;
    }
  }
  return false;
}

void PrintedDocument::RenderPrintedPage(const PrintedPage& page,
                                        HDC context) const {
#ifndef NDEBUG
  {
    // Make sure the page is from our list.
    AutoLock lock(lock_);
    DCHECK(&page == mutable_.pages_.find(page.page_number() - 1)->second.get());
  }
#endif

  // Save the state to make sure the context this function call does not modify
  // the device context.
  int saved_state = SaveDC(context);
  DCHECK_NE(saved_state, 0);
  skia::PlatformDevice::InitializeDC(context);
  {
    // Save the state (again) to apply the necessary world transformation.
    int saved_state = SaveDC(context);
    DCHECK_NE(saved_state, 0);

    // Setup the matrix to translate and scale to the right place. Take in
    // account the actual shrinking factor.
    XFORM xform = { 0 };
    xform.eDx = static_cast<float>(
        immutable_.settings_.page_setup_pixels().content_area().x());
    xform.eDy = static_cast<float>(
        immutable_.settings_.page_setup_pixels().content_area().y());
    xform.eM11 = static_cast<float>(1. / mutable_.shrink_factor);
    xform.eM22 = static_cast<float>(1. / mutable_.shrink_factor);
    BOOL res = ModifyWorldTransform(context, &xform, MWT_LEFTMULTIPLY);
    DCHECK_NE(res, 0);

    if (!page.native_metafile()->SafePlayback(context)) {
      NOTREACHED();
    }

    res = RestoreDC(context, saved_state);
    DCHECK_NE(res, 0);
  }

  // Print the header and footer.
  int base_font_size = gfx::Font().height();
  int new_font_size = ConvertUnit(10,
                                  immutable_.settings_.desired_dpi,
                                  immutable_.settings_.dpi());
  DCHECK_GT(new_font_size, base_font_size);
  gfx::Font font(gfx::Font().DeriveFont(new_font_size - base_font_size));
  HGDIOBJ old_font = SelectObject(context, font.hfont());
  DCHECK(old_font != NULL);
  // We don't want a white square around the text ever if overflowing.
  SetBkMode(context, TRANSPARENT);
  PrintHeaderFooter(context, page, PageOverlays::LEFT, PageOverlays::TOP,
                    font);
  PrintHeaderFooter(context, page, PageOverlays::CENTER, PageOverlays::TOP,
                    font);
  PrintHeaderFooter(context, page, PageOverlays::RIGHT, PageOverlays::TOP,
                    font);
  PrintHeaderFooter(context, page, PageOverlays::LEFT, PageOverlays::BOTTOM,
                    font);
  PrintHeaderFooter(context, page, PageOverlays::CENTER, PageOverlays::BOTTOM,
                    font);
  PrintHeaderFooter(context, page, PageOverlays::RIGHT, PageOverlays::BOTTOM,
                    font);
  int res = RestoreDC(context, saved_state);
  DCHECK_NE(res, 0);
}

bool PrintedDocument::RenderPrintedPageNumber(int page_number, HDC context) {
  scoped_refptr<PrintedPage> page;
  if (!GetPage(page_number, &page))
    return false;
  RenderPrintedPage(*page.get(), context);
  return true;
}

bool PrintedDocument::IsComplete() const {
  AutoLock lock(lock_);
  if (!mutable_.page_count_)
    return false;
  PageNumber page(immutable_.settings_, mutable_.page_count_);
  if (page == PageNumber::npos())
    return false;
  for (; page != PageNumber::npos(); ++page) {
    PrintedPages::const_iterator itr = mutable_.pages_.find(page.ToInt());
    if (itr == mutable_.pages_.end() || !itr->second.get() ||
        !itr->second->native_metafile())
      return false;
  }
  return true;
}

void PrintedDocument::DisconnectSource() {
  AutoLock lock(lock_);
  mutable_.source_ = NULL;
}

size_t PrintedDocument::MemoryUsage() const {
  std::vector<scoped_refptr<PrintedPage>> pages_copy;
  {
    AutoLock lock(lock_);
    pages_copy.reserve(mutable_.pages_.size());
    PrintedPages::const_iterator end = mutable_.pages_.end();
    for (PrintedPages::const_iterator itr = mutable_.pages_.begin();
         itr != end; ++itr) {
      if (itr->second.get()) {
        pages_copy.push_back(itr->second);
      }
    }
  }
  size_t total = 0;
  for (size_t i = 0; i < pages_copy.size(); ++i) {
    total += pages_copy[i]->native_metafile()->GetDataSize();
  }
  return total;
}

void PrintedDocument::set_page_count(int max_page) {
  AutoLock lock(lock_);
  DCHECK_EQ(0, mutable_.page_count_);
  mutable_.page_count_ = max_page;
  if (immutable_.settings_.ranges.empty()) {
    mutable_.expected_page_count_ = max_page;
  } else {
    // If there is a range, don't bother since expected_page_count_ is already
    // initialized.
    DCHECK_NE(mutable_.expected_page_count_, 0);
  }
}

int PrintedDocument::page_count() const {
  AutoLock lock(lock_);
  return mutable_.page_count_;
}

int PrintedDocument::expected_page_count() const {
  AutoLock lock(lock_);
  return mutable_.expected_page_count_;
}

void PrintedDocument::PrintHeaderFooter(HDC context,
                                        const PrintedPage& page,
                                        PageOverlays::HorizontalPosition x,
                                        PageOverlays::VerticalPosition y,
                                        const gfx::Font& font) const {
  const PrintSettings& settings = immutable_.settings_;
  const std::wstring& line = settings.overlays.GetOverlay(x, y);
  if (line.empty()) {
    return;
  }
  std::wstring output(PageOverlays::ReplaceVariables(line, *this, page));
  if (output.empty()) {
    // May happens if document name or url is empty.
    return;
  }
  const gfx::Size string_size(font.GetStringWidth(output), font.height());
  gfx::Rect bounding;
  bounding.set_height(string_size.height());
  const gfx::Rect& overlay_area(settings.page_setup_pixels().overlay_area());
  // Hard code .25 cm interstice between overlays. Make sure that some space is
  // kept between each headers.
  const int interstice = ConvertUnit(250, kHundrethsMMPerInch, settings.dpi());
  const int max_width = overlay_area.width() / 3 - interstice;
  const int actual_width = std::min(string_size.width(), max_width);
  switch (x) {
    case PageOverlays::LEFT:
      bounding.set_x(overlay_area.x());
      bounding.set_width(max_width);
      break;
    case PageOverlays::CENTER:
      bounding.set_x(overlay_area.x() +
                     (overlay_area.width() - actual_width) / 2);
      bounding.set_width(actual_width);
      break;
    case PageOverlays::RIGHT:
      bounding.set_x(overlay_area.right() - actual_width);
      bounding.set_width(actual_width);
      break;
  }

  DCHECK_LE(bounding.right(), overlay_area.right());

  switch (y) {
    case PageOverlays::BOTTOM:
      bounding.set_y(overlay_area.bottom() - string_size.height());
      break;
    case PageOverlays::TOP:
      bounding.set_y(overlay_area.y());
      break;
  }

  if (string_size.width() > bounding.width()) {
    if (line == PageOverlays::kUrl) {
      output = gfx::ElideUrl(url(), font, bounding.width(), std::wstring());
    } else {
      output = gfx::ElideText(output, font, bounding.width());
    }
  }

  // Save the state (again) for the clipping region.
  int saved_state = SaveDC(context);
  DCHECK_NE(saved_state, 0);

  int result = IntersectClipRect(context, bounding.x(), bounding.y(),
                                 bounding.right() + 1, bounding.bottom() + 1);
  DCHECK(result == SIMPLEREGION || result == COMPLEXREGION);
  TextOut(context,
          bounding.x(), bounding.y(),
          output.c_str(),
          static_cast<int>(output.size()));
  int res = RestoreDC(context, saved_state);
  DCHECK_NE(res, 0);
}

void PrintedDocument::DebugDump(const PrintedPage& page)
{
  if (!g_debug_dump_info->enabled)
    return;

  std::wstring filename;
  filename += date();
  filename += L"_";
  filename += time();
  filename += L"_";
  filename += name();
  filename += L"_";
  filename += StringPrintf(L"%02d", page.page_number());
  filename += L"_.emf";
  file_util::ReplaceIllegalCharacters(&filename, '_');
  std::wstring path(g_debug_dump_info->debug_dump_path);
  file_util::AppendToPath(&path, filename);
  page.native_metafile()->SaveTo(path);
}

void PrintedDocument::set_debug_dump_path(const std::wstring& debug_dump_path) {
  g_debug_dump_info->enabled = !debug_dump_path.empty();
  g_debug_dump_info->debug_dump_path = debug_dump_path;
}

const std::wstring& PrintedDocument::debug_dump_path() {
  return g_debug_dump_info->debug_dump_path;
}

PrintedDocument::Mutable::Mutable(PrintedPagesSource* source)
    : source_(source),
      expected_page_count_(0),
      page_count_(0),
      shrink_factor(0) {
}

PrintedDocument::Immutable::Immutable(const PrintSettings& settings,
                                      PrintedPagesSource* source,
                                      int cookie)
    : settings_(settings),
      source_message_loop_(MessageLoop::current()),
      name_(source->RenderSourceName()),
      url_(source->RenderSourceUrl()),
      cookie_(cookie) {
  // Setup the document's date.
#ifdef WIN32
  // On Windows, use the native time formatting for printing.
  SYSTEMTIME systemtime;
  GetLocalTime(&systemtime);
  date_ = win_util::FormatSystemDate(systemtime, std::wstring());
  time_ = win_util::FormatSystemTime(systemtime, std::wstring());
#else
  Time now = Time::Now();
  date_ = TimeFormat::ShortDateNumeric(now);
  time_ = TimeFormat::TimeOfDay(now);
#endif  // WIN32
}

}  // namespace printing
