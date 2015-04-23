// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/mock_printer.h"

#include "base/file_util.h"
#include "base/gfx/png_encoder.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/shared_memory.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/common/render_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

MockPrinter::MockPrinter()
  : printable_width_(0),
    printable_height_(0),
    dpi_(72),
    max_shrink_(2.0),
    min_shrink_(1.25),
    desired_dpi_(72),
    selection_only_(false),
    document_cookie_(-1),
    current_document_cookie_(0),
    printer_status_(PRINTER_READY),
    number_pages_(0),
    page_number_(0) {
  printable_width_ = static_cast<int>(dpi_ * 8.5);
  printable_height_ = static_cast<int>(dpi_ * 11.0);
}

MockPrinter::~MockPrinter() {
}

void MockPrinter::ResetPrinter() {
  printer_status_ = PRINTER_READY;
  document_cookie_ = -1;
}

void MockPrinter::GetDefaultPrintSettings(ViewMsg_Print_Params* params) {
  // Verify this printer is not processing a job.
  // Sorry, this mock printer is very fragile.
  EXPECT_EQ(-1, document_cookie_);

  // Assign a unit document cookie and set the print settings.
  document_cookie_ = CreateDocumentCookie();
  memset(params, 0, sizeof(ViewMsg_Print_Params));
  params->dpi = dpi_;
  params->max_shrink = max_shrink_;
  params->min_shrink = min_shrink_;
  params->desired_dpi = desired_dpi_;
  params->selection_only = selection_only_;
  params->document_cookie = document_cookie_;
  params->printable_size.set_width(printable_width_);
  params->printable_size.set_height(printable_height_);
}

void MockPrinter::SetDefaultPrintSettings(const ViewMsg_Print_Params& params) {
  dpi_ = params.dpi;
  max_shrink_ = params.max_shrink;
  min_shrink_ = params.min_shrink;
  desired_dpi_ = params.desired_dpi;
  selection_only_ = params.selection_only;
  printable_width_ = params.printable_size.width();
  printable_height_ = params.printable_size.height();
}

void MockPrinter::ScriptedPrint(int cookie,
                                int expected_pages_count,
                                bool has_selection,
                                ViewMsg_PrintPages_Params* settings) {
  // Verify the input parameters.
  EXPECT_EQ(document_cookie_, cookie);

  memset(settings, 0, sizeof(ViewMsg_PrintPages_Params));
  settings->params.dpi = dpi_;
  settings->params.max_shrink = max_shrink_;
  settings->params.min_shrink = min_shrink_;
  settings->params.desired_dpi = desired_dpi_;
  settings->params.selection_only = selection_only_;
  settings->params.document_cookie = document_cookie_;
  settings->params.printable_size.set_width(printable_width_);
  settings->params.printable_size.set_height(printable_height_);
  printer_status_ = PRINTER_PRINTING;
}

void MockPrinter::SetPrintedPagesCount(int cookie, int number_pages) {
  // Verify the input parameter and update the printer status so that the
  // RenderViewTest class can verify the this function finishes without errors.
  EXPECT_EQ(document_cookie_, cookie);
  EXPECT_EQ(PRINTER_PRINTING, printer_status_);
  EXPECT_EQ(0, number_pages_);
  EXPECT_EQ(0, page_number_);

  // Initialize the job status.
  number_pages_ = number_pages;
  page_number_ = 0;
  pages_.clear();
}

void MockPrinter::PrintPage(const ViewHostMsg_DidPrintPage_Params& params) {
  // Verify the input parameter and update the printer status so that the
  // RenderViewTest class can verify the this function finishes without errors.
  EXPECT_EQ(PRINTER_PRINTING, printer_status_);
  EXPECT_EQ(document_cookie_, params.document_cookie);
  EXPECT_EQ(page_number_, params.page_number);
  EXPECT_LE(params.page_number, number_pages_);

#if defined(OS_WIN)
  // Load the EMF data sent from a RenderView object and create a PageData
  // object.
  // We duplicate the given file handle when creating a base::SharedMemory
  // instance so that its destructor closes the copy.
  EXPECT_GT(params.data_size, 0U);
  base::SharedMemory emf_data(params.metafile_data_handle, true,
                              GetCurrentProcess());
  emf_data.Map(params.data_size);
  MockPrinterPage* page_data = driver_.LoadSource(emf_data.memory(),
                                                  params.data_size);
  if (!page_data) {
    printer_status_ = PRINTER_ERROR;
    return;
  }

  scoped_refptr<MockPrinterPage> page(page_data);
  pages_.push_back(page);
#endif

  // We finish printing a printing job.
  // Reset the job status and the printer status.
  ++page_number_;
  if (number_pages_ == page_number_)
    ResetPrinter();
}

int MockPrinter::GetPrintedPages() const {
  if (printer_status_ != PRINTER_READY)
    return -1;
  return page_number_;
}

int MockPrinter::GetWidth(size_t page) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return -1;
  return pages_[page]->width();
}

int MockPrinter::GetHeight(size_t page) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return -1;
  return pages_[page]->height();
}

bool MockPrinter::GetSourceChecksum(size_t page, std::string* checksum) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return false;
  return GetChecksum(pages_[page]->source_data(), pages_[page]->source_size(),
                     checksum);
}

bool MockPrinter::GetBitmapChecksum(size_t page, std::string* checksum) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return false;
  return GetChecksum(pages_[page]->bitmap_data(), pages_[page]->bitmap_size(),
                     checksum);
}

bool MockPrinter::GetBitmap(size_t page,
                            const void** data,
                            size_t* size) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return false;
  *data = pages_[page]->bitmap_data();
  *size = pages_[page]->bitmap_size();
  return true;
}

bool MockPrinter::SaveSource(size_t page,
                             const std::wstring& filename) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return false;
  const uint8* source_data = pages_[page]->source_data();
  size_t source_size = pages_[page]->source_size();
  file_util::WriteFile(filename, reinterpret_cast<const char*>(source_data),
                       source_size);
  return true;
}

bool MockPrinter::SaveBitmap(size_t page,
                             const std::wstring& filename) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return false;
  int width = pages_[page]->width();
  int height = pages_[page]->height();
  int row_byte_width = width * 4;
  const uint8* bitmap_data = pages_[page]->bitmap_data();
  std::vector<unsigned char> compressed;
  PNGEncoder::Encode(bitmap_data,
                     PNGEncoder::FORMAT_BGRA, width, height,
                     row_byte_width, true, &compressed);
  file_util::WriteFile(filename,
                       reinterpret_cast<char*>(&*compressed.begin()),
                       compressed.size());
  return true;
}

int MockPrinter::CreateDocumentCookie() {
  return ++current_document_cookie_;
}

bool MockPrinter::GetChecksum(const void* data,
                              size_t size,
                              std::string* checksum) const {
  MD5Digest digest;
  MD5Sum(data, size, &digest);
  checksum->assign(HexEncode(&digest, sizeof(digest)));
  return true;
}
